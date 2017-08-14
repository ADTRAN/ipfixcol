/**
 * \file profiles_check.c
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \brief Profiles validator tool
 */

/* Copyright (C) 2017 CESNET, z.s.p.o.
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

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include <ipfixcol.h>
#include <ipfixcol/profiles.h>
#include "config.h"
#include "utils/elements/collection.h"

void
print_help()
{
	printf("Validator of a profile configuration\n");
	printf("\n");
	printf("Parse a profiling file and output result of the parsing.\n");
	printf("If no error is found, the tool will just return a zero exit code.\n");
	printf("Otherwise a non-zero exit code is a returned and an error message is printed on\n");
	printf("standard output.\n");
	printf("\n");
	printf("Usage: ipfixcol-profiles-check [-e elements.xml] -p profiles.xml\n");
	printf("Parameters:\n");
	printf("  -p FILE  Read a profiling configuration from the FILE.\n");
	printf("  -e FILE  Read a set of IPFIX elements from the FILE.\n");
	printf("           This file is necessary for processing filter expressions.\n");
	printf("           If not defined, use the default ipfixcol file.\n");
	printf("  -h       Print this help message.\n");
}

int
main(int argc, char *argv[])
{
	const char *file_elements = NULL;
	const char *file_profiles = NULL;

	// Process command-line options
	int opt;
	while ((opt = getopt(argc, argv, "he:p:")) != -1) {
		switch (opt) {
		case 'h':
			print_help();
			return EXIT_SUCCESS;
		case 'e':
			file_elements = optarg;
			break;
		case 'p':
			file_profiles = optarg;
			break;
		default: // '?'
			fprintf(stdout, "For help, see \"%s -h\"\n", argv[0]);
			exit(EXIT_FAILURE);
		}
	}

	if (!file_elements) {
		// Use default path
		file_elements = DEFAULT_IPFIX_ELEMENTS;
	}

	if (!file_profiles) {
		fprintf(stdout, "File with profiles is not set!\n");
		return EXIT_FAILURE;
	}

	// Init a XML library
	LIBXML_TEST_VERSION;
	xmlInitParser();
	int exit_code = EXIT_FAILURE;

	// Load a description of IPFIX elements
	if (elem_coll_reload(file_elements) < 0) {
		// The function already printed an error message
		goto cleanup_xml;
	}

	// Try to process profiles
	void *profiles = profiles_process_xml(file_profiles);
	if (!profiles) {
		// The function already printed an error message
		goto cleanup_elems;
	}

	// Only valid expression goes here
	exit_code = EXIT_SUCCESS;
	profiles_free(profiles);

	// Cleanup
cleanup_elems:
	elem_coll_destroy();
cleanup_xml:
	xmlCleanupParser();
	return exit_code;
}
