/**
 * \file config.c
 * \author Radek Krejci <rkrejci@cesnet.cz>
 * \brief Routines for processing configuration data
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

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <commlbr.h>

#include <libxml/xpathInternals.h>

#include "../ipfixcol.h"
#include "config.h"

#define DEFAULT_STORAGE_PLUGIN "ipfix"

/**
 * \addtogroup internalConfig
 * \ingroup internalAPIs
 *
 * These functions implements processing of configuration data of the
 * collector.
 *
 * @{
 */

/**
 * \brief Get the node's children node with given name.
 * @param[in] node Search in the children nodes of this node.
 * @param[in] children_name Name of the requested children node.
 * @return Pointer to the children node with requested name. It should not be
 * freed since it is children of given node.
 */
static inline xmlNodePtr get_children (xmlNodePtr node, const xmlChar* children_name)
{
	/* check validity of parameters */
	if (!node || !children_name) {
		return (NULL);
	}

	xmlNodePtr children = node->children;

	while (children) {
		if (!xmlStrncmp (children->name, children_name, xmlStrlen (children_name)
		        + 1)) {
			return (children);
		}
		children = children->next;
	}

	return (children);
}

/**
 * \brief Get the text content of the node's children node with given children_name.
 *
 * Return value is just a pointer into the part of xmlNode so it  should not be
 * freed.
 *
 * @param[in] node Search in the children nodes of this node.
 * @param[in] children_name Name of the node which content will be returned.
 * @return Content (string) of the requested element or NULL if such element.
 * doesn't exist. It should not be freed since it is a part of given node.
 */
static inline xmlChar *get_children_content (xmlNodePtr node, xmlChar *children_name)
{
	/* check validity of parameters */
	if (!node || !children_name) {
		return (NULL);
	}

	/* init variables */
	xmlNodePtr cur = node->children;

	/* search in children nodes */
	while (cur) {
		if (!xmlStrncmp (cur->name, children_name, xmlStrlen (children_name)
		        + 1)) {
			if (cur->children && (cur->children->type == XML_TEXT_NODE)) {
				return (cur->children->content);
			}
		}
		/* go to sibling node (next children) */
		cur = cur->next;
	}
	/* nothing found */
	return (NULL);
}

/**
 * \brief Initiate internal configuration file - open, get xmlDoc and prepare
 * XPathContext. Also register namespace "urn:cesnet:params:xml:ns:yang:ipfixcol-internals"
 * with given namespace name.
 *
 * @param[in] ns_name Name for the "urn:cesnet:params:xml:ns:yang:ipfixcol-internals"
 * namespace in xpath queries.
 * @return XPath Context for the internal XML configuration. Caller will need to
 * free it including separated free of return->doc.
 */
static xmlXPathContextPtr ic_init (xmlChar* ns_name)
{
	int fd;
	xmlDocPtr doc = NULL;
	xmlXPathContextPtr ctxt = NULL;

	/* open and prepare internal XML configuration file */
	if ((fd = open (INTERNAL_CONFIG_FILE, O_RDONLY)) == -1) {
		VERBOSE(CL_VERBOSE_OFF, "Unable to open internal configuration file %s (%s)", INTERNAL_CONFIG_FILE, strerror(errno));
		return (NULL);
	}
	if ((doc = xmlReadFd (fd, NULL, NULL, XML_PARSE_NOERROR | XML_PARSE_NOWARNING | XML_PARSE_NOBLANKS)) == NULL) {
		VERBOSE(CL_VERBOSE_OFF, "Unable to parse internal configuration file %s", INTERNAL_CONFIG_FILE);
		close (fd);
		return (NULL);
	}
	close (fd);

	/* create xpath evaluation context of internal configuration file */
	if ((ctxt = xmlXPathNewContext (doc)) == NULL) {
		VERBOSE(CL_VERBOSE_OFF, "Unable to create XPath context for internal configuration (%s:%d).", __FILE__, __LINE__)
		xmlFreeDoc (doc);
		return (NULL);
	}
	/* register namespace for the context of internal configuration file */
	if (xmlXPathRegisterNs (ctxt, ns_name, BAD_CAST "urn:cesnet:params:xml:ns:yang:ipfixcol-internals") != 0) {
		VERBOSE(CL_VERBOSE_OFF, "Unable to register namespace for internal configuration file (%s:%d).", __FILE__, __LINE__);
		xmlXPathFreeContext (ctxt);
		xmlFreeDoc (doc);
		return (NULL);
	}

	return (ctxt);
}

/**
 * \brief Prepare basic information needed to dynamically load storage plugins
 * specified to be the output plugins of the collectingProcess. This information
 * is get from the user configuration (given as parameter) and from internal
 * configuration of the ipfixcol.
 *
 * @param[in] collector_node XML node with parameters for the particular
 * collectingProcess. This is part of user configuration file.
 * @param[in] config User XML configuration.
 * @return List of information about storage plugin for the specified collector,
 * NULL in case of error.
 */
struct plugin_xml_conf_list* get_storage_plugins (xmlNodePtr collector_node, xmlDocPtr config)
{
	int i, j, k, l;
	xmlDocPtr collector_doc = NULL, exporter_doc = NULL;
	xmlNodePtr aux_node = NULL;
	xmlXPathContextPtr internal_ctxt = NULL, collector_ctxt = NULL,
	        config_ctxt = NULL, exporter_ctxt = NULL;
	xmlXPathObjectPtr xpath_obj_expprocnames = NULL, xpath_obj_expproc = NULL,
	        xpath_obj_filewriters = NULL, xpath_obj_plugin_desc = NULL;
	xmlChar *file_format, *file_format_inter, *plugin_file;
	struct plugin_xml_conf_list* plugins = NULL, *aux_plugin = NULL;

	/* initiate internal config - open xml file, get xmlDoc and prepare xpath context for it */
	if ((internal_ctxt = ic_init (BAD_CAST "cesnet-ipfixcol-int")) == NULL) {
		goto cleanup;
	}

	/* get the list of supported storage plugins description (including supported file formats) */
	xpath_obj_plugin_desc = xmlXPathEvalExpression (BAD_CAST "/cesnet-ipfixcol-int:ipfixcol/cesnet-ipfixcol-int:storagePlugin", internal_ctxt);
	if (xpath_obj_plugin_desc != NULL) {
		if (xmlXPathNodeSetIsEmpty (xpath_obj_plugin_desc->nodesetval)) {
			VERBOSE(CL_VERBOSE_OFF, "No list of supported Storage formats found in internal configuration!");
			goto cleanup;
		}
	}

	/* get all <exportingProcess>s from the collector node */
	collector_doc = xmlNewDoc (BAD_CAST "1.0");
	xmlDocSetRootElement (collector_doc, xmlCopyNode (collector_node, 1));

	/* create xpath evaluation context of collector node */
	if ((collector_ctxt = xmlXPathNewContext (collector_doc)) == NULL) {
		VERBOSE(CL_VERBOSE_OFF, "Unable to create XPath context for collectingProcess (%s:%d).", __FILE__, __LINE__);
		goto cleanup;
	}

	/* register namespace for the context of internal configuration file */
	if (xmlXPathRegisterNs (collector_ctxt, BAD_CAST "ietf-ipfix", BAD_CAST "urn:ietf:params:xml:ns:yang:ietf-ipfix-psamp") != 0) {
		VERBOSE(CL_VERBOSE_OFF, "Unable to register namespace for collectingProcess (%s:%d).", __FILE__, __LINE__);
		return (NULL);
	}

	/* search for <exportingProcess>s nodes defining exporters (including fileWriters) */
	xpath_obj_expprocnames = xmlXPathEvalExpression (BAD_CAST "/ietf-ipfix:collectingProcess/ietf-ipfix:exportingProcess", collector_ctxt);
	if (xpath_obj_expprocnames != NULL) {
		if (xmlXPathNodeSetIsEmpty (xpath_obj_expprocnames->nodesetval)) {
			VERBOSE(CL_VERBOSE_BASIC, "No exportingProcess defined in the collectingProcess!");
			goto cleanup;
		}
	}

	/* create xpath evaluation context of user configuration */
	if ((config_ctxt = xmlXPathNewContext (config)) == NULL) {
		VERBOSE(CL_VERBOSE_OFF, "Unable to create XPath context for user configuration (%s:%d).", __FILE__, __LINE__);
		goto cleanup;
	}

	/* register namespace for the context of internal configuration file */
	if (xmlXPathRegisterNs (config_ctxt, BAD_CAST "ietf-ipfix", BAD_CAST "urn:ietf:params:xml:ns:yang:ietf-ipfix-psamp") != 0) {
		VERBOSE(CL_VERBOSE_OFF, "Unable to register namespace for user configuration (%s:%d).", __FILE__, __LINE__);
		goto cleanup;
	}

	/* now look for <exportingProcess> definition with name(s) specified in <collectingProcess> */
	/* first, get all <exportingProcess>es */
	xpath_obj_expproc = xmlXPathEvalExpression (BAD_CAST "/ietf-ipfix:ipfix/ietf-ipfix:exportingProcess", config_ctxt);
	if (xpath_obj_expproc != NULL) {
		if (xmlXPathNodeSetIsEmpty (xpath_obj_expproc->nodesetval)) {
			VERBOSE(CL_VERBOSE_OFF, "No exporting process defined in user configuration!");
			goto cleanup;
		}
	}
	/* and then check them for searching names */
	for (i = 0; i < xpath_obj_expprocnames->nodesetval->nodeNr; i++) {
		for (j = 0; j < xpath_obj_expproc->nodesetval->nodeNr; j++) {
			aux_node = xpath_obj_expproc->nodesetval->nodeTab[j]->children;
			while (aux_node) {
				/* get the <name> element */
				if (!xmlStrncmp (aux_node->name, BAD_CAST "name", strlen ("name") + 1)) {
					/* compare the content of <name> in user config and <name> in the internal config */
					if (!xmlStrncmp (xpath_obj_expprocnames->nodesetval->nodeTab[i]->children->content, aux_node->children->content, xmlStrlen (aux_node->children->content) + 1)) {
						/* we got it! maybe :) */

						/* now check if it is a <fileWriter> because all storage plugins ARE <fileWriter>s */
						exporter_doc = xmlNewDoc (BAD_CAST "1.0");
						xmlDocSetRootElement (exporter_doc, xmlCopyNode (xpath_obj_expproc->nodesetval->nodeTab[j], 1));

						/* create xpath evaluation context of <exportingProcess> node */
						exporter_ctxt = xmlXPathNewContext (exporter_doc);
						if (exporter_ctxt == NULL) {
							VERBOSE(CL_VERBOSE_OFF, "Unable to create XPath context for exportingProcess (%s:%d).", __FILE__, __LINE__);
							goto cleanup;
						}

						/* register namespace for the context of <exportingProcess> in user config file */
						if (xmlXPathRegisterNs (exporter_ctxt, BAD_CAST "ietf-ipfix", BAD_CAST "urn:ietf:params:xml:ns:yang:ietf-ipfix-psamp") != 0) {
							VERBOSE(CL_VERBOSE_OFF, "Unable to register namespace for exportingProcess (%s:%d).", __FILE__, __LINE__);
							goto cleanup;
						}
						/* search for <destination>/<fileWriter> nodes defining ipfixcol's storage plugins */
						xpath_obj_filewriters = xmlXPathEvalExpression (BAD_CAST "/ietf-ipfix:exportingProcess/ietf-ipfix:destination/ietf-ipfix:fileWriter", exporter_ctxt);
						if (xpath_obj_filewriters != NULL) {
							if (xmlXPathNodeSetIsEmpty (xpath_obj_filewriters->nodesetval)) {
								/* no fileWriter found */
								xmlXPathFreeObject (xpath_obj_filewriters);
								xpath_obj_filewriters = NULL;
								xmlXPathFreeContext (exporter_ctxt);
								exporter_ctxt = NULL;
								xmlFreeDoc (exporter_doc);
								exporter_doc = NULL;
								/* break while loop to get to another exportingProcess */
								break;
							}
						}
						/* now we have a <fileWriter> node with description of storage plugin */
						/* but first we have to check that we support this storage plugin type (according to fileFormat) */
						for (k = 0; k < xpath_obj_filewriters->nodesetval->nodeNr; k++) {
							for (l = 0; l < xpath_obj_plugin_desc->nodesetval->nodeNr; l++) {
								file_format_inter = get_children_content (xpath_obj_plugin_desc->nodesetval->nodeTab[l], BAD_CAST "fileFormat");
								if (file_format_inter == NULL) {
									/* this plugin description node is invalid, there is no fileFormat element */
									VERBOSE(CL_VERBOSE_BASIC, "storagePlugin with missing fileFormat detected!");
									continue;
								}
								file_format = get_children_content (xpath_obj_filewriters->nodesetval->nodeTab[k], BAD_CAST "fileFormat");
								if (file_format == NULL) {
									/* this fileWriter has no fileFormat element - use default format */
									VERBOSE(CL_VERBOSE_BASIC, "User configuration contain fileWriter without specified format - using %s.", DEFAULT_STORAGE_PLUGIN);
									/* do not allocate memory since we always use strings allocated at other places or static strings */
									file_format = BAD_CAST DEFAULT_STORAGE_PLUGIN;
								}
								if (!xmlStrncmp (file_format_inter, file_format, xmlStrlen (file_format) + 1)) {
									/* now we are almost done - prepare an item of the plugin list for return */
									plugin_file = get_children_content (xpath_obj_plugin_desc->nodesetval->nodeTab[l], BAD_CAST "file");
									if (plugin_file == NULL) {
										VERBOSE(CL_VERBOSE_BASIC, "Unable to detect path to storage plugin file for %s format in the internal configuration!", file_format_inter);
										break;
									}
									/* prepare plugin info structure for return list */
									aux_plugin = (struct plugin_xml_conf_list*) malloc (sizeof(struct plugin_xml_conf_list));
									aux_plugin->config.file = (char*) malloc (sizeof(char) * (xmlStrlen (plugin_file) + 1));
									strncpy (aux_plugin->config.file, (char*) plugin_file, xmlStrlen (plugin_file) + 1);
									aux_plugin->config.xmldata = xmlNewDoc (BAD_CAST "1.0");
									xmlDocSetRootElement (aux_plugin->config.xmldata, xmlCopyNode (xpath_obj_filewriters->nodesetval->nodeTab[k], 1));
									/* link new plugin item into the return list */
									aux_plugin->next = plugins;
									plugins = aux_plugin;
								}
							}
						}
						/* break while loop to get to another exportingProcess */
						break;
					}
				}
				/* go to the next exportingProcess element */
				aux_node = aux_node->next;
			}
		}
	}
	/* inform that everything was done but no valid plugin has been found */
	if (plugins == NULL) {
		VERBOSE(CL_VERBOSE_BASIC, "No valid storage plugin specification for the collector found.");
	}

	cleanup:
	/* Cleanup of XPath data */
	if (xpath_obj_expproc) {
		xmlXPathFreeObject (xpath_obj_expproc);
	}
	if (xpath_obj_expprocnames) {
		xmlXPathFreeObject (xpath_obj_expprocnames);
	}
	if (xpath_obj_filewriters) {
		xmlXPathFreeObject (xpath_obj_filewriters);
	}
	if (xpath_obj_plugin_desc) {
		xmlXPathFreeObject (xpath_obj_plugin_desc);
	}
	if (collector_ctxt) {
		xmlXPathFreeContext (collector_ctxt);
	}
	if (config_ctxt) {
		xmlXPathFreeContext (config_ctxt);
	}
	if (exporter_ctxt) {
		xmlXPathFreeContext (exporter_ctxt);
	}
	if (internal_ctxt) {
		xmlFreeDoc (internal_ctxt->doc);
		xmlXPathFreeContext (internal_ctxt);
	}
	if (exporter_doc) {
		xmlFreeDoc (exporter_doc);
	}
	if (collector_doc) {
		xmlFreeDoc (collector_doc);
	}

	return (plugins);
}

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
struct plugin_xml_conf_list* get_input_plugins (xmlNodePtr collector_node)
{
	int i, j;
	xmlChar *collector_name;
	xmlNodePtr auxNode = NULL, children1 = NULL, children2 = NULL;
	xmlXPathContextPtr internal_ctxt = NULL;
	xmlXPathObjectPtr xpath_obj_suppcolls = NULL, xpath_obj_file = NULL;
	struct plugin_xml_conf_list *retval = NULL;

	/* prepare return structure */
	retval = (struct plugin_xml_conf_list*) malloc (sizeof(struct plugin_xml_conf_list));
	if (retval == NULL) {
		VERBOSE(CL_VERBOSE_OFF, "Memory allocation failed (%s:%d)", __FILE__, __LINE__);
		return (NULL);
	}
	retval->next = NULL;
	retval->config.xmldata = NULL;
	retval->config.file = NULL;

	/* initiate internal config - open xml file, get xmlDoc and prepare xpath context for it */
	internal_ctxt = ic_init (BAD_CAST "cesnet-ipfixcol-int");
	if (internal_ctxt == NULL) {
		free (retval);
		return (NULL);
	}

	/*
	 * get the list of supported Collector types which will be used to get
	 * collector information from the user configuration file
	 */
	xpath_obj_suppcolls = xmlXPathEvalExpression (BAD_CAST "/cesnet-ipfixcol-int:ipfixcol/cesnet-ipfixcol-int:supportedCollectors/cesnet-ipfixcol-int:name", internal_ctxt);
	if (xpath_obj_suppcolls != NULL) {
		if (xmlXPathNodeSetIsEmpty (xpath_obj_suppcolls->nodesetval)) {
			VERBOSE(CL_VERBOSE_BASIC, "No list of supportedCollectors found in internal configuration!");
			free (retval);
			retval = NULL;
			goto cleanup;
		}
	}

	/* get paths to libraries implementing plugins from internal configuration */
	for (j = 0; j < xpath_obj_suppcolls->nodesetval->nodeNr; j++) {
		auxNode = get_children (collector_node, xpath_obj_suppcolls->nodesetval->nodeTab[j]->children->content);

		/* finnish the loop if the collector description found */
		if (auxNode) {
			break;
		}
	}
	/* if we didn't found any valid collector description, we have to quit */
	if (!auxNode) {
		VERBOSE(CL_VERBOSE_OFF, "No valid collectingProcess description found!;");
		free (retval);
		retval = NULL;
		goto cleanup;
	}

	/* remember node with input plugin parameters */
	retval->config.xmldata = xmlNewDoc (BAD_CAST "1.0");
	xmlDocSetRootElement (retval->config.xmldata, xmlCopyNode (auxNode, 1));

	/*
	 * remember filename of input plugin implementation
	 */
	/* we are looking for the node with this name node */
	collector_name = xpath_obj_suppcolls->nodesetval->nodeTab[j]->children->content;

	/* first get list of inputPlugin nodes in internal configuration file */
	xpath_obj_file = xmlXPathEvalExpression (BAD_CAST "/cesnet-ipfixcol-int:ipfixcol/cesnet-ipfixcol-int:inputPlugin", internal_ctxt);
	if (xpath_obj_file != NULL) {
		if (xmlXPathNodeSetIsEmpty (xpath_obj_file->nodesetval)) {
			VERBOSE(CL_VERBOSE_BASIC, "No inputPlugin definition found in internal configuration!");
			free (retval);
			retval = NULL;
			goto cleanup;
		}
	}
	/* and now select the one with required name element */
	for (i = 0; i < xpath_obj_file->nodesetval->nodeNr; i++) {
		children1 = children2  = xpath_obj_file->nodesetval->nodeTab[i]->children;
		while (children1) {
			if ((!strncmp ((char*) children1->name, "name", strlen ("name") + 1))
			        && (!xmlStrncmp (children1->children->content, collector_name, xmlStrlen (collector_name) + 1))) {
				while (children2) {
					if (!xmlStrncmp (children2->name, BAD_CAST "file", strlen ("file") + 1)) {
						retval->config.file = (char*) malloc (sizeof(char) * (strlen ((char*) children2->children->content) + 1));
						strncpy (retval->config.file, (char*) children2->children->content, strlen ((char*) children2->children->content) + 1);
						goto found_input_plugin_file;
					}
					children2 = children2->next;
				}
			}
			children1 = children1->next;
		}
	}

found_input_plugin_file:
	if (retval->config.file == NULL) {
		VERBOSE(CL_VERBOSE_OFF, "No definition for collector found.")
		free (retval);
		retval = NULL;
	}

cleanup:
	/* Cleanup of XPath data */
	if (xpath_obj_file) {
		xmlXPathFreeObject (xpath_obj_file);
	}
	if (xpath_obj_suppcolls) {
		xmlXPathFreeObject (xpath_obj_suppcolls);
	}
	if (internal_ctxt) {
		xmlFreeDoc (internal_ctxt->doc);
		xmlXPathFreeContext (internal_ctxt);
	}

	return (retval);
}

/**
 * @brief Get list of \<collectingPrecoess\>es from user configuration.
 * @param doc User XML configuration.
 * @return List of \<collectingProcess\> nodes in the form of XPath objects, NULL
 * in case of error.
 */
xmlXPathObjectPtr get_collectors (xmlDocPtr doc)
{
	xmlXPathObjectPtr retval = NULL;
	xmlXPathContextPtr context = NULL;

	/* create xpath evaluation context */
	if ((context = xmlXPathNewContext (doc)) == NULL) {
		return NULL;
	}

	/* register namespace */
	if (xmlXPathRegisterNs (context, BAD_CAST "ietf-ipfix", BAD_CAST "urn:ietf:params:xml:ns:yang:ietf-ipfix-psamp")
	        != 0) {
		return NULL;
	}

	/* search for collectingProcess nodes defining collectors */
	if ((retval
	        = xmlXPathEvalExpression (BAD_CAST "/ietf-ipfix:ipfix/ietf-ipfix:collectingProcess", context))
	        != NULL) {
		if (xmlXPathNodeSetIsEmpty (retval->nodesetval)) {
			xmlXPathFreeObject (retval);
			retval = NULL;
		}
	}

	/* Cleanup of XPath data */
	xmlXPathFreeContext (context);

	return (retval);
}

/**@}*/
