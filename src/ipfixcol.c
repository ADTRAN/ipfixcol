/**
 * \file ipfixcol.c
 * \author Radek Krejci <rkrejci@cesnet.cz>
 * \brief Main body of the ipfixcol
 *
 * Copyright (C) 2011 CESNET, z.s.p.o.
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

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <stdbool.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <commlbr.h>
#include <signal.h>
#include <syslog.h>
#include <sys/wait.h>
#include <pthread.h>

#include <libxml/parser.h>
#include <libxml/xpath.h>

#include "../ipfixcol.h"

#include "config.h"
#include "preprocessor.h"

/**
 * \defgroup internalAPIs ipfixcol's Internal APIs
 *
 * Internal ipfixcol's functions not supposed to be used outside the collector
 * core.
 *
 */


/** Version number */
#define VERSION "0.1"

/** Acceptable command-line parameters */
#define OPTSTRING "c:dhv:V"

/* verbose from libcommlbr */
extern int verbose;

/* main loop indicator */
volatile int done = 0;

/**
 * \brief Print program version information
 * @param progname Name of the program
 */
void version (char* progname)
{
	printf ("%s: IPFIX Collector capture daemon.\n", progname);
	printf ("Version: %s, Copyright (C) 2011 CESNET z.s.p.o.\n", VERSION);
	printf ("See http://www.liberouter.org/ipfixcol/ for more information.\n\n");
}

/**
 *
 * @param progname
 */
void help (char* progname)
{
	printf ("Usage: %s [-dhV] [-v level]\n", progname);
	printf ("  -c file   Path to configuration file (%s by default)\n", DEFAULT_CONFIG_FILE);
	printf ("  -d        Daemonize\n");
	printf ("  -h        Print this help\n");
	printf ("  -v level  Print verbose messages up to specified level\n");
	printf ("  -V        Print version information\n\n");
}

void term_signal_handler(int sig)
{
	if (done) {
		VERBOSE(CL_VERBOSE_OFF, "Another termination signal (%i) detected - quiting without cleanup.", sig);
		exit (EXIT_FAILURE);
	} else {
		VERBOSE(CL_VERBOSE_OFF, "Signal: %i detected, will exit as soon as possible", sig);
		done = 1;
	}
}

int main (int argc, char* argv[])
{
	int c, i, fd, retval = 0, get_retval, proc_count = 0, proc_id = 0;
	pid_t pid = 0;
	bool daemonize = false;
	char *progname, *config_file = NULL;
	struct plugin_xml_conf_list* input_plugins = NULL, *storage_plugins = NULL,
	        *aux_plugins = NULL;
	struct input input;
	struct storage_list *storage_list = NULL, *aux_storage_list = NULL;
	void *input_plugin_handler = NULL, *storage_plugin_handler = NULL;
	struct sigaction action;
	char *packet = NULL;
	struct input_info* input_info;

	xmlXPathObjectPtr collectors;
	xmlNodePtr collector_node;
	xmlDocPtr xml_config;
	xmlChar *plugin_params;

	/* some initialization */
	input.dll_handler = NULL;
	input.config = NULL;

	/* get program name withou execute path */
	progname = ((progname = strrchr (argv[0], '/')) != NULL) ? (progname + 1) : argv[0];

	/* parse command line parameters */
	while ((c = getopt (argc, argv, OPTSTRING)) != -1) {
		switch (c) {
		case 'c':
			config_file = optarg;
			break;
		case 'd':
			daemonize = true;
			break;
		case 'h':
			version (progname);
			help (progname);
			exit (EXIT_SUCCESS);
			break;
		case 'v':
			verbose = atoi (optarg);
			break;
		case 'V':
			version (progname);
			exit (EXIT_SUCCESS);
			break;
		default:
			VERBOSE(CL_VERBOSE_OFF, "Unknown parameter %c", c);
			help (progname);
			exit (EXIT_FAILURE);
			break;
		}
	}

	/* prepare signal handling */
	/* establish the signal handler */
	sigemptyset(&action.sa_mask);
	action.sa_flags = 0;
	action.sa_handler = term_signal_handler;
	sigaction(SIGINT, &action, NULL);
	sigaction(SIGQUIT, &action, NULL);
	sigaction(SIGTERM, &action, NULL);

	/* daemonize */
	if (daemonize) {
		debug_init(progname, 1);
		closelog();
		openlog(progname, LOG_PID, 0);
		/* and send all following messages to the syslog */
		if (daemon (1, 0)) {
			VERBOSE(CL_VERBOSE_OFF, "%s", strerror(errno));
		}
	}

	/*
	 * this initialize the library and check potential ABI mismatches
	 * between the version it was compiled for and the actual shared
	 * library used.
	 */LIBXML_TEST_VERSION
	xmlIndentTreeOutput = 1;

	/*
	 * open and prepare XML configuration file
	 */
	/* check config file */
	if (config_file == NULL) {
		/* and use default if not specified */
		config_file = DEFAULT_CONFIG_FILE;
		VERBOSE(CL_VERBOSE_BASIC, "Using default configuration file %s.", config_file);
	}

	/* TODO: this part should be in the future replaced by NETCONF configuration */
	fd = open (config_file, O_RDONLY);
	if (fd == -1) {
		VERBOSE(CL_VERBOSE_OFF, "Unable to open configuration file %s (%s)", config_file, strerror(errno));
		exit (EXIT_FAILURE);
	}
	xml_config = xmlReadFd (fd, NULL, NULL, XML_PARSE_NOERROR | XML_PARSE_NOWARNING | XML_PARSE_NOBLANKS);
	if (xml_config == NULL) {
		VERBOSE(CL_VERBOSE_OFF, "Unable to parse configuration file %s", config_file);
		close (fd);
		exit (EXIT_FAILURE);
	}
	close (fd);

	/* get collectors' specification from the configuration file */
	collectors = get_collectors (xml_config);
	if (collectors == NULL) {
		/* no collectingProcess configured */
		VERBOSE(CL_VERBOSE_OFF, "No collectingProcess configured - nothing to do.");
		retval = EXIT_FAILURE;
		goto cleanup;
	}

	/* create separate process for each <collectingProcess */
	for (i = (collectors->nodesetval->nodeNr - 1); i >= 0; i--) {

		/*
		 * fork for multiple collectors - original parent process handle only
		 * collector 0
		 */
		if (i > 0) {
			pid = fork();
			if (pid > 0) { /* parent process waits for collector 0 */
                proc_count++;
				continue;
			} else if (pid < 0) { /* error occured, fork failed */
				VERBOSE(CL_VERBOSE_OFF, "Forking collector process failed (%s), skipping collector %d.", strerror(errno), i);
				continue;
			}
			/* else child - just continue to handle plugins */
            proc_id = i;
			VERBOSE(CL_VERBOSE_BASIC, "[%d] New collector process started.", proc_id);
		}
		collector_node = collectors->nodesetval->nodeTab[i];
		break;
	}

	/*
	 * initialize plugins for the collector
	 */
	/* get input plugin - one */
	input_plugins = get_input_plugins (collector_node);
	if (input_plugins == NULL) {
		retval = EXIT_FAILURE;
		goto cleanup;
	}

	/* get storage plugins - at least one */
	storage_plugins = get_storage_plugins (collector_node, xml_config);
	if (storage_plugins == NULL) {
		retval = EXIT_FAILURE;
		goto cleanup;
	}

	/* prepare input plugin */
	for (aux_plugins = input_plugins; aux_plugins != NULL; aux_plugins = aux_plugins->next) {
		input.xml_conf = &aux_plugins->config;
		VERBOSE(CL_VERBOSE_ADVANCED, "[%d] Opening input plugin: %s", proc_id, aux_plugins->config.file);
		input_plugin_handler = dlopen (input_plugins->config.file, RTLD_LAZY);
		if (input_plugin_handler == NULL) {
			VERBOSE(CL_VERBOSE_OFF, "[%d] Unable to load input xml_conf (%s)", proc_id, dlerror());
			continue;
		}
		input.dll_handler = input_plugin_handler;

		/* prepare Input API routines */
		input.init = dlsym (input_plugin_handler, "input_init");
		if (input.init == NULL) {
			VERBOSE(CL_VERBOSE_OFF, "[%d] Unable to load input xml_conf (%s)", proc_id, dlerror());
			dlclose(input.dll_handler);
			continue;
		}
		input.get = dlsym (input_plugin_handler, "get_packet");
		if (input.get == NULL) {
			VERBOSE(CL_VERBOSE_OFF, "[%d] Unable to load input xml_conf (%s)", proc_id, dlerror());
			dlclose(input.dll_handler);
			continue;
		}
		input.close = dlsym (input_plugin_handler, "input_close");
		if (input.close == NULL) {
			VERBOSE(CL_VERBOSE_OFF, "[%d] Unable to load input xml_conf (%s)", proc_id, dlerror());
			dlclose(input.dll_handler);
			continue;
		}
        /* get the first one we can */
        break;
	}
	/* check if we have found any input xml_conf */
	if (!input.dll_handler || !input.init || !input.get || !input.close) {
		VERBOSE(CL_VERBOSE_OFF, "[%d] Loading input xml_conf failed.", proc_id);
		retval = EXIT_FAILURE;
		goto cleanup;
	}

	/* prepare storage xml_conf(s) */
	//while (storage_plugins) {
	for (aux_plugins = storage_plugins; aux_plugins != NULL; aux_plugins = aux_plugins->next) {
		//aux_plugins = storage_plugins;
		VERBOSE(CL_VERBOSE_ADVANCED, "[%d] Opening storage xml_conf: %s", proc_id, storage_plugins->config.file);

		storage_plugin_handler = dlopen (aux_plugins->config.file, RTLD_LAZY);
		if (storage_plugin_handler == NULL) {
			VERBOSE(CL_VERBOSE_OFF, "[%d] Unable to load storage xml_conf (%s)", proc_id, dlerror());
			continue;
		}

		aux_storage_list = storage_list;
		storage_list = (struct storage_list*) malloc (sizeof(struct storage_list));
		if (storage_list == NULL) {
			VERBOSE (CL_VERBOSE_OFF, "[%d] Memory allocation failed (%s:%d)", proc_id, __FILE__, __LINE__);
			storage_list = aux_storage_list;
			dlclose(storage_plugin_handler);
			continue;
		}
		memset(storage_list, 0, sizeof(struct storage_list));

		storage_list->storage.dll_handler = storage_plugin_handler;

		/* prepare Input API routines */
		storage_list->storage.init = dlsym (storage_plugin_handler, "storage_init");
		if (storage_list->storage.init == NULL) {
			VERBOSE(CL_VERBOSE_OFF, "[%d] Unable to load storage xml_conf (%s)", proc_id, dlerror());
			dlclose (storage_plugin_handler);
			storage_plugin_handler = NULL;
			free (storage_list);
			storage_list = aux_storage_list;
			dlclose(storage_plugin_handler);
			continue;
		}
		storage_list->storage.store = dlsym (storage_plugin_handler, "store_packet");
		if (storage_list->storage.store == NULL) {
			VERBOSE(CL_VERBOSE_OFF, "[%d] Unable to load storage xml_conf (%s)", proc_id, dlerror());
			dlclose (storage_plugin_handler);
			storage_plugin_handler = NULL;
			free (storage_list);
			storage_list = aux_storage_list;
			dlclose(storage_plugin_handler);
			continue;
		}
		storage_list->storage.store_now = dlsym (storage_plugin_handler, "store_now");
		if (storage_list->storage.store_now == NULL) {
			VERBOSE(CL_VERBOSE_OFF, "[%d] Unable to load storage xml_conf (%s)", proc_id, dlerror());
			dlclose (storage_plugin_handler);
			storage_plugin_handler = NULL;
			free (storage_list);
			storage_list = aux_storage_list;
			dlclose(storage_plugin_handler);
			continue;
		}
		storage_list->storage.close = dlsym (storage_plugin_handler, "storage_close");
		if (storage_list->storage.close == NULL) {
			VERBOSE(CL_VERBOSE_OFF, "[%d] Unable to load storage xml_conf (%s)", proc_id, dlerror());
			dlclose (storage_plugin_handler);
			storage_plugin_handler = NULL;
			free (storage_list);
			storage_list = aux_storage_list;
			dlclose(storage_plugin_handler);
			continue;
		}
		storage_list->storage.xml_conf = &aux_plugins->config;
		storage_list->next = aux_storage_list;
		continue;
	}
	/* check if we have found at least one storage plugin */
	if (!storage_list) {
		VERBOSE(CL_VERBOSE_OFF, "[%d] Loading storage xml_conf(s) failed.", proc_id);
		retval = EXIT_FAILURE;
		goto cleanup;
	}

	/*
	 * CAPTURE DATA
	 */

	/* init input xml_conf */
	xmlDocDumpMemory (input.xml_conf->xmldata, &plugin_params, NULL);
	retval = input.init ((char*) plugin_params, &(input.config));
	xmlFree (plugin_params);
	if (retval != 0) {
		VERBOSE(CL_VERBOSE_OFF, "[%d] Initiating input xml_conf failed.", proc_id);
		goto cleanup;
	}

	/* main loop */
	while (!done) {
		/* get data to process */
		if ((get_retval = input.get (input.config, &input_info, &packet)) < 0) {
			if (!done || get_retval != INPUT_INTR) { /* if interrupted and closing, it's ok */
				VERBOSE(CL_VERBOSE_OFF, "[%d] Getting IPFIX data failed!", proc_id);
			}
			continue;
		} else if (get_retval == INPUT_CLOSED) {
            /* ensure that parser gets NULL packet => closed connection */
            if (packet != NULL) {
                /* free the memory allocated by xml_conf (if any) right away */
                free(packet); 
                packet = NULL;
            }
        }
		/* distribute data to the particular Data Manager for further processing */
		preprocessor_parse_msg (packet, input_info, storage_list);
		packet = NULL;
		input_info = NULL;
	}

cleanup:
	/* xml cleanup */
	if (collectors) {
		xmlXPathFreeObject (collectors);
	}
	if (input_plugins) {
		if (input_plugins->config.file) {
			free (input_plugins->config.file);
		}
		if (input_plugins->config.xmldata) {
			xmlFreeDoc (input_plugins->config.xmldata);
		}
		free (input_plugins);
	}
	if (xml_config) {
		xmlFreeDoc (xml_config);
	}
	xmlCleanupParser ();
	while (storage_plugins) { /* while is just for sure, it should be always one */
		if (storage_plugins->config.file) {
			free (storage_plugins->config.file);
		}
		if (storage_plugins->config.xmldata) {
			xmlFreeDoc (storage_plugins->config.xmldata);
		}
		aux_plugins = storage_plugins->next;
		free (storage_plugins);
		storage_plugins = aux_plugins;
	}

	/* DLLs cleanup */
	if (input_plugin_handler) {
		if (input.config != NULL) {
			input.close (&(input.config));
		}
		dlclose (input_plugin_handler);
	}
   
    /* close preprocessor (will close all data managers) */
    preprocessor_close();

    /* free allocated resources in storages ( xml configuration closed above ) */
	while (storage_list) {
		aux_storage_list = storage_list->next;
		if (storage_list->storage.dll_handler) {
			dlclose(storage_list->storage.dll_handler);
		}
		free (storage_list);
		storage_list = aux_storage_list;
	}

    /* wait for child processes */
    if (pid > 0) {
        for (i=0; i<proc_count; i++) {
            pid = wait(NULL);
            VERBOSE(CL_VERBOSE_BASIC, "[%d] Collector child process %d terminated", proc_id, pid);
        }
        VERBOSE(CL_VERBOSE_BASIC, "[%d] Closing collector.", proc_id);
    }

	return (retval);
}
