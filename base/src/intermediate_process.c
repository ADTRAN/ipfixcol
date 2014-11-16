/**
 * \file intermediate_process.c
 * \author Michal Srb <michal.srb@cesnet.cz>
 * \brief Intermediate Process
 *
 * Copyright (C) 2012 CESNET, z.s.p.o.
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

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/prctl.h>
#include <string.h>
#include "queues.h"
#include "intermediate_process.h"
#include <ipfixcol/intermediate.h>

static char *msg_module = "Intermediate Process";

/** configuration structure for Intermediate Process */
struct ip_config {
	struct ring_buffer *in_queue;
	struct ring_buffer *out_queue;
	char *xmldata;
	int (*intermediate_init)(char *, void *, uint32_t, struct ipfix_template_mgr *, void **);
	int (*intermediate_process_message)(void *, void *);
	int (*intermediate_close)(void *);
	void *plugin_config;
	pthread_t thread_id;
	char thread_name[16];
	unsigned int index;
	bool dropped;
};


/**
 * \brief Wait for data from input queue in loop.
 *
 * This function runs in separated thread.
 *
 * \param[in] config configuration structure
 * \return NULL
 */
void *ip_loop(void *config)
{
	struct ip_config *conf;
	struct ipfix_message *message;
	unsigned int index;

	conf = (struct ip_config *) config;

	prctl(PR_SET_NAME, conf->thread_name, 0, 0, 0);

	index = conf->in_queue->read_offset;

	/* wait for messages and process them */
	while (1) {
		/* get message from input buffer */
		message = rbuffer_read(conf->in_queue, &index);

		if (!message) {
			/* terminating mediator */
			MSG_DEBUG(msg_module, "NULL message, terminating intermediate process.");
			break;
		}
		conf->index = index;
		conf->dropped = false;
		/* process message */
		conf->intermediate_process_message(conf->plugin_config, message);

		if (!conf->dropped) {
			/* remove message from input queue, but do not free memory (it must be done later in output manager) */
			rbuffer_remove_reference(conf->in_queue, index, 0);
		}

		/* update index */
		index = (index + 1) % conf->in_queue->size;
	}

	return (void *) 0;
}


/**
 * \brief Initialize Intermediate Process.
 *
 * \param[in] in_queue input queue
 * \param[in] out_queue output queue
 * \param[in] intermediate intermediate plugin structure
 * \param[in] xmldata XML configuration for this plugin
 * \param[in] ip_id source ID for creating templates
 * \param[out] config configuration structure
 * \return 0 on success, negative value otherwise
 */
int ip_init(struct ring_buffer *in_queue, struct ring_buffer *out_queue, 
	struct intermediate *intermediate, char *xmldata, uint32_t ip_id,
	struct ipfix_template_mgr *template_mgr, void **config)
{
	struct ip_config *conf;
	int ret;

	conf = (struct ip_config *) malloc(sizeof(*conf));
	if (!conf) {
		MSG_ERROR(msg_module, "Not enough memory (%s:%d)", __FILE__, __LINE__);
		return -1;
	}
	memset(conf, 0, sizeof(*conf));

	conf->in_queue = in_queue;
	conf->out_queue = out_queue;
	conf->intermediate_init = intermediate->intermediate_init;
	conf->intermediate_process_message = intermediate->intermediate_process_message;
	conf->intermediate_close = intermediate->intermediate_close;
	conf->xmldata = xmldata;
	snprintf(conf->thread_name, 16, "%s", intermediate->thread_name);
//	conf->thread_name = intermediate->thread_name;

	conf->intermediate_init(conf->xmldata, conf, ip_id, template_mgr, &(conf->plugin_config));
	if (conf->plugin_config == NULL) {
		MSG_ERROR(msg_module, "Unable to initialize Intermediate Process");
		return -1;
	}

	/* start main thread */
	ret = pthread_create(&(conf->thread_id), NULL, ip_loop, conf);
	if (ret != 0) {
		MSG_ERROR(msg_module, "Unable to create thread for Intermediate Process");
		goto err_init;
	}

	*config = conf;

	return 0;

err_init:
	free(conf);
	return -1;
}


/**
 * \brief Pass processed IPFIX message to the output queue.
 *
 * \param[in] config configuration structure
 * \param[in] message IPFIX message
 * \return 0 on success, negative value otherwise
 */
int pass_message(void *config, struct ipfix_message *message)
{
	struct ip_config *conf;
	int ret;

	conf = (struct ip_config *) config;

	if (message == NULL) {
		MSG_WARNING(msg_module, "NULL message from intermediate plugin, skipping.");
		return 0;
	}
	ret = rbuffer_write(conf->out_queue, message, 1);

	return ret;
}


/**
 * \brief Drop IPFIX message.
 *
 * \param[in] config configuration structure
 * \param[in] message IPFIX message
 * \return 0 on success, negative value otherwise
 */
int drop_message(void *config, struct ipfix_message *message)
{
	struct ip_config *conf = (struct ip_config *) config;

	rbuffer_remove_reference(conf->in_queue, conf->index, 1);
	
	conf->dropped = true;
	return 0;
}

/**
 * \brief Close Intermediate Process
 *
 * \param[in] config configuration structure
 * \return 0 on success
 */
int ip_destroy(void *config)
{
	struct ip_config *conf;

	conf = (struct ip_config *) config;

	if (!conf) {
		return -1;
	}

	/* free XML configuration */
	if (conf->xmldata) {
		free(conf->xmldata);
	}

	/* free input queue (output queue will be freed by next intermediate process) */
	rbuffer_free(conf->in_queue);

	/* Close plugin */
	conf->intermediate_close(conf->plugin_config);

	free(conf);

	return 0;
}

/**
 * \brief Stop Intermediate Process
 *
 * \param[in] config configuration structure
 * \return 0 on success, negative value otherwise
 */
int ip_stop(void *config)
{
	struct ip_config *conf;
	void *retval;
	int ret;

	conf = (struct ip_config *) config;

	if (!conf) {
		return -1;
	}

	/* wait for thread to terminate */
	rbuffer_write(conf->in_queue, NULL, 1);
	ret = pthread_join(conf->thread_id, &retval);

	if (ret != 0) {
		MSG_DEBUG(msg_module, "pthread_join() error");
	}

	return 0;
}
