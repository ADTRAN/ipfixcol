/*
 * output_manager.c
 *
 *  Created on: 6.3.2014
 *      Author: michal
 */


#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <libxml/tree.h>
#include <sys/prctl.h>

#include <ipfixcol.h>

#include "data_manager.h"


/**
 * \struct output_mm_config
 *
 * Contains all configuration of output managers' manager.
 */
struct output_manager_config {
	struct data_manager_config *data_managers;      /* output managers */
	struct data_manager_config *last;
	struct storage_list *storage_plugins;    /* list of storage structures */
	struct ring_buffer *in_queue;     /* input queue */
	pthread_t thread_id;              /* manager's thread ID */
};

char *msg_module = "output_manager";


/**
 * \brief Search for Data manager handling specified Observation Domain ID
 *
 * \todo: improve search e.g. by some kind of sorting data_managers
 *
 * @param[in] id Observation domain ID of wanted Data manager.
 * @return Desired Data manager's configuration structure if exists, NULL if
 * there is no Data manager for specified Observation domain ID
 */
static struct data_manager_config *get_data_mngmt_config (uint32_t id, struct data_manager_config *data_mngmts)
{
	struct data_manager_config *aux_cfg = data_mngmts;

	while (aux_cfg) {
		if (aux_cfg->observation_domain_id == id) {
			break;
		}
		aux_cfg = aux_cfg->next;
	}

	return (aux_cfg);
}

void output_manager_insert(struct output_manager_config *output_manager, struct data_manager_config *new_manager)
{
	new_manager->next = NULL;
	if (output_manager->last == NULL) {
		output_manager->data_managers = new_manager;
	} else {
		output_manager->last->next = new_manager;
	}
	output_manager->last = new_manager;
}

/**
 * \brief Output Managers thread
 *
 * @param[in] config configuration structure
 * @return NULL
 */
static void *output_manager_plugin_thread(void* config)
{
	struct output_manager_config *conf = NULL;
	struct data_manager_config *data_config = NULL;
	struct ipfix_message* msg = NULL;
	unsigned int index;

	conf = (struct output_manager_config *) config;
	index = conf->in_queue->read_offset;

	/* set the thread name to reflect the configuration */
	prctl(PR_SET_NAME, "om", 0, 0, 0);      /* output managers' manager */


    /* loop will break upon receiving NULL from buffer */
	while (1) {
		/* get next data */
		msg = rbuffer_read(conf->in_queue, &index);
		if (!msg) {
			MSG_NOTICE(msg_module, "No more data from core.");
            break;
		}

		/* get appropriate data manager's config according to Observation domain ID */
		data_config = get_data_mngmt_config (ntohl(msg->pkt_header->observation_domain_id), conf->data_managers);
		if (data_config == NULL) {
			/*
			 * no data manager config for this observation domain ID found -
			 * we have a new observation domain ID, so create new data manager for
			 * it
			 */
			data_config = data_manager_create(ntohl(msg->pkt_header->observation_domain_id), conf->storage_plugins);
			if (data_config == NULL) {
				MSG_WARNING(msg_module, "Unable to create data manager for Observation Domain ID %d, skipping data.",
						ntohl(msg->pkt_header->observation_domain_id));
				free (msg);
			}

		    /* add config to data_mngmts structure */
	    	output_manager_insert(conf, data_config);

	        MSG_NOTICE(msg_module, "Created new Data manager for ODID %i", ntohl(msg->pkt_header->observation_domain_id));
		}

		if (rbuffer_write(data_config->in_queue, msg, 1) != 0) {
			MSG_WARNING(msg_module, "Unable to write into Data manager's input queue, skipping data.");
			free(msg);
		}
	}

	MSG_NOTICE(msg_module, "Closing Output Manager's thread.");

	return (void *) 0;
}


/**
 * \brief Creates new Output Manager
 *
 * @param[in] storages list of storage plugin
 * @param[in] in_queue manager's input queue
 * @param[out] config configuration structure
 * @return 0 on success, negative value otherwise
 */
int output_manager_create(struct storage_list *storages, struct ring_buffer *in_queue, void **config) {

	struct output_manager_config *conf;
	int retval;

	conf = (struct output_manager_config *) malloc(sizeof(*conf));
	if (!conf) {
		MSG_ERROR(msg_module, "Memory allocation failed (%s:%d)", __FILE__, __LINE__);
		return -1;
	}
	memset(conf, 0, sizeof(*conf));

	conf->storage_plugins = storages;
	conf->in_queue = in_queue;

	conf->data_managers = NULL;
	conf->last = NULL;

	retval = pthread_create(&(conf->thread_id), NULL, &output_manager_plugin_thread, (void *) conf);
	if (retval != 0) {
		MSG_ERROR(msg_module, "Unable to create storage plugin thread.");
		goto err_thread;
	}

	*config = conf;

	return 0;

err_thread:
	free(conf);
	return -1;
}

void output_manager_close(struct output_manager_config *manager) {
	struct data_manager_config *aux_config = manager->data_managers, *tmp = NULL;
	while (aux_config) {
		tmp = aux_config;
		aux_config = aux_config->next;
		data_manager_close(&tmp);
	}

	manager->data_managers = NULL;
	manager->last = NULL;

	rbuffer_write(manager->in_queue, NULL, 1);

	pthread_join(manager->thread_id, NULL);

	rbuffer_free(manager->in_queue);
	free(manager);
}
