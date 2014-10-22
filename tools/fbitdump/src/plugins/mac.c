/**
 * \file mac.c
 * \author Michal Kozubik <kozubik@cesnet.cz>
 * \brief fbitdump plugin for parsing/formating MAC address
 * 
 * Copyright (C) 2014 CESNET, z.s.p.o.
 */

#include <stdio.h>
#include <string.h>
#include "plugin_header.h"

char *info()
{
	return \
"Converts MAC address (six groups of two hex digits separated by colons) to number and vice versa.";
}


void format(const union plugin_arg * arg, int plain_numbers, char buff[PLUGIN_BUFFER_SIZE])
{
        if (plain_numbers) {
		snprintf(buff, PLUGIN_BUFFER_SIZE, "%lu", arg->uint64);
                return;
        }
	
        snprintf(buff, PLUGIN_BUFFER_SIZE, "%02x:%02x:%02x:%02x:%02x:%02x",
                ((unsigned char *) &(arg->uint64))[5],
                ((unsigned char *) &(arg->uint64))[4],
                ((unsigned char *) &(arg->uint64))[3],
                ((unsigned char *) &(arg->uint64))[2],
                ((unsigned char *) &(arg->uint64))[1],
                ((unsigned char *) &(arg->uint64))[0]);
}


void parse(char *input, char out[PLUGIN_BUFFER_SIZE])
{
	uint64_t addr = 0;
	sscanf(input, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", 
		&((unsigned char *) &addr)[5],
		&((unsigned char *) &addr)[4],
		&((unsigned char *) &addr)[3],
		&((unsigned char *) &addr)[2],
		&((unsigned char *) &addr)[1],
		&((unsigned char *) &addr)[0]);
	
	snprintf(out, PLUGIN_BUFFER_SIZE, "%llu", addr);
}

