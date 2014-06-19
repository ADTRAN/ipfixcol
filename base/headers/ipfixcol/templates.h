/**
 * \file templates.h
 * \author Radek Krejci <rkrejci@cesnet.cz>
 * \author Michal Srb <michal.srb@cesnet.cz>
 * \brief Public structures and functions (API) of the ipfixcol's Template
 * Manager
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

/**
 * \defgroup templateMngAPI Template Manager API
 * \ingroup publicAPIs
 *
 * These functions should be used to work with Template Manager. Template
 * Manager is unique for every Data Manager and is represented by \link
 * #ipfix_template_mgr_t ipfix_template_mgr_t structure\endlink. Its job
 * is to manage (Options) Templates.
 *
 *
 * @{
 */
#ifndef IPFIXCOL_TEMPLATES_H_
#define IPFIXCOL_TEMPLATES_H_

/**
 * \def TM_OPTIONS_TEMPLATE
 * \brief Template manager's options template number
 */
#define TM_OPTIONS_TEMPLATE 1

/**
 * \def TM_TEMPLATE
 * \brief Template manager's template number
 */
#define TM_TEMPLATE 0

/**
 * \def TM_UDP_TIMEOUT
 * \brief Specifies Default template timeout for UDP
 */
#define TM_UDP_TIMEOUT 1800

/**
 * \def TM_TEMPLATE_WITHDRAW_LEN
 * \brief Length of withdraw template in octets
 */
#define TM_TEMPLATE_WITHDRAW_LEN 4

/**
 * \struct ipfix_template
 * \brief Structure for storing Template Record/Options Template Record
 *
 * All data in this structure are in host byte order
 */
struct ipfix_template {
	uint16_t original_id;			 /** Original template ID */
	uint32_t references;		 /** Number of packets referencing to this template */
	struct ipfix_template *next; /** Pointer to older template with the same template id */

	uint8_t template_type;       /**Type of Template - TM_TEMPLATE = Template,
	                              * TM_OPTIONS_TEMPLATE = Options Template */
	time_t last_transmission;    /**Time of last transmission of Template,
	                              * UDP only */
	uint32_t last_message;		 /** Message number of last update,
	 	 	 	 	 	 	 	  * UDP only */
	uint16_t template_id;        /**Template ID given by collector */
	uint16_t field_count;        /**Number of fields in Template Record */
	uint16_t scope_field_count;  /**Number of scope fields */
	uint16_t template_length;    /**Length of the template. This is size
	                              * of the template structure and actual template
	                              * fields.
	                              * sizeof(struct ipfix_template) - sizeof(template_ie)
	                              * + length of the template fields */
	uint32_t data_length;        /**Length of the data record specified
	                              * by this template. If the most significant
	                              * bit is set to 1, then there is at least
	                              * one Information Element with variable length.
	                              * In such case this value is invalid and true
	                              * length of the Data Record has to be
	                              * calculated somehow else. For more information,
	                              * see section 7 in RFC 5101. */
	template_ie fields[1];       /**Template fields */
};


struct released_tid {
	uint16_t tid;
	struct released_tid *next;
};

/**
 * \struct ipfix_free_tid
 * \brief Structure for storing free Template IDs for ODID
 */
struct ipfix_free_tid {
	struct released_tid *released; /** Released Template IDs */
	uint32_t odid;     /** Observation Domain ID */
	uint16_t tid;      /** Free Template ID */
};

/**
 * \struct ipfix_template_mgr
 * \brief Template Manager structure.
 */
struct ipfix_template_mgr {
	int free_tid_cnt;     /** Number of ipfix_free_tid structures */
	int free_tid_max;     /** Maximum number of ipfix_free_tid structures (allocated space) */
	struct ipfix_free_tid **free_tid;  /** Array of free Template IDs */
	struct ipfix_template_mgr_record *first; /** list of template manager's record for each source */
	struct ipfix_template_mgr_record *last;  /** last member of list */
};

/**
 * \struct ipfix_template_key
 * \brief Unique identifier of template in Template Manager
 */
struct ipfix_template_key {
	uint32_t odid; 	/** Observation Domain ID */
	uint32_t crc;  	/** CRC from source IP address */
	uint32_t tid;	/** Template ID */
};

/**
 * \struct ipfix_template_mgr_record
 * \brief Record of Template Manager's structure
 */
struct ipfix_template_mgr_record {
	struct ipfix_template **templates;/**array of pointers to Templates */
	uint16_t max_length;  /**maximum length of array */
	uint16_t counter;     /**number of templates in array */
	uint64_t key;		  /** unique identifier (combination of odid and crc from ipfix_template_key) */
	struct ipfix_template_mgr_record *next; /** pointer to next record in template manager's list */
};


/**
 * \brief Function for creating new template
 *
 * \param[in]  tmp Pointer where new Template Record starts.
 * \param[in]  max_len Maximum length of the template. Typically length
 * to the end of the Template Set.
 * \param[in]  type Type of the Template Record. TM_TEMPLATE = Template,
 * TM_OPTIONS_TEMPLATE = Options Template.
 * \return Pointer to new ipfix_template on success, NULL otherwise
 */
struct ipfix_template *tm_create_template(void *tmp, int max_len, int type);


/**
 * \brief Function for adding new templates.
 *
 * \param[in]  tm Template Manager
 * \param[in]  tmp Pointer where new Template Record starts.
 * \param[in]  max_len Maximum length of the template. Typically length
 * to the end of the Template Set.
 * \param[in]  type Type of the Template Record. TM_TEMPLATE = Template,
 * TM_OPTIONS_TEMPLATE = Options Template.
 * \param[in]  key Unique identifier of template in Template Manager
 * \return Pointer to new ipfix_template on success, NULL otherwise
 */
struct ipfix_template *tm_add_template(struct ipfix_template_mgr *tm,
                                          void *tmp, int max_len, int type, struct ipfix_template_key *key);


/**
 * \brief Insert existing template into Template Manager
 *
 * \param[in] tm Template Manager
 * \param[in] tmpl Existing IPFIX Template
 * \param[in] key Unique identifier of template in Template Manager
 */
struct ipfix_template *tm_insert_template(struct ipfix_template_mgr *tm, struct ipfix_template *tmpl, struct ipfix_template_key *key);

/**
 * \brief Function for updating an existing templates.
 *
 * \param[in]  tm Template Manager
 * \param[in]  tmp Pointer where new Template Record starts.
 * \param[in]  max_len Maximum length of the template. Typically length
 * to the end of the Template Set.
 * \param[in]  type Type of the Template Record. TM_TEMPLATE = Template,
 * TM_OPTIONS_TEMPLATE = Options Template.
 * \param[in]  key Unique identifier of template in Template Manager
 * \return updated ipfix_template on success, NULL if error occurs.
 */
struct ipfix_template *tm_update_template(struct ipfix_template_mgr *tm,
                                          void *tmp, int max_len, int type, struct ipfix_template_key *key);

/**
 * \brief Function for specific Template lookup.
 *
 * \param[in]  tm Template Manager
 * \param[in]  key Unique identifier of template in Template Manager
 * \return pointer on the Temaplate on success, NULL if there is no such
 * Template.
 */
struct ipfix_template *tm_get_template(struct ipfix_template_mgr *tm, struct ipfix_template_key *key);

/**
 * \brief Function for removing Temaplates.
 *
 * \param[in]  tm Template Manager
 * \param[in]  key Unique identifier of template in Template Manager
 * \return 0 on success, negative value otherwise.
 */
int tm_remove_template(struct ipfix_template_mgr *tm, struct ipfix_template_key *key);

/**
 * \brief Function for removing all Temaplates of specific type.
 *
 * \param[in]  tm Template Manager
 * \param[in]  type type of the template to withdraw. TM_TEMPLATE = Template,
 * TM_OPTIONS_TEMPLATE = Options Template.
 * \return 0 on success, negative value otherwise.
 */
int tm_remove_all_templates(struct ipfix_template_mgr *tm, int type);

/**
 * \brief Remove all templates for ODID
 *
 * \param[in] tm Template Manager
 * \param[in] odid Observation Domain ID
 */
void tm_remove_all_odid_templates(struct ipfix_template_mgr *tm, uint32_t odid);

/**
 * \brief Create new template manager and set default values
 *
 * \return struct ipfix_template_mgr New template manager
 */
struct ipfix_template_mgr *tm_create();

/**
 * \brief Determines whether specific template contains given field
 *
 * \param[in]  templ template
 * \param[in]  field field
 * \return 0 on success, negative value otherwise.
 */
int template_contains_field(struct ipfix_template *templ, uint16_t field);


/**
 * \brief Increment number of references to template
 *
 * \param[in] templ template
 */
void tm_template_reference_inc(struct ipfix_template *templ);

/**
 * \brief Decrement number of references to template
 *
 * \param[in] templ template
 */
void tm_template_reference_dec(struct ipfix_template *templ);

/**
 * \brief Destroys and frees specified template manager
 *
 * \param[in] ipfix_template_mgr Template manager to be destroyed
 */
void tm_destroy(struct ipfix_template_mgr *tm);

/**
 * \brief Make ipfix_template_key from ODID, crc and template id
 * 
 * @param odid Observation Domain ID
 * @param crc  CRC from source IP and source port
 * @param tid  Template ID
 * @return pointer to ipfix_template_key
 */
struct ipfix_template_key *tm_key_create(uint32_t odid, uint32_t crc, uint32_t tid);

/**
 * \brief Change Template ID in template_key
 *
 * @param key Template identifier in Template Manager
 * @param tid New Template ID
 * @return pointer to changed ipfix_template_key
 */
struct ipfix_template_key *tm_key_change_template_id(struct ipfix_template_key *key, uint32_t tid);

/**
 * \brief Destroy ipfix_template_key structure
 * 
 * @param key IPFIX template key
 */
void tm_key_destroy(struct ipfix_template_key *key);


#endif /* IPFIXCOL_TEMPLATES_H_ */

/**@}*/
