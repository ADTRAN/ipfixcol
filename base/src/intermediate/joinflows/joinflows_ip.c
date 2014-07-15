/**
 * \file joinflows_ip.c
 * \author Michal Kozubik <kozubik.michal@gmail.com>
 * \brief Intermediate Process that is able to join multiple flows
 * into the one.
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


#include <stdlib.h>
#include <string.h>

#include <ipfixcol.h>
#include "../../intermediate_process.h"
#include "../../ipfix_message.h"

#include <libxml/parser.h>
#include <libxml/tree.h>

/* module name for MSG_* */
static const char *msg_module = "joinflows";

/* struct for mapping templates and records */
struct mapped_template {
	int references;
	uint16_t reclen;
	struct ipfix_template_record *rec;
	struct ipfix_template *templ;
};

/* struct for mapping ODIDs */
struct mapping {
	uint32_t orig_odid;  /* original ODID */
	uint32_t new_odid;	 /* new ODID */
	uint16_t orig_tid;   /* original Template ID */
	uint16_t new_tid;	 /* new Template ID */
	int orig_rec_len;    /* length of original record */
	struct mapped_template *new_templ;  /* mapped template */
	struct ipfix_template_record *orig_rec; /* original template record */
	struct mapping *next; /* next mapping */
};

/* reuse released Template ID */
struct tid_reuse {
	uint16_t tid;
	struct tid_reuse *next;
};

/* Mapping header (commo for multiple sources) */
struct mapping_header {
	struct mapping *first; /* first mapping record */
	uint16_t free_tid;     /* free Template ID */
	uint32_t new_odid;     /* new ODID */
	struct tid_reuse *reuse; /* released Template IDs */
	struct mapping_header *next; /* Next header */
};

/* structure for each mapped source ODID */
struct source {
	uint32_t orig_odid;      /* original ODID */
	uint32_t new_odid;		 /* new ODID */
	struct input_info *input_info;
	struct mapping_header *mapping; /* mapping common for all source ODIDs mapped on the same new ODID */
	struct source *next;
};

/* plugin's configuration structure */
struct joinflows_ip_config {
	char *params;  /* input parameters */
	void *ip_config; /* internal process configuration */
	struct source *sources; /* source structure for each ODID */
	struct mapping_header *mappings; /* mappings */
	struct source *default_source;  /* mapping for unmentioned ODIDs */
	uint32_t ip_id; /* source ID for Template Manager */
	struct ipfix_template_mgr *tm; /* Template Manager */
};

/* struct for data and template processing */
struct joinflows_processor {
	uint8_t *msg;
	uint16_t offset;
	uint32_t orig_odid;
	uint8_t *value;
	uint32_t length;
	struct source *src;
};

/* TODO!!!!!!!!*/
#define TEMPL_MAX_LEN 100000

/**
 * \brief Compate template records
 *
 * \param[in] first First template record
 * \param[in] lenf Length of first template record
 * \param[in] second Second template record
 * \param[in] lens Length of second template record
 * \param[in] odid ODID
 * \return non-zero if records differs
 */
int records_compare(struct ipfix_template_record *first, int lenf, struct ipfix_template_record *second, int lens, uint32_t odid)
{
	if (first == NULL || second == NULL) {
		return 1;
	}

	if (first == second) {
		return 0;
	}

	if (first->count != second->count) {
		return 1;
	}

	if (lenf != lens) {
		return 1;
	}

	return memcmp(((uint8_t *) first) + 4, ((uint8_t *) second) + 4, lenf - 4);
}

/**
 * \brief Update original template record (add field 405 - original ODID)
 *
 * \param[in] orig_rec Original template record
 * \param[in] rec_len Record length
 * \param[in] new_tid Template ID of new template record
 * \param[in] odid ODID
 * \return pointer to updated template
 */
struct mapped_template *updated_templ(struct ipfix_template_record *orig_rec, int rec_len, uint16_t new_tid, uint32_t odid)
{
	struct mapped_template *new_mapped;
	struct ipfix_template_record *new_rec;
	uint16_t field_num = htons(405);
	uint16_t field_len = htons(4);

	/* Allocate memory for updated template */
	new_mapped = malloc(sizeof(struct mapped_template));
	if (new_mapped == NULL) {
		return NULL;
	}

	new_rec = calloc(1, rec_len + 4);


	/* Copy all informations and add field 405 at the end */
	memcpy(new_rec, orig_rec, rec_len);
	memcpy(((uint8_t *)new_rec) + rec_len, &field_num, 2);
	memcpy(((uint8_t *)new_rec) + rec_len + 2, &field_len, 2);

	/* Set new values */
	new_rec->template_id = htons(new_tid);
	new_rec->count = htons(ntohs(new_rec->count) + 1);

	new_mapped->templ = tm_create_template(new_rec, TEMPL_MAX_LEN, TM_TEMPLATE, odid);
	new_mapped->rec = new_rec;
	new_mapped->reclen = rec_len + 4;
	new_mapped->references = 1;

	return new_mapped;
}

/**
 * \brief Get next free Template ID for this mapping header
 *
 * \param[in] map Mapping header
 * \return Template ID
 */
uint16_t mapping_get_free_tid(struct mapping_header *map)
{
	uint16_t tid;
	if (map->reuse) {
		tid = map->reuse->tid;
		struct tid_reuse *tmp = map->reuse;
		map->reuse = map->reuse->next;
		free(tmp);
	} else {
		tid = map->free_tid++;
	}

	return tid;
}

/**
 * \brief Reuse Template ID
 *
 * \param[in] map Mapping header
 * \param[in] tid Released Template ID
 */
void mapping_reuse_tid(struct mapping_header *map, uint16_t tid)
{
	struct tid_reuse *reuse = malloc(sizeof(struct tid_reuse));
	if (!reuse) {
		return;
	}

	reuse->tid = tid;
	reuse->next = map->reuse;
	map->reuse = reuse;
}

/**
 * \brief Find mapping for given ODID and TID
 *
 * \param[in] map Mapping header
 * \param[in] orig_odid Original ODID
 * \param[in] orig_tid Original Template ID
 * \return pointer to mapping
 */
struct mapping *mapping_lookup(struct mapping_header *map, uint32_t orig_odid, uint16_t orig_tid)
{
	if (map == NULL) {
		return NULL;
	}

	struct mapping *aux_map = map->first;
	while (aux_map) {
		if (aux_map->orig_odid == orig_odid && aux_map->orig_tid == orig_tid) {
			return aux_map;
		}
		aux_map = aux_map->next;
	}

	return NULL;
}

/**
 * \brief Insert mapping into map
 *
 * \param[in] map Mapping header
 * \param[in] new_map Inserted mapping
 */
void mapping_insert(struct mapping_header *map, struct mapping *new_map)
{
	new_map->next = map->first;
	map->first = new_map;
}

/**
 * \brief Create new mapping
 *
 * \param[in] map Mapping header
 * \param[in] orig_odid Original ODID
 * \param[in] orig_tid Original Template ID
 * \param[in] orig_rec Original template record
 * \param[in] rec_len Record's length
 * \return pointer to new mapping
 */
struct mapping *mapping_create(struct mapping_header *map, uint32_t orig_odid, uint16_t orig_tid, struct ipfix_template_record *orig_rec, int rec_len)
{
	struct mapping *new_map;

	new_map = malloc(sizeof(struct mapping));
	if (new_map == NULL) {
		return NULL;
	}

	/* Fill in informations of new mapping */
	new_map->orig_odid = orig_odid;
	new_map->orig_tid = orig_tid;
	new_map->orig_rec = malloc(rec_len);
	memcpy(new_map->orig_rec, orig_rec, rec_len);

	new_map->orig_rec_len = rec_len;
	new_map->new_odid = map->new_odid;
	new_map->new_tid = mapping_get_free_tid(map);



	/* Create updated template */
	new_map->new_templ = updated_templ(orig_rec, rec_len, new_map->new_tid, orig_odid);

	mapping_insert(map, new_map);

	MSG_DEBUG(msg_module, "[%u -> %u] New mapping from %u to %u", orig_odid, new_map->new_odid, orig_tid, new_map->new_tid);

	return new_map;
}

/**
 * \brief Create mapping from another one
 *
 * \param[in] orig_map Original mapping
 * \param[in] orig_odid Original ODID
 * \param[in] orig_tid Original Template ID
 * \return pointer to new mapping
 */
struct mapping *mapping_copy(struct mapping *orig_map, uint32_t orig_odid, uint16_t orig_tid)
{
	struct mapping *new_map = calloc(1, sizeof(struct mapping));
	if (new_map == NULL) {
		return NULL;
	}

	/* ODID and TID are different, rest is the same */
	new_map->orig_odid = orig_odid;
	new_map->orig_tid = orig_tid;

	new_map->orig_rec= orig_map->orig_rec;
	new_map->orig_rec_len = orig_map->orig_rec_len;
	new_map->new_odid = orig_map->new_odid;
	new_map->new_tid = orig_map->new_tid;
	new_map->new_templ = orig_map->new_templ;
	new_map->new_templ->references++;

	return new_map;
}

/**
 * \brief Find mapping with given template record
 *
 * \param[in] map Mapping header
 * \param[in] orig_rec Original template record
 * \param[in] rec_len Record's length
 * \return pointer to mapping
 */
struct mapping *mapping_find_equal(struct mapping_header *map, struct ipfix_template_record *orig_rec, int rec_len)
{
	struct mapping *aux_map;

	aux_map = map->first;
	while (aux_map) {
		if (!records_compare(aux_map->orig_rec, aux_map->orig_rec_len, orig_rec, rec_len, aux_map->orig_odid)) {
			return aux_map;
		}
		aux_map = aux_map->next;
	}

	return NULL;
}

/**
 * \brief Find and copy equal mapping
 *
 * \param[in] map Mapping header
 * \param[in] orig_odid Original ODID
 * \param[in] orig_tid Original TID
 * \param[in] orig_rec Original Template record
 * \param[in] rec_len Record's length
 * \return pointer to mapping
 */
struct mapping *mapping_equal(struct mapping_header *map, uint32_t orig_odid, uint16_t orig_tid, struct ipfix_template_record *orig_rec, int rec_len)
{
	struct mapping *new_mapping = NULL, *equal_mapping = NULL;

	equal_mapping = mapping_find_equal(map, orig_rec, rec_len);
	if (equal_mapping != NULL) {
		new_mapping = mapping_copy(equal_mapping, orig_odid, orig_tid);
		mapping_insert(map, new_mapping);
		MSG_DEBUG(msg_module, "[%u -> %u] Equal mapping from %u to %u", orig_odid, new_mapping->new_odid, orig_tid, new_mapping->new_tid);
	}
	return new_mapping;
}

/**
 * \brief Remove mapping
 *
 * \param[in,out] map Mapping header
 * \param[in] old_map Old mapping
 */
void mapping_remove(struct mapping_header *map, struct mapping *old_map)
{
	struct mapping *aux_map = map->first;

	if (map->first == old_map) {
		map->first = old_map->next;
	} else {
		while (aux_map) {
			if (aux_map->next == old_map) {
				aux_map->next = old_map->next;
				break;
			}

			aux_map = aux_map->next;
		}
	}

	old_map->new_templ->references--;
	if (old_map->new_templ->references <= 0) {
		/* If there is no reference on modified template, remove it */
		while (old_map->new_templ->templ->references > 0);
		free(old_map->new_templ->templ);
		free(old_map->new_templ->rec);
		mapping_reuse_tid(map, old_map->new_tid);
		free(old_map->new_templ);
		free(old_map->orig_rec);
	}

	/* Free mapping */
	free(old_map);
}

/**
 * \brief Destroy mapping
 *
 * \param[in] map Mapping header
 */
void mapping_destroy(struct mapping_header *map)
{
	struct mapping *aux_map = map->first;
	struct tid_reuse *aux_reuse = map->reuse;

	while (aux_map) {
		map->first = map->first->next;
		if (aux_map->new_templ != NULL) {
			aux_map->new_templ->references--;
			if (aux_map->new_templ->references <= 0) {
//				while (aux_map->new_templ->templ->references > 0);
				free(aux_map->new_templ->templ);
				free(aux_map->new_templ->rec);
				free(aux_map->new_templ);
			}
		}
		free(aux_map);

		aux_map = map->first;
	}

	while (aux_reuse) {
		map->reuse = map->reuse->next;
		free(aux_reuse);
		aux_reuse = map->reuse;
	}

	free(map);
}

/**
 * \brief Initialize joinflows plugin
 *
 * \param[in] params Plugin parameters
 * \param[in] ip_config Internal process configuration
 * \param[in] ip_id Source ID into Template Manager
 * \param[in] template_mgr Template Manager
 * \param[out] config Plugin configuration
 * \return 0 if everything OK
 */
int intermediate_plugin_init(char *params, void *ip_config, uint32_t ip_id, struct ipfix_template_mgr *template_mgr, void **config)
{
	struct joinflows_ip_config *conf;
	conf = (struct joinflows_ip_config *) calloc(1, sizeof(*conf));
	if (!conf) {
		MSG_ERROR(msg_module, "Unable to allocate memory (%s:%d)", __FILE__, __LINE__);
		return -1;
	}

	if (!params) {
		MSG_ERROR(msg_module, "Missing plugin configuration!");
		free(conf);
		return -1;
	}

	/* parse configuration */
	char *to = NULL;

	xmlDoc *doc = NULL;
	xmlNode *root = NULL;
	xmlNode *curr = NULL;
	xmlNode *join = NULL;
	xmlNode *from = NULL;

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

	struct mapping_header *new_map = NULL;
	struct source *src = NULL;

	for (curr = root->children; curr != NULL; curr = curr->next) {
		join = curr->children;
		for (join = curr->children; join != NULL; join = join->next) {
			to = (char *) xmlGetProp(curr, (const xmlChar *) "to");
			new_map = calloc(1, sizeof(struct mapping_header));
			new_map->free_tid = 256;
			new_map->new_odid = atoi(to);
			new_map->next = conf->mappings;
			conf->mappings = new_map;

			for (from = join->children; from != NULL; from = from->next) {
				if (!xmlStrcmp(from->content, (const xmlChar *) "*")) {
					if (!conf->default_source) {
						conf->default_source = calloc(1, sizeof(struct source));
					}
					src = conf->default_source;
				} else {
					src = calloc(1, sizeof(struct source));
					src->next = conf->sources;
					conf->sources = src;
					MSG_DEBUG(msg_module, "Setting sources");
				}

				src->mapping = new_map;
				src->orig_odid = atoi((char *)from->content);
				src->input_info = NULL;
				src->new_odid = new_map->new_odid;
			}
			xmlFree(to);
		}
	}

	conf->ip_id = ip_id;
	conf->ip_config = ip_config;
	conf->tm = template_mgr;

	xmlFreeDoc(doc);
	*config = conf;
	MSG_NOTICE(msg_module, "Successfully initialized");
	return 0;
}

/**
 * \brief Process new templates
 *
 * \param[in] rec Template record
 * \param[in] rec_len Record's length
 * \param[in] data Processor's data
 */
void templates_processor(uint8_t *rec, int rec_len, void *data)
{
	struct joinflows_processor *proc = (struct joinflows_processor *) data;
	struct ipfix_template_record *record = (struct ipfix_template_record *) rec;
	struct mapping *map = NULL;
	struct mapped_template *mapped = NULL;
	uint16_t orig_tid = ntohs(record->template_id);

	map = mapping_lookup(proc->src->mapping, proc->orig_odid, orig_tid);
	if (map == NULL) {
		map = mapping_equal(proc->src->mapping, proc->orig_odid, orig_tid, record, rec_len);
		if (map == NULL) {
			map = mapping_create(proc->src->mapping, proc->orig_odid, orig_tid, record, rec_len);
			mapped = map->new_templ;
		}
	} else if (records_compare(record, rec_len, map->orig_rec, map->orig_rec_len, map->orig_odid)) {
		/* UDPATED*/
		mapping_remove(proc->src->mapping, map);
		map = mapping_equal(proc->src->mapping, proc->orig_odid, orig_tid, record, rec_len);
		if (map == NULL) {
			map = mapping_create(proc->src->mapping, proc->orig_odid, orig_tid, record, rec_len);
			mapped = map->new_templ;
		}
	}

	/* Add new mapped record to template set */
	if (mapped) {
		memcpy(proc->msg + proc->offset, mapped->rec, mapped->reclen);
		proc->offset += mapped->reclen;
		proc->length += mapped->reclen;
	}
}

/**
 * \brief Process data records
 *
 * \param[in] rec Data record
 * \param[in] rec_len Redord's length
 * \param[in] templ Template
 * \param[in] data Processor's data
 */
void data_processor(uint8_t *rec, int rec_len, struct ipfix_template *templ, void *data)
{
	struct joinflows_processor *proc = (struct joinflows_processor *) data;

	memcpy(proc->msg + proc->offset, rec, rec_len);
	proc->offset += rec_len;

	memcpy(proc->msg + proc->offset, &(proc->orig_odid), 4);
	proc->offset += 4;
	proc->length += rec_len + 4;
}

/**
 * \brief Get source structure for this ODID
 *
 * \param[in] conf Plugin configuration
 * \param[in] odid ODID
 * \return source structure
 */
struct source *joinflows_get_source(struct joinflows_ip_config *conf, uint32_t odid)
{
	struct source *aux_src = NULL;

	for (aux_src = conf->sources; aux_src; aux_src = aux_src->next) {
		if (aux_src->orig_odid == odid) {
			return aux_src;
		}
	}

	return conf->default_source;
}

/**
 * \brief Update input_info structure
 *
 * \param[in] src Source structure
 * \param[in] inpu_info Original input_info structure
 */
void joinflows_update_input_info(struct source *src, struct input_info *input_info)
{
	if (src->input_info == NULL) {
		if (input_info->type == SOURCE_TYPE_IPFIX_FILE) {
			src->input_info = calloc(1, sizeof(struct input_info_file));
			memcpy(src->input_info, input_info, sizeof(struct input_info_file));
		} else {
			src->input_info = calloc(1, sizeof(struct input_info_network));
			memcpy(src->input_info, input_info, sizeof(struct input_info_network));
		}
		src->input_info->odid = src->new_odid;
	}
}

int process_message(void *config, void *message)
{
	uint32_t orig_odid, i, prevoffset, tsets = 0;
	struct joinflows_processor proc;
	struct ipfix_message *msg, *new_msg;
	struct joinflows_ip_config *conf;
	struct ipfix_template *templ;
	struct mapping *map;
	struct source *src;

	msg = (struct ipfix_message *) message;
	conf = (struct joinflows_ip_config *) config;

	orig_odid = msg->input_info->odid;

	src = joinflows_get_source(conf, orig_odid);

	if (!src) {
		MSG_DEBUG(msg_module, "[%u] No mapping, ignoring", orig_odid);
		pass_message(conf->ip_config, message);
	}

	joinflows_update_input_info(src, msg->input_info);

	if (msg->source_status == SOURCE_STATUS_CLOSED) {
		msg->input_info = src->input_info;
		pass_message(conf->ip_config, (void *) msg);
		return 0;
	}

	proc.msg = calloc(1, ntohs(msg->pkt_header->length) + 4 * (msg->data_records_count + msg->templ_records_count));
	if (!proc.msg) {
		MSG_ERROR(msg_module, "Unable to allocate memory (%s:%d)", __FILE__, __LINE__);
		return 1;
	}

	new_msg = calloc(1, sizeof(struct ipfix_message));
	if (!new_msg) {
		MSG_ERROR(msg_module, "Unable to allocate memory (%s:%d)", __FILE__, __LINE__);
		free(proc.msg);
		return 1;
	}

	memcpy(proc.msg, msg->pkt_header, IPFIX_HEADER_LENGTH);
	proc.offset = IPFIX_HEADER_LENGTH;
	proc.orig_odid = orig_odid;
	proc.src = src;
	new_msg->pkt_header = (struct ipfix_header *) proc.msg;

	for (i = 0; i < 1024 && msg->templ_set[i]; ++i) {
		prevoffset = proc.offset;
		memcpy(proc.msg + proc.offset, &(msg->templ_set[i]->header), 4);
		proc.offset += 4;
		proc.length = 4;

		template_set_process_records(msg->templ_set[i], TM_TEMPLATE, &templates_processor, (void *) &proc);

		if (proc.offset == prevoffset + 4) {
			proc.offset = prevoffset;
		} else {
			new_msg->templ_set[tsets] = (struct ipfix_template_set *) ((uint8_t *) proc.msg + prevoffset);
			new_msg->templ_set[tsets]->header.length = htons(proc.length);
			tsets++;
		}
	}

	for (i = 0; i < 1024 && msg->data_couple[i].data_set; ++i) {
		templ = msg->data_couple[i].data_template;
		if (!templ) {
			continue;
		}

		map = mapping_lookup(src->mapping, orig_odid, templ->template_id);
		if (map == NULL) {
			MSG_DEBUG(msg_module, "[%u] %d nenalezeno", orig_odid, templ->template_id);
		}

		memcpy(proc.msg + proc.offset, &(msg->data_couple[i].data_set->header), 4);
		proc.offset += 4;
		proc.length = 4;

		new_msg->data_couple[i].data_set = ((struct ipfix_data_set *) ((uint8_t *)proc.msg + proc.offset - 4));
		new_msg->data_couple[i].data_template = map->new_templ->templ;
		__sync_fetch_and_add(&(new_msg->data_couple[i].data_template->references), templ->references);
		templ->references = 0;

		data_set_process_records(msg->data_couple[i].data_set, templ, &data_processor, (void *) &proc);

		new_msg->data_couple[i].data_set->header.length = htons(proc.length);
	}

	new_msg->pkt_header->observation_domain_id = htonl(src->new_odid);
	new_msg->pkt_header->length = htons(proc.offset);
	new_msg->input_info = src->input_info;
	new_msg->templ_records_count = tsets;
	new_msg->data_records_count = msg->data_records_count;
	new_msg->source_status = msg->source_status;
	drop_message(conf->ip_config, message);
	pass_message(conf->ip_config, (void *) new_msg);
	return 0;
}

int intermediate_plugin_close(void *config)
{
	struct joinflows_ip_config *conf;
	struct mapping_header *aux_map;
	struct source *aux_src;

	conf = (struct joinflows_ip_config *) config;


	aux_src = conf->sources;
	aux_map = conf->mappings;

	while (aux_src) {
		conf->sources = conf->sources->next;
		free(aux_src);
		aux_src = conf->sources;
	}

	while (aux_map) {
		conf->mappings = conf->mappings->next;
		mapping_destroy(aux_map);
		aux_map = conf->mappings;
	}

	if (conf->default_source) {
		free(conf->default_source);
	}

	free(conf);

	return 0;
}
