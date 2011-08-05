/**
 * \file configuration.h
 * \author Petr Velan <petr.velan@cesnet.cz>
 * \brief Header of class for managing user input of ipfixdump
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

#ifndef CONFIGURATION_H_
#define CONFIGURATION_H_

#include <cstdio>
#include <vector>
#include <set>
#include <getopt.h>
#include <iostream>
#include <cstdlib>
#include <cstring>
#include <stdint.h>
#include "dirent.h"


/** Acceptable command-line parameters */
#define OPTSTRING "hVaAr:f:n:c:D:N:s:qIM:mR:o:v:Z:t:"

/** ipfixdump version */
#define VERSION "0.1"

/**
 * \brief Namespace of the ipfixdump utility
 */
namespace ipfixdump {

typedef std::vector<std::string> stringVector;

typedef std::set<std::string> stringSet;

/**
 * \brief Class managing program configuration
 *
 * Parses commandline arguments and sets corresponding properties
 */
class configuration {
private:
	char *progname; /**< Name of the program parsed from argv[0]*/

public:
	stringVector tables; /**< fastbit tables to be used*/
	std::vector<stringVector*> parts; /**< table templates to be used*/
	std::string filter;
	std::string select;
	stringVector order;
	std::string format;
	uint64_t maxRecords;

	/**
	 * \brief Configuration class destructor
	 */
	~configuration();

	/**
	 * \brief Configuration class constructor
	 */
	configuration();

	/**
	 * \brief Configuration class initializator
	 *
	 * @param argc int number of arguments passed to program
	 * @param argv char** array of strings passed to program
	 * @return 0 if all is set to continue, 1 when exiting without error,
	 * negative value otherwise
	 */
	int init(int argc, char *argv[]);

	/**
	 * \brief Prints program help
	 *
	 * Currently this is copied from nfdump and some options have been ommited
	 */
	void help();

	/**
	 * \brief Returns current program version
	 * @return version string
	 */
	const char* version();
};

}  // namespace ipfixdump


#endif /* CONFIGURATION_H_ */
