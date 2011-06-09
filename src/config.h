/**
 * \file config.h
 * \author Radek Krejci <rkrejci@cesnet.cz>
 * \brief Functions to process ipfixcol's configuration data.
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

#ifndef CONFIG_H_
#define CONFIG_H_

#include <libxml/tree.h>

/**
 * \defgroup internalConfig Configuration Processing
 * \ingroup internalAPIs
 *
 * These functions implements processing of configuration data of the
 * collector.
 *
 * @{
 */

/** Path to internal ipfixcol's configuration file */
#define INTERNAL_CONFIG_FILE "/etc/ipfixcol/internalcfg.xml"

/** Path to default (if not specified) user configuration file */
#define DEFAULT_CONFIG_FILE "/etc/ipfixcol/startup.xml"

/**
 * \brief Item in list of ipfixcol's plugins (both input and storage).
 */
struct plugin_list {
	char* file;
	xmlNodePtr xmldata;
	struct plugin_list *next;
};


/**
 * \brief Get list of \<collectingPrecoess\>es from user configuration.
 * @param doc User XML configuration.
 * @return List of \<collectingProcess\> nodes in the form of XPath objects, NULL
 * in case of error.
 */
xmlXPathObjectPtr get_collectors(xmlDocPtr doc);

/**
 * \brief Prepare basic information needed to dynamically load input plugin
 * specified to be the input of the specified \<collectingProcess\>. This
 * information is get from the user configuration (given as parameter) and from
 * internal configuration of the ipfixcol.
 *
 * @param[in] collector_node XML node with parameters for the particular
 * collectingProcess. This is part of user configuration file.
 * @return Information about first input plugin for the specified collector,
 * NULL in case of error.
 */
struct plugin_list* get_input_plugins(xmlNodePtr collectorNode);

/**
 * \brief Prepare basic information needed to dynamically load storage plugins
 * specified to be the output plugins of the \<collectingProcess\>. This
 * information is get from the user configuration (given as parameter) and from
 * internal configuration of the ipfixcol.
 *
 * @param[in] collector_node XML node with parameters for the particular
 * collectingProcess. This is part of user configuration file.
 * @param[in] config User XML configuration.
 * @return List of information about storage plugin for the specified collector,
 * NULL in case of error.
 */
struct plugin_list* get_storage_plugins(xmlNodePtr collectorNode, xmlDocPtr config);

/**@}*/

#endif /* CONFIG_H_ */
