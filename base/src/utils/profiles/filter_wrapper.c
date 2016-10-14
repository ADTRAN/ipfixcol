/**
 * \file profiles/filter_wrapper.c
 * \author Imrich Štoffa <xstoff02@stud.fit.vutbr.cz>
 * \brief Wrapper of future intermediate plugin for IPFIX data filtering
 *
 * Copyright (C) 2015 CESNET, z.s.p.o.
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

#define _XOPEN_SOURCE

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include <ipfixcol.h>
#include <stdint.h>

#include "filter_wrapper.h"
#include "ffilter.h"

//TODO: Transform indentification to utilise ability to set pointer to general data in external identification
#define toGenEnId(gen, en, id) (((uint64_t)gen & 0xffff) << 48 |\
				((uint64_t)en & 0xffffffff) << 16 |\
				 (uint16_t)id)
#define toEnId(en, id) (((uint64_t)en & 0xffffffff) << 16 |\
			 (uint16_t)id)

static const char *msg_module = "ipx_filter";

enum nff_control_e {
	CTL_NA = 0x00,
	CTL_V4V6IP = 0x01,
	//TODO: solve mdata items when time comes
	CTL_MDATA_ITEM = 0x02,
	CTL_CALCULATED_ITEM = 0x04,
	CTL_FLAGS = 0x08,
	CTL_CONST_ITEM = 0x10,
	CTL_FPAIR = 0x8000,
};

enum nff_calculated_e {
	CALC_PPS = 1,
	CALC_DURATION,
	CALC_BPS,
	CALC_BPP,
	CALC_MPLS,
	CALC_MPLS_EOS,
	CALC_MPLS_EXP
};

enum nff_constant_e {
	CONST_INET = 0,
	CONST_INET6,
	CONST_END
};

uint64_t constants[CONST_END] = {
	4,
	6,
};

struct ipx_filter {
	ff_t *filter;	//internal filter representation
	void* buffer;	//buffer
};

/**
 * \brief Structure of ipfix message and record pointers
 *
 * Used to pass ipfix_message and record as one argument
 * Needed due to char* parameter in ff_data_func_t type
 */
typedef struct nff_msg_rec_s {
	struct ipfix_message* msg;
	struct ipfix_record* rec;
} nff_msg_rec_t;

/**
 * \struct nff_item_s
 * \brief Data structure that holds extra keywords and their numerical synonyms (some with extra flags)
 *
 * Pair field MUST be followed by adjacent fields, map is NULL terminated !
 */
typedef struct nff_item_s {
	const char* name;
	union {
		uint64_t en_id;
		uint64_t data;
	};
}nff_item_t;

void unpackEnId(uint64_t from, uint16_t *gen, uint32_t* en, uint16_t* id)
{
	*gen = (uint16_t)(from >> 48);
	*en = (uint32_t)(from >> 16);
	*id = (uint16_t)(from);

	return;
}

/* This map of strings and ids determines which (hopefully) synonyms of nfdump filter keywords are supported */
static struct nff_item_s nff_ipff_map[]={

	/* items contains name as inputted to filter and mapping to iana ipfix enterprise and element_id */

	//Implement default values (automatic)
	{"inet", toEnId(0, 60)},
	//{"inet", toGenEnId(CTL_CONSTANT, 60, CONST_INET6)},
	{"ipv", toEnId(0, 60)},
	{"inetfamily", toEnId(0, 60)},

	/*{"inet", toGenEnId(CTL_CONSTANT, 60, CONST_INET)},
	{"ipv4", toGenEnId(CTL_CONSTANT, 60, CONST_INET)},
	{"ipv6", toGenEnId(CTL_CONSTANT, 60, CONST_INET6)},
	 */

	{"proto", toEnId(0, 4)},

	/* for functionality reasons there are extra flags in mapping part CTL_FPAIR
	 * stands for item that maps to two other elements and mapping contain
	 * offsets relative to itself where taget items lie in map*/
	{"ip", toGenEnId(CTL_FPAIR, 1, 2)},

	/* CTL_V4V6IP flag allows filter to try to swtch to another equivalent field
	 * when IPv4 item is not present in flow */
		{"src ip", toGenEnId(CTL_V4V6IP, 0, 8)},
		{"dst ip", toGenEnId(CTL_V4V6IP, 0, 12)},

	{"net", toGenEnId(CTL_FPAIR, 1, 2)},
		{"src net", toGenEnId(CTL_V4V6IP, 0, 8)},
		{"dst net", toGenEnId(CTL_V4V6IP, 0, 12)},
	//synonym of IP
	{"host", toGenEnId(CTL_FPAIR, 1, 2)},
		{"src host", toGenEnId(CTL_V4V6IP, 0, 8)},
		{"dst host", toGenEnId(CTL_V4V6IP, 0, 12)},

	{"mask", toGenEnId(CTL_FPAIR, 1, 2)},
		{"src mask", toGenEnId(CTL_V4V6IP, 0, 9)},
		{"dst mask", toGenEnId(CTL_V4V6IP, 0, 13)},

/*
	//direct specific mapping
	{"ipv4", toGenEnId(CTL_FPAIR, 1, 2)},
		{"srcipv4", toEnId(0, 8)},
		{"dstipv4", toEnId(0, 12)},
	{"ipv6", toGenEnId(CTL_FPAIR, 1, 2)},
		{"srcipv6", toEnId(0, 27)},
		{"dstipv6", toEnId(0, 28)},
*/

	{"if", toGenEnId(CTL_FPAIR, 1, 2)},
		{"in if", toEnId(0, 10)},
		{"out if", toEnId(0, 14)},

	{"port", toGenEnId(CTL_FPAIR, 1, 2)},
		{"src port", toEnId(0, 7)},
		{"dst port", toEnId(0, 11)},

	{"icmp-type", toEnId(0, 176)},
	{"icmp-code", toEnId(0, 177)},

	{"engine-type", toEnId(0, 38)},
	{"engine-id", toEnId(0, 39)},
/*	{"sysid", toEnId(0, 177)},
*/
	{"icmp-type", toEnId(0, 176)},
	{"icmp-code", toEnId(0, 177)},

	{"as", toGenEnId(CTL_FPAIR, 1, 2)},
	{"as", toGenEnId(CTL_FPAIR, 2, 3)},
	{"as", toGenEnId(CTL_FPAIR, 3, 4)},
		{"src as", toEnId(0, 16)},
		{"dst as", toEnId(0, 17)},
		{"next as", toEnId(0, 128)}, //maps  to BGPNEXTADJACENTAS
		{"prev as", toEnId(0, 129)}, //similar as above


	{"vlan", toGenEnId(CTL_FPAIR, 1, 2)},
		{"src vlan", toEnId(0, 58)},
		{"dst vlan", toEnId(0, 59)},
	/* CTL_FLAGS Marks this to be evaluated like flag in case no operator
	 * is supplied */
	{"flags", toGenEnId(CTL_FLAGS, 0, 6)},

	{"nextip", toGenEnId(CTL_V4V6IP, 0, 15)},

	{"bgpnextip", toEnId(0, 18)},

	{"routerip", toEnId(0, 130)},

	{"mac", toGenEnId(CTL_FPAIR, 1, 2)},
	{"in mac", toGenEnId(CTL_FPAIR, 4, 5)},
	{"out mac", toGenEnId(CTL_FPAIR, 5, 6)},
	{"src mac", toGenEnId(CTL_FPAIR, 2, 4)},
	{"dst mac", toGenEnId(CTL_FPAIR, 2, 4)},
		{"in src mac", toEnId(0, 56)},
		{"in dst mac", toEnId(0, 80)},
		{"out src mac", toEnId(0, 81)},
		{"out dst mac", toEnId(0, 57)},

/*
	{"mplseos", toGenEnId(CTL_CALCULATED_ITEM, 0, CALC_MPLS_EOS)},
	{"mplsexp", toGenEnId(CTL_CALCULATED_ITEM, 0, CALC_MPLS_EXP)},
	{"mplslabel", toGenEnId(CTL_CALCULATED_ITEM, 0, CALC_MPLS)},
	{"mplslabel1", toGenEnId(CTL_CALCULATED_ITEM, 1, CALC_MPLS)},
	{"mplslabel2", toGenEnId(CTL_CALCULATED_ITEM, 2, CALC_MPLS)},
	{"mplslabel3", toGenEnId(CTL_CALCULATED_ITEM, 3, CALC_MPLS)},
	{"mplslabel4", toGenEnId(CTL_CALCULATED_ITEM, 4, CALC_MPLS)},
	{"mplslabel5", toGenEnId(CTL_CALCULATED_ITEM, 5, CALC_MPLS)},
	{"mplslabel6", toGenEnId(CTL_CALCULATED_ITEM, 6, CALC_MPLS)},
	{"mplslabel7", toGenEnId(CTL_CALCULATED_ITEM, 7, CALC_MPLS)},
	{"mplslabel8", toGenEnId(CTL_CALCULATED_ITEM, 8, CALC_MPLS)},
	{"mplslabel9", toGenEnId(CTL_CALCULATED_ITEM, 9, CALC_MPLS)},
	{"mplslabel10", toGenEnId(CTL_CALCULATED_ITEM, 10, CALC_MPLS)},
*/

	{"packets", toEnId(0, 2)},

	{"bytes", toEnId(0, 1)},

	{"flows", toEnId(0, 3)},

	{"tos", toEnId(0, 5)},
	{"src tos", toEnId(0, 5)},
	{"dst tos", toEnId(0, 55)},

	/* CTL_CALCULATED_ITEM marks specific elements, enumerated ie_id mappings
	 * are for calculated virtual fields */
	{"pps", toGenEnId(CTL_CALCULATED_ITEM, 0, CALC_PPS)},

	{"duration", toGenEnId(CTL_CALCULATED_ITEM, 0, CALC_DURATION)},

	{"bps", toGenEnId(CTL_CALCULATED_ITEM, 0, CALC_BPS)},

	{"bpp", toGenEnId(CTL_CALCULATED_ITEM, 0, CALC_BPP)},

//Not verified, for
//	{"asa event", toEnId(0, 230)},
	{"xevent", toEnId(0, 233)},
/*
	{"xip", toGenEnId(CTL_FPAIR, 1, 2)},
		{"src xip", toGenEnId(CTL_V4V6IP, 0, 225)},
		{"dst xip", toGenEnId(CTL_V4V6IP, 0, 226)},

	{"xport", toGenEnId(CTL_FPAIR, 1, 2)},
		{"src xport", toEnId(0, 227)},
		{"dst xport", toEnId(0, 228)},
*/
	{"nat event", toEnId(0, 230)},

	{"nip", toGenEnId(CTL_FPAIR, 1, 2)},
		{"src nip", toGenEnId(CTL_V4V6IP, 0, 225)},
		{"dst nip", toGenEnId(CTL_V4V6IP, 0, 226)},

	{"nport", toGenEnId(CTL_FPAIR, 1, 2)},
		{"src nport", toEnId(0, 227)},
		{"dst nport", toEnId(0, 228)},

	{"vrfid", toGenEnId(CTL_FPAIR, 1, 2)},
		{"ingress vrfid", toEnId(0, 234)},
		{"egress vrfid", toEnId(0, 235)},

	//{"tstart", toEnId(0, 152)},
	//{"tend", toEnId(0, 153)},

	/* Array is null terminated */
	{ NULL, 0U},
};

/* IANA protocol list */
static struct nff_item_s nff_proto_id_map[]={
	{ "HOPOPT",	0 },
	{ "ICMP",	1 },
	{ "IGMP",	2 },
	{ "GGP",	3 },
	{ "IPv4",	4 },
	{ "ST",		5 },
	{ "TCP",	6 },
	{ "CBT",	7 },
	{ "EGP",	8 },
	{ "IGP",	9 },
	{ "BBN-RCC-MON",	10 },
	{ "NVP-II",	11 },
	{ "PUP",	12 },
	{ "ARGUS", 	13 },
	{ "EMCON",	14 },
	{ "XNET",	15 },
	{ "CHAOS",	16 },
	{ "UDP",	17 },
	{ "MUX",	18 },
	{ "DCN-MEAS",	19 },
	{ "HMP",	20 },
	{ "PRM",	21 },
	{ "XNS-IDP",	22 },
	{ "TRUNK-1",	23 },
	{ "TRUNK-2",	24 },
	{ "LEAF-1",	25 },
	{ "LEAF-2",	26 },
	{ "RDP",	27 },
	{ "IRTP",	28 },
	{ "ISO-TP4",	29 },
	{ "NETBLT",	30 },
	{ "MFE-NSP",	31 },
	{ "MERIT-INP",	32 },
	{ "DCCP",	33 },
	{ "3PC",	34 },
	{ "IDPR",	35 },
	{ "XTP",	36 },
	{ "DDP",	37 },
	{ "IDPR-CMTP",	38 },
	{ "TP++",	39 },
	{ "IL",		40 },
	{ "SDRP",	42 },
	{ "IPv6",	41 },
	{ "IPv6-Route",	43 },
	{ "IPv6-Frag",	44 },
	{ "IDRP",	45 },
	{ "RSVP",	46 },
	{ "GRE",	47 },
	{ "DSR",	48 },
	{ "BNA",	49 },
	{ "ESP",	50 },
	{ "AH",		51 },
	{ "I-NLSP",	52 },
	{ "SWIPE", 	53 },
	{ "NARP",	54 },
	{ "MOBILE",	55 },
	{ "TLSP",	56 },
	{ "SKIP",	57 },
	{ "IPv6-ICMP",	58 },
	{ "ICMP6",	58 },
	{ "IPv6-NoNxt",	59 },
	{ "IPv6-Opts",	60 },
	{ "CFTP",	62 },
	{ "SAT-EXPAK",	64 },
	{ "KRYPTOLAN",	65 },
	{ "RVD",	66 },
	{ "IPPC",	67 },
	{ "SAT-MON",	69 },
	{ "VISA",	70 },
	{ "IPCV",	71 },
	{ "CPNX",	72 },
	{ "CPHB",	73 },
	{ "WSN",	74 },
	{ "PVP",	75 },
	{ "BR-SAT-MON",	76 },
	{ "SUN-ND",	77 },
	{ "WB-MON",	78 },
	{ "WB-EXPAK",	79 },
	{ "ISO-IP",	80 },
	{ "VMTP",	81 },
	{ "SECURE-VMTP",	82 },
	{ "VINES",	83 },
	{ "TTP",	84 },
	{ "IPTM",	84 },
	{ "NSFNET-IGP",	85 },
	{ "DGP",	86 },
	{ "TCF",	87 },
	{ "EIGRP",	88 },
	{ "OSPFIGP",	89 },
	{ "Sprite-RPC",	90 },
	{ "LARP",	91 },
	{ "MTP",	92 },
	{ "AX.25",	93 },
	{ "IPIP",	94 },
	{ "MICP", 	95 },
	{ "SCC-SP",	96 },
	{ "ETHERIP",	97 },
	{ "ENCAP",	98 },
	{ "GMTP",	100 },
	{ "IFMP",	101 },
	{ "PNNI",	102 },
	{ "PIM",	103 },
	{ "ARIS",	104 },
	{ "SCPS",	105 },
	{ "QNX",	106 },
	{ "A/N",	107 },
	{ "IPComp",	108 },
	{ "SNP",	109 },
	{ "Compaq-Peer",	110 },
	{ "IPX-in-IP",	111 },
	{ "VRRP",	112 },
	{ "PGM",	113 },
	{ "L2TP",	115 },
	{ "DDX",	116 },
	{ "IATP",	117 },
	{ "STP",	118 },
	{ "SRP",	119 },
	{ "UTI",	120 },
	{ "SMP",	121 },
	{ "SM", 	122 },
	{ "PTP",	123 },
	{ "ISIS-over-IPv4",	124 },
	{ "FIRE",	125 },
	{ "CRTP",	126 },
	{ "CRUDP",	127 },
	{ "SSCOPMCE",	128 },
	{ "IPLT",	129 },
	{ "SPS",	130 },
	{ "PIPE",	131 },
	{ "SCTP",	132 },
	{ "FC",		133 },
	{ "RSVP-E2E-IGNORE",	134 },
	{ "Mobility-Header",	135 },
	{ "UDPLite",	136 },
	{ "MPLS-in-IP",	137 },
	{ "manet",	138 },
	{ "HIP",	139 },
	{ "Shim6",	140 },
	{ "WESP",	141 },
	{ "ROHC",	142 },
	{ NULL, 	0U }
};

/* IANA assigned port names */
static struct nff_item_s nff_port_map[]={
	{ "tcpmux",	1 },
	{ "compressnet",3 },
	{ "rje",	5 },
	{ "echo",	7 },
	{ "discard",	9 },
	{ "systat",	11 },
	{ "daytime",	13 },
	{ "qotd",	17 },
	{ "msp",	18 },
	{ "chargen",	19 },
	{ "ftp-data",	20 },
	{ "ftp",	21 },
	{ "ssh",	22 },
	{ "telnet",	23 },
	{ "smtp",	25 },
	{ "nsw-fe",	27 },
	{ "msg-icp",	29 },
	{ "msg-auth",	31 },
	{ "dsp",	33 },
	{ "time",	37 },
	{ "rap",	38 },
	{ "rlp",	39 },
	{ "graphics",	41 },
	{ "name",	42 },
	{ "nameserver",	42 },
	{ "nicname",	43 },
	{ "mpm-flags",	44 },
	{ "mpm",	45 },
	{ "mpm-snd",	46 },
	{ "http",	80 },
	{ "https",	443 },
	{ NULL, 	0U }
};

/**
 * \brief specify_ipv switches information_element to equivalent from ipv4
 * to ipv6 and vice versa
 *
 * \param i Element Id
 * \return Nonzero on success
 */
int specify_ipv(uint16_t *i)
{
	switch(*i)
	{
	//src ip
	case 8: *i = 27; break;
	case 27: *i = 8; break;
	//dst ip
	case 12: *i = 28; break;
	case 28: *i = 12; break;
	//src mask
	case 9: *i = 29; break;
	case 29: *i = 9; break;
	//dst mask
	case 13: *i = 30; break;
	case 30: *i = 13; break;
	//nexthop ip
	case 15: *i = 62; break;
	case 62: *i = 15; break;
	//bgpnext ip
	case 18: *i = 63; break;
	case 63: *i = 18; break;
	//router ip
	case 130: *i = 131; break;
	case 131: *i = 130; break;
	//src xlate ip
	case 225: *i = 281; break;
	case 281: *i = 225; break;
	//dst xlate ip
	case 226: *i = 282; break;
	case 282: *i = 226; break;
	default:
		return 0;
	}
	return 1;
}

/**
 * \brief set_external_ids
 * \param[in] item
 * \param lvalue
 * \return number of ids set
 */
int set_external_ids(nff_item_t *item, ff_lvalue_t *lvalue)
{
	uint16_t gen, of2;
	uint32_t of1;
	unpackEnId(item->en_id, &gen, &of1, &of2);

	int ids = 0;

	if (gen & CTL_FPAIR) {
		ids += set_external_ids(item+of1, lvalue);
		ids += set_external_ids(item+of2, lvalue);
		lvalue->options |= FF_OPTS_MULTINODE;
		return ids;
	}

	if (gen & CTL_FLAGS) {
		lvalue->options |= FF_OPTS_FLAGS;
	}

	while(ids < FF_MULTINODE_MAX && lvalue->id[ids].index) {
		ids++;
	}
	if (ids < FF_MULTINODE_MAX) {
		lvalue->id[ids].index = item->en_id;
	}

	return ids;
}


/* callback from ffilter to lookup field */
ff_error_t ipf_lookup_func(ff_t *filter, const char *fieldstr, ff_lvalue_t *lvalue)
{
	/* fieldstr is set - try to find field id and relevant function */
	nff_item_t* item = NULL;
	const ipfix_element_t * elem;
	for(int x = 0; nff_ipff_map[x].name != NULL; x++){
		if(!strcmp(fieldstr, nff_ipff_map[x].name)){
			item = &nff_ipff_map[x];
			break;
		}
	}
	if(item == NULL) {	//Polozka nenajdena
		//potrebujem prekodovat nazov pola na en a id
		const ipfix_element_result_t elemr = get_element_by_name(fieldstr, false);
		if (elemr.result == NULL){
			ff_set_error(filter, "\"%s\" element item not found", fieldstr);
			return FF_ERR_OTHER_MSG;
		}

		lvalue->id[0].index = toEnId(elemr.result->en, elemr.result->id);
		lvalue->id[1].index = 0;
		elem = elemr.result;
	} else {
		lvalue->id[1].index = 0;

		uint16_t gen, id;
		uint32_t enterprise;

		set_external_ids(item, lvalue);

		unpackEnId(lvalue->id[0].index, &gen, &enterprise, &id);

		//This sets bad type when header or metadata items are selected
		//TODO: Test that works
		if (gen & CTL_CALCULATED_ITEM) {
			switch(id) {
			case CALC_MPLS:
				lvalue->n = enterprise;
				if (enterprise != 0) {
					lvalue->options |= FF_OPTS_MPLS_LABEL;
				}
				if (enterprise > 10) {
					return FF_ERR_OTHER_MSG;
				}
				lvalue->type = FF_TYPE_MPLS;
				break;
			case CALC_MPLS_EOS:
				lvalue->options |= FF_OPTS_MPLS_EOS;
				lvalue->type = FF_TYPE_MPLS;
				break;
			case CALC_MPLS_EXP:
				lvalue->options |= FF_OPTS_MPLS_EXP;
				lvalue->type = FF_TYPE_MPLS;
				break;
			default:
				lvalue->type = FF_TYPE_UNSIGNED;
			}
			return FF_OK;

		/*} else if (gen & CTL_CONST_ITEM) {
			lvalue->type = FF_TYPE_UNSUPPORTED;
			ff_set_error(filter, "ipfix metadata item %s has unsupported format", fieldstr);
			return FF_ERR_OTHER_MSG;
		*/} else {
			elem = get_element_by_id(id, enterprise);
		}
	}

	//Rozhodni datovy typ pre filter
	//TODO: solve conflicting types
	switch(elem->type){

		case ET_BOOLEAN:
		case ET_UNSIGNED_8:
		case ET_UNSIGNED_16:
		case ET_UNSIGNED_32:
		case ET_UNSIGNED_64:
			lvalue->type = FF_TYPE_UNSIGNED_BIG;
			break;

		case ET_SIGNED_8:
		case ET_SIGNED_16:
		case ET_SIGNED_32:
		case ET_SIGNED_64:
			lvalue->type = FF_TYPE_SIGNED_BIG;
			break;

		case ET_FLOAT_64:
			lvalue->type = FF_TYPE_DOUBLE;
			break;

		case ET_MAC_ADDRESS:
			lvalue->type = FF_TYPE_MAC;
			break;

		case ET_STRING:
			lvalue->type = FF_TYPE_STRING;
			break;

		case ET_DATE_TIME_MILLISECONDS:
			lvalue->type = FF_TYPE_TIMESTAMP;
			break;

		case ET_IPV4_ADDRESS:
		case ET_IPV6_ADDRESS:
			lvalue->type = FF_TYPE_ADDR;
			break;

		case ET_DATE_TIME_SECONDS:
		case ET_DATE_TIME_MICROSECONDS:
		case ET_DATE_TIME_NANOSECONDS:
		case ET_FLOAT_32:
		case ET_OCTET_ARRAY:
		case ET_BASIC_LIST:
		case ET_SUB_TEMPLATE_LIST:
		case ET_SUB_TEMPLATE_MULTILIST:
		case ET_UNASSIGNED:
		default:
			lvalue->type = FF_TYPE_UNSUPPORTED;
			ff_set_error(filter, "ipfix item \"%s\" has unsupported format", fieldstr);
			return FF_ERR_OTHER_MSG;
	}
	return FF_OK;
}

/* Flow duration calculation function */
ff_uint64_t calc_record_duration(uint8_t *record, struct ipfix_template *templ);

/* getting data callback */
ff_error_t ipf_data_func(ff_t *filter, void *rec, ff_extern_id_t id, char *data, size_t *size)
{
	//assuming rec is struct ipfix_message
	struct nff_msg_rec_s* msg_pair = rec;
	int len;
	uint64_t tmp;
	char *ipf_field;
	ff_mpls_label_t mpls_stack[10];

	uint32_t en;
	uint16_t ie_id;
	uint16_t generic_set;
	unpackEnId(id.index, &generic_set, &en, &ie_id);

	if (generic_set & CTL_MDATA_ITEM) {

		return FF_ERR_OTHER;

	} else if (generic_set & CTL_CALCULATED_ITEM) {
		ff_uint64_t flow_duration;
		ff_uint64_t tmp, tmp2;

		//TODO: add mpls handlers
		//TODO: check that memcpy construction works
		switch (ie_id) {
		case CALC_PPS:
			flow_duration = calc_record_duration(msg_pair->rec->record, msg_pair->rec->templ);
			if (!flow_duration) { return FF_ERR_OTHER; }

			ipf_field = data_record_get_field((msg_pair->rec)->record, (msg_pair->rec)->templ, 0, 2, &len);
			if (!len) { return FF_ERR_OTHER; }
			memcpy(&tmp, ipf_field, len); //Not sure if this construction works for all lenghts

			tmp = ((ntohll(tmp) * 1000) / flow_duration);
			len = sizeof(tmp);

			break;
		case CALC_DURATION:
			tmp = calc_record_duration(msg_pair->rec->record, msg_pair->rec->templ);
			if (!tmp) { return FF_ERR_OTHER; }

			len = sizeof(tmp);

			break;
		case CALC_BPS:
			flow_duration = calc_record_duration(msg_pair->rec->record, msg_pair->rec->templ);
			if (!len) { return FF_ERR_OTHER; }
			ipf_field = data_record_get_field((msg_pair->rec)->record, (msg_pair->rec)->templ, 0, 1, &len);
			if (!len) { return FF_ERR_OTHER; }
			memcpy(&tmp, ipf_field, len);

			tmp = ((ntohll(tmp) * 1000) / flow_duration);
			len = sizeof(tmp);

			break;
		case CALC_BPP: ipf_field = data_record_get_field((msg_pair->rec)->record, (msg_pair->rec)->templ, 0, 1, &len);
			if (!len) { return FF_ERR_OTHER; }
			memcpy(&tmp, ipf_field, len);

			ipf_field = data_record_get_field((msg_pair->rec)->record, (msg_pair->rec)->templ, 0, 2, &len);
			if (!len) { return FF_ERR_OTHER; }
			memcpy(&tmp2, ipf_field, len);

			tmp = ntohll(tmp) / ntohll(tmp2);
			len = sizeof(tmp);

			break;
		case CALC_MPLS:
		case CALC_MPLS_EXP:
		case CALC_MPLS_EOS:
			memset(&mpls_stack[0], 0, sizeof(ff_mpls_stack_t));
			for(tmp = 0; tmp < 10; tmp++){
				ipf_field = data_record_get_field((msg_pair->rec)->record, (msg_pair->rec)->templ, 0, 70+tmp, &len);
				if(ipf_field != NULL) {
					memcpy(&mpls_stack[tmp].data, ipf_field, len);
				}
			}
			ipf_field = &mpls_stack[0];

			break;
		default: return FF_ERR_OTHER;
		}
		ipf_field = &tmp;
	} else {

		ipf_field = data_record_get_field((msg_pair->rec)->record, (msg_pair->rec)->templ, en, ie_id, &len);
		if (ipf_field == NULL && generic_set & CTL_V4V6IP) {
			if (specify_ipv(&ie_id)) {
				ipf_field = data_record_get_field((msg_pair->rec)->record, (msg_pair->rec)->templ, en, ie_id, &len);
			}
		}
		if (ipf_field == NULL) {
			return FF_ERR_OTHER;
		}
	}

	memcpy(data, ipf_field, len);

	*size = len;
	return FF_OK;
}

ff_error_t ipf_rval_map_func(ff_t *filter, const char *valstr, ff_type_t type, ff_extern_id_t id, char *buf, size_t *size)
{
	struct nff_item_s *dict = NULL;
	char *tcp_ctl_bits = "FSRPAUECNX";
	char *hit = NULL;
	*size = 0;

	uint32_t en;
	uint16_t ie_id;
	uint16_t generic_set;
	unpackEnId(id.index, &generic_set, &en, &ie_id);

	if (en != 0 || valstr == NULL) {
		return FF_ERR_OTHER;
	}

	int x;
	if (type == FF_TYPE_UNSIGNED_BIG || type == FF_TYPE_UINT64) {

		*size = sizeof(ff_uint64_t);
		ff_uint64_t val;

		switch (ie_id) {
		default:
			return FF_ERR_UNSUP;
			break;
		case 4:
			dict = &nff_proto_id_map[0];
			break;

			/* Translate tcpControlFlags */
		case 6:
			if (strlen(valstr)>9) {
				return FF_ERR_OTHER;
			}

			for (x = val = 0; x < strlen(valstr); x++) {
				if ((hit = strchr(tcp_ctl_bits, valstr[x])) == NULL) {
					return FF_ERR_OTHER;
				}
				val |= 1 << (hit - tcp_ctl_bits);
				/* If X was in string set all flags */
				if (*hit == 'X') {
					val = 1 << (hit - tcp_ctl_bits);
					val--;
				}
			}
			memcpy(buf, &val, sizeof(val));
			return FF_OK;
			break;
		case 7:
		case 11:
			dict = &nff_port_map[0];
			break;
		}


		nff_item_t *item = NULL;
		const ipfix_element_t *elem;

		for (int x = 0; dict[x].name != NULL; x++) {
			if (!strcasecmp(valstr, dict[x].name)) {
				item = &dict[x];
				break;
			}
		}

		if (item != NULL) {
			memcpy(buf, &item->data, sizeof(item->data));
			*size = sizeof(item->data);
			return FF_OK;
		}
	}

	return FF_ERR_OTHER;
}

ff_uint64_t calc_record_duration(uint8_t *record, struct ipfix_template *templ)
{
	int len;
	char *ipf_data = NULL;
	ff_uint64_t tend, tstart;
	tend = tstart = 0;

	ipf_data = data_record_get_field(record, templ, 0, 153, &len);
	if (len) {
		memcpy(&tend, ipf_data, len);
		tend = ntohll(tend);
		ipf_data = data_record_get_field(record, templ, 0, 152, &len);
		if (len) {
			memcpy(&tstart, ipf_data, len);
			tstart = ntohll(tstart);
		} else { return 0; }
	} else { return 0; }

	return abs(tend - tstart);
}

/* Constructor */
ipx_filter_t *ipx_filter_create()
{
	ipx_filter_t *filter = NULL;

	if ((filter = calloc(1, sizeof(ipx_filter_t))) == NULL) {
		return NULL;
	}

	if ((filter->buffer = malloc(FF_MAX_STRING * sizeof(char))) == NULL) {
		free(filter);
		return NULL;
	}

	return filter;
}

/* Memory release function */
void ipx_filter_free(ipx_filter_t *filter)
{
	if (filter != NULL) {
		ff_free(filter->filter);
		free(filter->buffer);
	}
	free(filter);
}

int ipx_filter_parse(ipx_filter_t *filter, char* filter_str)
{
	//vytvorenie options - lookup fc data-callback fc
	int retval = 0;
	ff_options_t *opts = NULL;

	if (ff_options_init(&opts) == FF_ERR_NOMEM) {
		ff_set_error(filter->filter, "Memory allocation for options failed");
		return 1;
	}

	opts->ff_lookup_func = ipf_lookup_func;
	opts->ff_data_func = ipf_data_func;
	opts->ff_rval_map_func = ipf_rval_map_func;

	if (ff_init(&filter->filter, filter_str, opts) != FF_OK) {
		retval = 1;
	}

	ff_options_free(opts);

	return retval;
}

/* Evaulate expresion tree */
int ipx_filter_eval(ipx_filter_t *filter, struct ipfix_message *msg, struct ipfix_record *record)
{
	struct nff_msg_rec_s pack;
	pack.msg = msg;
	pack.rec = record;
	/* Necesarry to pass both msg and record to ff_eval, passed structure that contains both */
	return ff_eval(filter->filter, &pack);
}

char *ipx_filter_get_error(ipx_filter_t *filter)
{
	ff_error(filter->filter, (char *) filter->buffer, FF_MAX_STRING);
	((char *) filter->buffer)[FF_MAX_STRING - 1] = 0;

	return (char *) filter->buffer;
}
