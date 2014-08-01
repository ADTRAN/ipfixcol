/**
 * \file fbitexpire.cpp
 * \author Michal Kozubik <kozubik@cesnet.cz>
 * \brief Main body of the fbitexpire
 *
 * Copyright (C) 2014 CESNET, z.s.p.o.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of the Company nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * ALTERNATIVELY, provided that this notice is retained in full, this
 * product may be distributed under the terms of the GNU General Public
 * License (GPL) version 2 or later, in which case the provisions
 * of the GPL apply INSTEAD OF those given above.
 *
 * This software is provided ``as is, and any express or implied
 * warranties, including, but not limited to, the implied warranties of
 * merchantability and fitness for a particular purpose are disclaimed.
 * In no event shall the company or contributors be liable for any
 * direct, indirect, incidental, special, exemplary, or consequential
 * damages (including, but not limited to, procurement of substitute
 * goods or services; loss of use, data, or profits; or business
 * interruption) however caused and on any theory of liability, whether
 * in contract, strict liability, or tort (including negligence or
 * otherwise) arising in any way out of the use of this software, even
 * if advised of the possibility of such damage.
 *
 */

#include <cstdlib>
#include <condition_variable>
#include <fstream>
#include <iostream>
#include <mutex>
#include <sstream>

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <sys/stat.h>
#include <unistd.h>

#include "Cleaner.h"
#include "config.h"
#include "fbitexpire.h"
#include "PipeListener.h"
#include "Scanner.h"
#include "verbose.h"
#include "Watcher.h"

#define OPTSTRING "rmhVDfkop:d:s:v:w:"
#define DEFAULT_PIPE "./fbitexpire_fifo"

static const char *msg_module = "fbitexpire";

using namespace fbitexpire;

void print_help()
{
	std::cout << "\nUse: " << PACKAGE_NAME << " [-rhVfok] [-p pipe] [-d depth] [-s size] [-w watermark] [-v verbose] [directory]\n\n";
	std::cout << "Options:\n";
	std::cout << "  -h           show this text\n";
	std::cout << "  -V           show tool version\n";
	std::cout << "  -r           Send daemon message to rescan folder. Daemon HAVE TO be running\n";
	std::cout << "  -f           Force rescan directories when daemon starts (ignore stat files)\n";
	std::cout << "  -p pipe      Pipe name, default is " << DEFAULT_PIPE << "\n";
	std::cout << "  -s size      Max size of all directories (in MB)\n";
	std::cout << "  -w watermark Lower limit when removing folders (in MB)\n";
	std::cout << "  -d depth     Dept of watched directories, default 1\n";
	std::cout << "  -D           Daemonize\n";
	std::cout << "  -m           Multiple sources on top level directory. See man pages for more info\n";
	std::cout << "  -k           Stop fbitexpire daemon listening on pipe specified by -p\n";
	std::cout << "  -o           Only scan dirs and remove old (if needed). Don't wait for new folders\n";
	std::cout << "  -v verbose   Verbose level\n";
	std::cout << "\n";
}

void print_version()
{
	std::cout << PACKAGE_STRING << "\n";
}

int write_to_pipe(bool pipe_exists, std::string pipe, std::string msg)
{
	std::ofstream f_pipe;

	if (!pipe_exists) {
		/* pipe does not exists */
		MSG_ERROR(msg_module, "cannot write to pipe %s", pipe.c_str());
		return 1;
	}

	f_pipe.open(pipe.c_str(), std::ios::out);
	if (!f_pipe.is_open()) {
		MSG_ERROR(msg_module, "cannot open pipe %s", pipe.c_str());
		return 1;
	}

	/* write message */
	MSG_DEBUG(msg_module, "writing %s", msg.c_str());
	
	f_pipe << msg;

	/* done */
	f_pipe.close();
	return 0;
}

int main(int argc, char *argv[])
{
	int c, depth{1};
	bool rescan{false}, force{false}, daemonize{false}, pipe_exists{false}, multiple{false};
	bool wmarkset{false}, sizeset{false}, kill_daemon{false}, only_remove{false};
	uint64_t watermark{0}, size{0};
	std::string pipe{DEFAULT_PIPE};

	while ((c = getopt(argc, argv, OPTSTRING)) != -1) {
		switch (c) {
		case 'h':
			print_help();
			return 0;
		case 'V':
			print_version();
			return 0;
		case 'r':
			rescan = true;
			break;
		case 'f':
			force = true;
			break;
		case 'p':
			pipe = std::string(optarg);
			break;
		case 's':
			sizeset = true;
			size = std::atol(optarg) * 1024 * 1024;
			break;
		case 'w':
			wmarkset = true;
			watermark = std::atol(optarg) * 1024 * 1024;
			break;
		case 'd':
			depth = std::atoi(optarg);
			break;
		case 'D':
			daemonize = true;
			break;
		case 'm':
			multiple = true;
			break;
		case 'k':
			kill_daemon = true;
			break;
		case 'o':
			only_remove = true;
			break;
		case 'v':
			MSG_SET_VERBOSE(std::atoi(optarg));
			break;
		default:
			print_help();
			return 1;
		}
	}

	openlog(0, LOG_CONS | LOG_PID, LOG_USER);
	
	if ((daemonize && rescan) || (daemonize && kill_daemon) || (daemonize && only_remove)
		|| (rescan && only_remove) || (kill_daemon && only_remove)) {
		MSG_ERROR(msg_module, "conflicting arguments");
		return 1;
	}

	if (optind >= argc && !kill_daemon) {
		MSG_ERROR(msg_module, "directory not specified");
		return 1;
	}
	
	if (!wmarkset) {
		watermark = size;
	}

	/* does pipe exist? */
	struct stat st;
	pipe_exists = (!lstat(pipe.c_str(), &st) && S_ISFIFO(st.st_mode));

	std::stringstream msg;
	if (rescan) {
		while (optind < argc) {
			msg << "r" << argv[optind++] << "\n";
		}
	}
	if (kill_daemon) {
		msg << "k\n";
	}
	
	/* Send command to rescan folder or to kill daemon */
	if (rescan || kill_daemon) {
		return write_to_pipe(pipe_exists, pipe, msg.str());
	}

	if (!sizeset) {
		MSG_ERROR(msg_module, "size not specified");
		return 1;
	}
	
	if (!pipe_exists) {
		/* Create pipe */
		if (mkfifo(pipe.c_str(), S_IRWXU | S_IRWXG | S_IRWXO)) {
			MSG_ERROR(msg_module, "%s", strerror(errno));
			return 1;
		}
	}
	
	std::string basedir{argv[optind]};
	
	basedir = Directory::correctDirName(basedir);
	
	if (basedir.empty()) {
		return 1;
	}
	
	if (daemonize) {
		closelog();
		MSG_SYSLOG_INIT(PACKAGE);
		/* and send all following messages to the syslog */
		if (daemon (1, 0)) {
			MSG_ERROR(msg_module, strerror(errno));
		}
	} 
	
	Watcher watcher;
	Cleaner cleaner;
	Scanner scanner;
	PipeListener listener(pipe);

	std::mutex mtx;
	std::unique_lock<std::mutex> lock(mtx);
	std::condition_variable cv;
	
	
	try {
		scanner.createDirTree(basedir, depth);
		watcher.run(&scanner, multiple);
		scanner.run(&cleaner, size, watermark, multiple);
		cleaner.run();
		listener.run(&watcher, &scanner, &cleaner, &cv);
	} catch (std::exception &e) {
		MSG_ERROR(msg_module, e.what());
		return 1;
	}
	
	if (only_remove) {
		/*
		 * tell PipeListener to stop other threads
		 */
		listener.killAll();
	}
	
	cv.wait(lock, [&listener]{ return listener.isDone(); });
	closelog();
}
