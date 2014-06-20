/**
 * \file forwarding.c
 * \author Michal Kozubik <michal.kozubik@cesnet.cz>
 * \brief Storage plugin that forwards data to network
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

#include <ipfixcol.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <stdbool.h>

#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>
#include "../../preprocessor.h"

/* Identifier for MSG_* */
static const char *msg_module = "forwarding storage";

union addr_t {
	struct sockaddr_in  addr4;
	struct sockaddr_in6 addr6;
};

typedef struct forwarding_config {
	enum SOURCE_TYPE type;
	int version, port, sockfd;
	union addr_t addr;
	struct udp_conf udp;
	int records_cnt, records_max;
	struct ipfix_template_record **records;
} forwarding;

/**
 * \brief Connect to IPv4 destination address
 * \param[in] conf Plugin configuration
 * \param[in] destination IPv4 address
 * \return 0 on success
 */
int forwarding_connect4(forwarding *conf, char *destination)
{
	struct hostent *server = gethostbyname(destination);
	if (!server) {
		MSG_ERROR(msg_module, "No such host \"%s\"", destination);
		return 1;
	}

	memmove((char *) &(conf->addr.addr4.sin_addr), (char *) server->h_addr, server->h_length);
	conf->addr.addr4.sin_family = AF_INET;
	conf->addr.addr4.sin_port = htons(conf->port);

	if (conf->type == SOURCE_TYPE_TCP) {
		conf->sockfd = socket(AF_INET, SOCK_STREAM, 0);
		if (conf->sockfd < 0) {
			MSG_ERROR(msg_module, "Cannot open socket");
			return 1;
		}
		if (connect(conf->sockfd, (struct sockaddr *) &(conf->addr.addr4), sizeof(conf->addr.addr4)) < 0) {
			MSG_ERROR(msg_module, "Cannot connect to \"%s:%d\" - %s", destination, conf->port, sys_errlist[errno]);
			return 1;
		}
		MSG_NOTICE(msg_module, "Connected to %s:%d", destination, conf->port);
	} else {
		conf->sockfd = socket(AF_INET, SOCK_DGRAM, 0);
		if (conf->sockfd < 0) {
			MSG_ERROR(msg_module, "Cannot open socket");
			return 1;
		}
	}

	return 0;
}

/**
 * \brief Connect to IPv6 destination address
 * \param[in] conf Plugin configuration
 * \param[in] destination destination IPv6 address
 * \return 0 on success
 */
int forwarding_connect6(forwarding *conf, char *destination)
{
	struct hostent *server = gethostbyname2(destination, AF_INET6);
	if (!server) {
		MSG_ERROR(msg_module, "No such host \"%s\"", destination);
		return 1;
	}

	memmove((char *) &(conf->addr.addr6.sin6_addr), (char *) server->h_addr, server->h_length);
	conf->addr.addr6.sin6_flowinfo = 0;
	conf->addr.addr6.sin6_family = AF_INET6;
	conf->addr.addr6.sin6_port = htons(conf->port);

	if (conf->type == SOURCE_TYPE_TCP) {
		conf->sockfd = socket(AF_INET6, SOCK_STREAM, 0);
		if (conf->sockfd < 0) {
			MSG_ERROR(msg_module, "Cannot open socket");
			return 1;
		}
		if (connect(conf->sockfd, (struct sockaddr *) &(conf->addr.addr6), sizeof(conf->addr.addr6)) < 0) {
			MSG_ERROR(msg_module, "Cannot connect to \"%s:%d\" - %s", destination, conf->port, sys_errlist[errno]);
			return 1;
		}
		MSG_NOTICE(msg_module, "Connected to %s:%d", destination, conf->port);
	} else {
		conf->sockfd = socket(AF_INET6, SOCK_DGRAM, 0);
		if (conf->sockfd < 0) {
			MSG_ERROR(msg_module, "Cannot open socket");
			return 1;
		}
	}

	return 0;
}

/**
 * \brief Get index to array of template record for given Template ID
 * \param[in] conf Plugin configuration
 * \param[in] tid Template ID
 * \return Index on success, -1 otherwise
 */
int forwarding_get_record_index(forwarding *conf, uint16_t tid)
{
	int i, c;
	for (i = 0, c = 0; i < conf->records_max && c < conf->records_cnt; ++i) {
		if (conf->records[i] != NULL) {
			c++;
			if (conf->records[i]->template_id == tid) {
				return i;
			}
		}
	}
	return -1;
}

/**
 * \brief Get template record with given Template ID
 * \param[in] conf Plugin configuration
 * \param[in] tid Template ID
 * \return Pointer to template
 */
struct ipfix_template_record *forwarding_get_record(forwarding *conf, uint16_t tid)
{
	int i = forwarding_get_record_index(conf, tid);
	if (i < 0) {
		MSG_DEBUG(msg_module, "%d not found", ntohs(tid));
		return NULL;
	}
	MSG_DEBUG(msg_module, "%d found", ntohs(tid));
	return conf->records[i];
}

/**
 * \brief Remove template record with givet Template ID from array
 * \param[in] conf Plugin configuration
 * \param[in] tid Template ID
 */
void forwarding_remove_record(forwarding *conf, uint32_t tid)
{
	int i = forwarding_get_record_index(conf, tid);
	if (i >= 0) {
		free(conf->records[i]);
		conf->records[i] = NULL;
		conf->records_cnt--;
	}
}

/**
 * \brief Add new template record into array
 * \param[in] conf Plugin configuration
 * \param[in] templ New template record
 * \return Pointer to inserted template record
 */
struct ipfix_template_record *forwarding_add_record(forwarding *conf, struct ipfix_template_record *record)
{
	int i;
	MSG_DEBUG(msg_module, "%d adding", ntohs(record->template_id));
	if (conf->records_cnt == conf->records_max) {
		/* Array is full, need more memory */
		conf->records_max += 32;
		conf->records = realloc(conf->records, conf->records_max * sizeof(struct ipfix_template_record *));
		if (conf->records == NULL) {
			MSG_ERROR(msg_module, "Not enought memory (%s:%d)", __FILE__, __LINE__);
			return NULL;
		}
		/* Initialize new pointers */
		for (i = conf->records_cnt; i < conf->records_max; ++i) {
			conf->records[i] = NULL;
		}
	}

	/* Find place for template */
	for (i = 0; i < conf->records_max; ++i) {
		if (conf->records[i] == NULL) {
			conf->records[i] = record;
			conf->records_cnt++;
			return conf->records[i];
		}
	}

	return NULL;
}

/**
 * \brief Initialize template records array
 * \param[in] conf Plugin configuration
 * \return 0 on success
 */
int forwarding_init_records(forwarding *conf)
{
	conf->records_cnt = 0;
	conf->records_max = 32;

	conf->records = calloc(conf->records_max, sizeof(struct ipfix_template_record *));
	if (conf->records== NULL) {
		return 1;
	}
	return 0;
}

/**
 * \brief Initialize configuration by xml file
 * \param[in] conf Plugin configuration
 * \param[in] root XML root element
 * \return Destination address
 */
char *forwarding_init_conf(forwarding *conf, xmlDoc *doc, xmlNodePtr root)
{
	xmlNodePtr cur = root->children;

	bool type = false, addr = false, port = false;
	char *destination = NULL;
	xmlChar *aux_str = NULL;

	while (cur) {
		if (!xmlStrcmp(cur->name, (const xmlChar *) "type")) {
			/* Get connection type */
			if (type) {
				MSG_ERROR(msg_module, "Multiple occurrences of type!");
				return NULL;
			}
			aux_str = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			if (!xmlStrcasecmp(aux_str, (const xmlChar *) "tcp")) {
				/* TCP */
				conf->type = SOURCE_TYPE_TCP;
			} else if (!xmlStrcasecmp(aux_str, (const xmlChar *) "udp")) {
				/* UDP */
				conf->type = SOURCE_TYPE_UDP;
			} else {
				MSG_ERROR(msg_module, "Unknown connection type \"%s\"!", aux_str);
				xmlFree(aux_str);
				return NULL;
			}
			type = true;
		} else if (!xmlStrcasecmp(cur->name, (const xmlChar *) "ipv4")) {
			/* Parse IPv4 destination address */
			if (addr) {
				MSG_ERROR(msg_module, "Multiple occurrences of IP address!");
				free(destination);
				return NULL;
			}
			destination = (char *) xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			conf->version = 4;
			addr = true;
		} else if (!xmlStrcasecmp(cur->name, (const xmlChar *) "ipv6")) {
			/* Parse IPv6 destination address */
			if (addr) {
				MSG_ERROR(msg_module, "Multiple occurrences of IP address!");
				free(destination);
				return NULL;
			}
			destination = (char *) xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			conf->version = 6;
			addr = true;
		} else if (!xmlStrcasecmp(cur->name, (const xmlChar *) "port")) {
			/* Get destination port */
			if (port) {
				MSG_ERROR(msg_module, "Multiple occurrences of port!");
				return NULL;
			}
			aux_str = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			conf->port = atoi((char *) aux_str);
			port = true;

		/* UDP configuration */
		} else if (!xmlStrcasecmp(cur->name, (const xmlChar *) "templateLifeTime")) {
			aux_str = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			conf->udp.template_life_time = atoi((char *) aux_str);
		} else if (!xmlStrcasecmp(cur->name, (const xmlChar *) "optionsTemplateLifeTime")) {
			aux_str = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			conf->udp.options_template_life_time = atoi((char *) aux_str);
		} else if (!xmlStrcasecmp(cur->name, (const xmlChar *) "templateLifePacket")) {
			aux_str = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			conf->udp.template_life_packet = atoi((char *) aux_str);
		} else if (!xmlStrcasecmp(cur->name, (const xmlChar *) "optionsTemplateLifePacket")) {
			aux_str = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			conf->udp.options_template_life_packet = atoi((char *) aux_str);
		} else if (xmlStrcmp(cur->name, (const xmlChar *) "fileFormat")) {
			MSG_WARNING(msg_module, "Unknown element \"%s\"!", cur->name);
		}

		if (aux_str) {
			xmlFree(aux_str);
			aux_str = NULL;
		}
		cur = cur->next;
	}

	if (!type || !addr || !port) {
		MSG_ERROR(msg_module, "Missing some configuration element(s)");
		return NULL;
	}

	if (forwarding_init_records(conf)) {
		free(destination);
		return NULL;
	}

	return destination;
}

/**
 * \brief Initialize plugin
 * \param[in] param Parameters
 * \param[out] config Plugin configuration
 * \return 0 on success
 */
int storage_init(char *params, void **config)
{
	forwarding *conf = NULL;
	xmlDoc *doc = NULL;
	xmlNodePtr root = NULL;
	char *destination = NULL;
	int ret;

	MSG_DEBUG(msg_module, "Initialization");

	conf = calloc(1, sizeof(forwarding));
	if (!conf) {
		MSG_ERROR(msg_module, "Not enough memory (%s:%d)", __FILE__, __LINE__);
		return -1;
	}

	if (!params) {
		MSG_ERROR(msg_module, "Missing plugin configuration!");
		free(conf);
		return -1;
	}

	doc = xmlParseDoc(BAD_CAST params);
	if (!doc) {
        MSG_ERROR(msg_module, "Cannot parse config xml!");
        free(conf);
        return -1;
	}

	root = xmlDocGetRootElement(doc);
	if (!root) {
		MSG_ERROR(msg_module, "Cannot get document root element!");
		free(conf);
		return -1;
	}

	if (xmlStrcmp(root->name, (const xmlChar *) "fileWriter")) {
		MSG_ERROR(msg_module, "Root node != fileWriter");
		goto init_err;
	}

	destination = forwarding_init_conf(conf, doc, root);
	if (destination == NULL) {
		goto init_err;
	}

	if (conf->version == 4) {
		ret = forwarding_connect4(conf, destination);
	} else {
		ret = forwarding_connect6(conf, destination);
	}

	if (ret) {
		goto init_err;
	}

	*config = conf;

	xmlFreeDoc(doc);
	return 0;

init_err:
	xmlFreeDoc(doc);
	free(conf);
	return -1;
}

/**
 * \brief Store everything - flush buffers
 * \param[in] config Plugin configuration
 * \return 0 on success
 */
int store_now(const void *config)
{
	forwarding *conf = (forwarding *) config;

	MSG_DEBUG(msg_module, "Flushing data");
	fsync(conf->sockfd);

	return 0;
}

/**
 * \brief Conver IPFIX message into packet
 * \param[in] msg IPFIX message structure
 * \param[out] packet_length Length of packet
 * \return pointer to packet
 */
void *msg_to_packet(const struct ipfix_message *msg, int *packet_length)
{
	struct ipfix_set_header *aux_header;
	int i, c, len, offset = 0;

	*packet_length = ntohs(msg->pkt_header->length);
	void *packet = calloc(1, *packet_length);

	if (!packet) {
		return NULL;
	}

	/* Copy header */
	memcpy(packet, msg->pkt_header, IPFIX_HEADER_LENGTH);
	offset += IPFIX_HEADER_LENGTH;

	/* Copy template sets */
	for (i = 0; msg->templ_set[i] != NULL && i < 1024; ++i) {
		aux_header = &(msg->templ_set[i]->header);
		len = ntohs(aux_header->length);
		for (c = 0; c < len; c += 4) {
			memcpy(packet + offset + c, aux_header, 4);
			aux_header++;
		}
		offset += len;
	}

	/* Copy data sets */
	for (i = 0; msg->data_couple[i].data_set != NULL && i < 1024; ++i) {
		len = ntohs(msg->data_couple[i].data_set->header.length);
		memcpy(packet + offset,   &(msg->data_couple[i].data_set->header), 4);
		memcpy(packet + offset + 4, msg->data_couple[i].data_set->records, len - 4);
		offset += len;
	}

	return packet;
}

/**
 * \brief Send UDP packet
 * \param[in] conf Plugin configuration
 * \param[in] packet IPFIX packet
 * \param[in] length Packet length
 * \return -1 on error
 */
int forwarding_send_udp(forwarding *conf, void *packet, int length)
{
	if (conf->version == 4) {
		return sendto(conf->sockfd, packet, length, 0,
				(struct sockaddr *) &(conf->addr.addr4), sizeof(conf->addr.addr4));
	} else {
		return sendto(conf->sockfd, packet, length, 0,
				(struct sockaddr *) &(conf->addr.addr6), sizeof(conf->addr.addr6));
	}
}

/**
 * \brief Send TCP packet
 * \param[in] conf Plugin configuration
 * \param[in] packet IPFIX packet
 * \param[in] length Packet length
 * \return -1 on error
 */
int forwarding_send_tcp(forwarding *conf, void *packet, int length)
{
	return send(conf->sockfd, packet, length, 0);
}

/**
 * \brief Send packet
 * \param[in] conf Plugin configuration
 * \param[in] packet IPFIX packet
 * \param[in] length Packet length
 * \return 0 on success
 */
int forwarding_send(forwarding *conf, void *packet, int length)
{
	int ret;
	if (conf->type == SOURCE_TYPE_UDP) {
		ret = forwarding_send_udp(conf, packet, length);
	} else {
		ret = forwarding_send_tcp(conf, packet, length);
	}
	if (ret < 0) {
		return 1;
	}
	return 0;
}

/**
 * \brief Checks whether template record was send earlier
 *
 * Also controls template updates and inserts not-sent records into array
 *
 * \param[in] conf Plugin configuration
 * \param[in] rec Template record
 * \param[in] len Template record's length
 * \return 0 if it wasnt sent
 */
int forwarding_record_sent(forwarding *conf, struct ipfix_template_record *rec, int len)
{
	int i = forwarding_get_record_index(conf, rec->template_id);

	if (i >= 0) {
		if (tm_compare_template_records(conf->records[i], rec)) {
			/* records are equal */
			return 1;
		}
		/* stored record is old, update it */
		forwarding_remove_record(conf, conf->records[i]->template_id);
	}

	/* Store new record */
	struct ipfix_template_record *stored = calloc(1, len);
	if (!stored) {
		MSG_ERROR(msg_module, "Not enought memory (%s:%d)", __FILE__, __LINE__);
		return 0;
	}

	memmove(stored, rec, len);
	forwarding_add_record(conf, stored);
	return 0;
}

/**
 * \brief Remove template sets without any template records
 * \param[in] msg IPFIX message
 */
void forwarding_remove_empty_sets(struct ipfix_message *msg)
{
	int i, j, len;
	for (i = 0, j = 0; msg->templ_set[i] != NULL && i < 1024; ++i) {
		len = ntohs(msg->templ_set[i]->header.length);
		if (len <= 4) {
			/* Set correct message length */
			msg->pkt_header->length = htons(ntohs(msg->pkt_header->length) - len);
			/* Shift all template sets behind this (there must not be hole) */
			for (j = i; msg->templ_set[j]; ++j) {
				msg->templ_set[j] = msg->templ_set[j + 1];
			}
		}
	}
}

/**
 * \brief Remove templates already sent in past (TCP only)
 * \param[in] conf Plugin configuration
 * \param[in] msg IPFIX message
 * \return 0 on success
 */
int forwarding_remove_sent_templates(forwarding *conf, const struct ipfix_message *msg)
{
	int i, max_len, rec_len;
	uint8_t *ptr;
	uint32_t data_len;
	struct ipfix_template_record *aux_rec;

	/* Go throught all template sets and remove already sent template records */
	for (i = 0; msg->templ_set[i] != NULL && i < 1024; ++i) {
		ptr = (uint8_t*) &msg->templ_set[i]->first_record;
		while (ptr < (uint8_t*) msg->templ_set[i] + ntohs(msg->templ_set[i]->header.length)) {
			max_len = ((uint8_t *) msg->templ_set[i] + ntohs(msg->templ_set[i]->header.length)) - ptr;
			aux_rec = (struct ipfix_template_record *) ptr;
			/* Get record length */
			rec_len = tm_template_record_length(aux_rec, max_len, TM_TEMPLATE, &data_len);

			if (rec_len == 0) {
				/* Malformed set? skip */
				break;
			}

			if (forwarding_record_sent(conf, aux_rec, rec_len)) {
				/* Record was sent earlier, skip it and correct template set length */
				memmove(ptr, ptr + rec_len, max_len - rec_len);
				msg->templ_set[i]->header.length = htons(ntohs(msg->templ_set[i]->header.length) - rec_len);
				msg->pkt_header->length = htons(ntohs(msg->pkt_header->length) - rec_len);
			} else {
				ptr += rec_len;
			}
		}
	}

	forwarding_remove_empty_sets((struct ipfix_message *) msg);

	return 0;
}

/**
 * \brief Store packet - make packet from message and write it into socket
 * \param[in] config Plugin configuration
 * \param[in] ipfix_msg IPFIX message
 * \param[in] template_mgr Template manager
 * \return 0 on success
 */
int store_packet(void *config, const struct ipfix_message *ipfix_msg,
                 const struct ipfix_template_mgr *template_mgr)
{
	(void) template_mgr;
	forwarding *conf = (forwarding *) config;
	void *packet;
	int ret, length;

	if (conf->type == SOURCE_TYPE_TCP) {
		/* Remove already sent templates */
		forwarding_remove_sent_templates(conf, ipfix_msg);
	}

	/* Create packet from message */
	packet = msg_to_packet(ipfix_msg, &length);
	if (!packet) {
		return 1;
	}

	/* Send data */
	ret = forwarding_send(conf, packet, length);
	free(packet);

	return ret;
}

/**
 * \brief Close plugin
 * \param[in] config Plugin configuration
 * \return 0 on success
 */
int storage_close(void **config)
{
	MSG_DEBUG(msg_module, "CLOSE");
	forwarding *conf = (forwarding *) *config;

	close(conf->sockfd);

	int i, c;
	for (i = 0, c = 0; i < conf->records_max && c < conf->records_cnt; ++i) {
		if (conf->records[i]) {
			c++;
			free(conf->records[i]);
		}
	}

	free(conf->records);
	free(conf);
	conf = NULL;

	return 0;
}
