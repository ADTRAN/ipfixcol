

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <commlbr.h>
//#include <sys/types.h>
#include <unistd.h>
#include <dlfcn.h>
#include <time.h>

#include "nffile.h"
#include "../../../headers/storage.h"

void hex(void *ptr, int size){
	int i,space = 0;
	for(i=1;i<size+1;i++){
	        if(!((i-1)%16)){
	                fprintf(stderr,"%p  ", &((char *)ptr)[i-1]);
	        }
	        fprintf(stderr,"%02hhx",((char *)ptr)[i-1]);
	        if(!(i%8)){
	                if(space){
	                        fprintf(stderr,"\n");
	                        space = 0;
	                        continue;
	                }
	                fprintf(stderr," ");
	                space = 1;
	        }
	        fprintf(stderr," ");
	}
	fprintf(stderr,"\n");
}

struct extension{
	uint16_t *value; //map array
	int values_count;
	int id;
};

struct extensions{
	int filled;
	int size;
	struct extension *map;
};

struct storage{
	int x;
};

//EXTENSION 0 -- not a real extension its just pading ect
void ext0_parse(uint32_t *data, int *offset, uint8_t flags, struct storage *s){
	VERBOSE(CL_VERBOSE_ADVANCED,"\tZERO EXTENSION");
}

//EXTENSION 1	
void ext1_parse(uint32_t *data, int *offset, uint8_t flags, struct storage *s){
	if(TestFlag(flags, FLAG_IPV6_ADDR)){
		VERBOSE(CL_VERBOSE_ADVANCED,"\tIPv6-SRC: hight:%lu low:%lu",*((uint64_t *) &data[*offset]), \
			*((uint64_t *) &data[(*offset)+2]));
			*offset+=4;

		VERBOSE(CL_VERBOSE_ADVANCED,"\tIPv6-DST: hight:%lu low:%lu",*((uint64_t *) &data[*offset]), \
			*((uint64_t *) &data[(*offset)+2]));
			*offset+=4;
	} else {
		VERBOSE(CL_VERBOSE_ADVANCED,"\tIPv4-SRC: %u", *((uint32_t *) &data[*offset]));
		(*offset)++;
		VERBOSE(CL_VERBOSE_ADVANCED,"\tIPv4-DST: %u", *((uint32_t *) &data[*offset]));
		(*offset)++;
	}
}

//EXTENSION 2
void ext2_parse(uint32_t *data, int *offset, uint8_t flags, struct storage *s){
	if(TestFlag(flags, FLAG_PKG_64)){
		VERBOSE(CL_VERBOSE_ADVANCED,"\tPACKET COUNTER: %lu", *((uint64_t *) &data[*offset]));
		*offset+=2;
	} else {
		VERBOSE(CL_VERBOSE_ADVANCED,"\tPACKET COUNTER: %u", *((uint32_t *) &data[*offset]));
		(*offset)++;
	}
}

//EXTENSION 3
void ext3_parse(uint32_t *data, int *offset, uint8_t flags, struct storage *s){
	if(TestFlag(flags, FLAG_BYTES_64)){
		VERBOSE(CL_VERBOSE_ADVANCED,"\tBYTE COUNTER: %lu", *((uint64_t *) &data[*offset]));
		*offset+=2;
	} else {
		VERBOSE(CL_VERBOSE_ADVANCED,"\tBYTE COUNTER: %u", *((uint32_t *) &data[*offset]));
		(*offset)++;
	}
}

//OPTIONAL EXTENSIONS
//EXTENSION 4 - interface record (16b ints)
void ext4_parse(uint32_t *data, int *offset, uint8_t flags, struct storage *s){
	VERBOSE(CL_VERBOSE_ADVANCED,"\tINTERFACE RECORD INPUT: %hu (16b)", *((uint16_t *) &data[*offset]));
	VERBOSE(CL_VERBOSE_ADVANCED,"\tINTERFACE RECORD OUTPUT: %hu (16b)", *(((uint16_t *) &data[*offset])+1));
	(*offset)++;
	
}

//EXTENSION 5 - interface record (32b ints)
void ext5_parse(uint32_t *data, int *offset, uint8_t flags, struct storage *s){
	VERBOSE(CL_VERBOSE_ADVANCED,"\tINTERFACE RECORD INPUT: %u (32b)", *((uint32_t *) &data[*offset]));
	(*offset)++;
	VERBOSE(CL_VERBOSE_ADVANCED,"\tINTERFACE RECORD OUTPUT: %u (32b)", *((uint32_t *) &data[*offset]));
	(*offset)++;
	
}

//OPTIONAL EXTENSIONS
//EXTENSION 6 - AS record (16b ints)
void ext6_parse(uint32_t *data, int *offset, uint8_t flags, struct storage *s){
	VERBOSE(CL_VERBOSE_ADVANCED,"\tAS-SRC: %hu (16b)", *((uint16_t *) &data[*offset]));
	VERBOSE(CL_VERBOSE_ADVANCED,"\tAS-DST: %hu (16b)", *(((uint16_t *) &data[*offset])+1));
	(*offset)++;
	
}

//EXTENSION 7 - AS record (32b ints)
void ext7_parse(uint32_t *data, int *offset, uint8_t flags, struct storage *s){
	VERBOSE(CL_VERBOSE_ADVANCED,"\tAS-SRC: %u (32b)", *((uint32_t *) &data[*offset]));
	(*offset)++;
	VERBOSE(CL_VERBOSE_ADVANCED,"\tAS-DST: %u (32b)", *((uint32_t *) &data[*offset]));
	(*offset)++;
	
}

//EXTENSION 8 - dst tos, dir, srcmask, dstmask in one32b int
void ext8_parse(uint32_t *data, int *offset, uint8_t flags, struct storage *s){
	VERBOSE(CL_VERBOSE_ADVANCED,"\tDST-TOS: %hhu (8b)", *((uint8_t *) &data[*offset]));
	VERBOSE(CL_VERBOSE_ADVANCED,"\tDIR: %hhu (8b)", *(((uint8_t *) &data[*offset])+1));
	VERBOSE(CL_VERBOSE_ADVANCED,"\tSRC-MASK: %hhu (8b)", *(((uint8_t *) &data[*offset])+2));
	VERBOSE(CL_VERBOSE_ADVANCED,"\tDST-MASK: %hhu (8b)", *(((uint8_t *) &data[*offset])+3));
	(*offset)++;
	
}

//EXTENSION 9 - next hop ipv4
void ext9_parse(uint32_t *data, int *offset, uint8_t flags, struct storage *s){
	VERBOSE(CL_VERBOSE_ADVANCED,"\tNEXT-HOP: %u (ipv4)", *((uint32_t *) &data[*offset]));
	(*offset)++;
	
}


//EXTENSION 10 - next hop ipv6
void ext10_parse(uint32_t *data, int *offset, uint8_t flags, struct storage *s){
	
	VERBOSE(CL_VERBOSE_ADVANCED,"\tNEXT-HOP: hight:%lu low:%lu (ipv6)",*((uint64_t *) &data[*offset]), \
		*((uint64_t *) &data[(*offset)+8]));
	(*offset)+=4;
}


//EXTENSION 11 - BGP next hop ipv4
void ext11_parse(uint32_t *data, int *offset, uint8_t flags, struct storage *s){
	VERBOSE(CL_VERBOSE_ADVANCED,"\tBGP-NEXT-HOP: %u (ipv4)", *((uint32_t *) &data[*offset]));
	(*offset)++;
	
}


//EXTENSION 12 - BGP next hop ipv6
void ext12_parse(uint32_t *data, int *offset, uint8_t flags, struct storage *s){
	VERBOSE(CL_VERBOSE_ADVANCED,"\tBGP-NEXT-HOP: hight:%lu low:%lu (ipv6)",*((uint64_t *) &data[*offset]), \
		*((uint64_t *) &data[(*offset)+8]));
	(*offset)+=4;
}

//EXTENSION 13 - VLAN record (16b ints)
void ext13_parse(uint32_t *data, int *offset, uint8_t flags, struct storage *s){
	VERBOSE(CL_VERBOSE_ADVANCED,"\tVLAN-SRC: %hu (16b)", *((uint16_t *) &data[*offset]));
	VERBOSE(CL_VERBOSE_ADVANCED,"\tVLAN-DST: %hu (16b)", *(((uint16_t *) &data[*offset])+1));
	(*offset)++;
	
}

//EXTENSION 14 - Out packet count (32b int)
void ext14_parse(uint32_t *data, int *offset, uint8_t flags, struct storage *s){
	VERBOSE(CL_VERBOSE_ADVANCED,"\tOUT-PACKETS: %u (32b)", *((uint32_t *) &data[*offset]));
	(*offset)++;
	
}

//EXTENSION 15 - Out packet count (64b int)
void ext15_parse(uint32_t *data, int *offset, uint8_t flags, struct storage *s){
	VERBOSE(CL_VERBOSE_ADVANCED,"\tOUT-PACKETS: %lu (64b)", *((uint64_t *) &data[*offset]));
	(*offset)+=2;
	
}

//EXTENSION 16 - Out bytes count (32b int)
void ext16_parse(uint32_t *data, int *offset, uint8_t flags, struct storage *s){

	VERBOSE(CL_VERBOSE_ADVANCED,"\tOUT-BYTES: %u (32b)", *((uint32_t *) &data[*offset]));
	(*offset)++;
	
}

//EXTENSION 17 - Out bytes count (64b int)
void ext17_parse(uint32_t *data, int *offset, uint8_t flags, struct storage *s){
	VERBOSE(CL_VERBOSE_ADVANCED,"\tOUT-BYTES: %lu (64b)", *((uint64_t *) &data[*offset]));
	(*offset)+=2;
	
}

//EXTENSION 18 - Aggr flows (32b int)
void ext18_parse(uint32_t *data, int *offset, uint8_t flags, struct storage *s){

	VERBOSE(CL_VERBOSE_ADVANCED,"\tAGGR-FLOWS: %u (32b)", *((uint32_t *) &data[*offset]));
	(*offset)++;
	
}

//EXTENSION 19 - Aggr flows (64b int)
void ext19_parse(uint32_t *data, int *offset, uint8_t flags, struct storage *s){
	VERBOSE(CL_VERBOSE_ADVANCED,"\tAGGR-FLOWS: %lu (64b)", *((uint64_t *) &data[*offset]));
	(*offset)+=2;
	
}

//EXTENSION 20 - in src mac, out dst mac (64b int)
void ext20_parse(uint32_t *data, int *offset, uint8_t flags, struct storage *s){

	VERBOSE(CL_VERBOSE_ADVANCED,"\tIN-SRC-MAC: %lu (48b - 64 aling)", *((uint64_t *) &data[*offset]));
	(*offset)+=2;
	VERBOSE(CL_VERBOSE_ADVANCED,"\tOUT-DST-MAC: %lu (48b - 64 aling)", *((uint64_t *) &data[*offset]));
	(*offset)+=2;
	
}

//EXTENSION 21 - in dst mac, out src mac (64b int)
void ext21_parse(uint32_t *data, int *offset, uint8_t flags, struct storage *s){

	VERBOSE(CL_VERBOSE_ADVANCED,"\tIN-DST-MAC: %lu (48b - 64 aling)", *((uint64_t *) &data[*offset]));
	(*offset)+=2;
	VERBOSE(CL_VERBOSE_ADVANCED,"\tOUT-SRC-MAC: %lu (48b - 64 aling)", *((uint64_t *) &data[*offset]));
	(*offset)+=2;
	
}

//EXTENSION 22 - MPLS (32b ints)
void ext22_parse(uint32_t *data, int *offset, uint8_t flags, struct storage *s){
	int i=0;
	for(i=0;i<10;i++){
		VERBOSE(CL_VERBOSE_ADVANCED,"\tMPLS-LABEL-%i: %u (32b)",i, *((uint32_t *) &data[*offset+1]));
		VERBOSE(CL_VERBOSE_ADVANCED,"\tMPLS-LABEL-%i: %u (32b)",(i++), *((uint32_t *) &data[*offset]));
		(*offset)+=2;
	}
}

//EXTENSION 23 - Router ipv4
void ext23_parse(uint32_t *data, int *offset, uint8_t flags, struct storage *s){
	VERBOSE(CL_VERBOSE_ADVANCED,"\tROUTER-IP: %u (ipv4)", *((uint32_t *) &data[*offset]));
	(*offset)++;
	
}


//EXTENSION 24 - Router ipv6
void ext24_parse(uint32_t *data, int *offset, uint8_t flags, struct storage *s){
	VERBOSE(CL_VERBOSE_ADVANCED,"\tROUTER-IP: hight:%lu low:%lu (ipv6)",*((uint64_t *) &data[*offset]), \
		*((uint64_t *) &data[(*offset)+8]));
	(*offset)+=4;
}

//EXTENSION 25 - Router source id
void ext25_parse(uint32_t *data, int *offset, uint8_t flags, struct storage *s){
	VERBOSE(CL_VERBOSE_ADVANCED,"\tROUTER-ID-FILL: %hu ", *((uint16_t *) &data[*offset]));
	VERBOSE(CL_VERBOSE_ADVANCED,"\tROUTER-ID-ENGINE-TYPE: %hhu ", *(((uint8_t *) &data[*offset])+2));
	VERBOSE(CL_VERBOSE_ADVANCED,"\tROUTER-ID-ENGINE-ID: %hhu ", *(((uint8_t *) &data[*offset])+3));
	(*offset)++;
	
}

void (*ext_parse[26]) (uint32_t *data, int *offset, uint8_t flags, struct storage *s) = {
	ext0_parse,
	ext1_parse,
	ext2_parse,
	ext3_parse,
	ext4_parse,
	ext5_parse,
	ext6_parse,
	ext7_parse,
	ext8_parse,
	ext9_parse,
	ext10_parse,
	ext11_parse,
	ext12_parse,
	ext13_parse,
	ext14_parse,
	ext15_parse,
	ext16_parse,
	ext17_parse,
	ext18_parse,
	ext19_parse,
	ext20_parse,
	ext21_parse,
	ext22_parse,
	ext23_parse,
	ext24_parse,
	ext25_parse
};


//EXTENSION 0 -- not a real extension its just pading ect
void ext0_fill_tm(uint8_t flags, struct ipfix_template * template){
	VERBOSE(CL_VERBOSE_ADVANCED,"\tZERO EXTENSION");
}

//EXTENSION 1	
void ext1_fill_tm(uint8_t flags, struct ipfix_template * template){
	if(TestFlag(flags, FLAG_IPV6_ADDR)){
		//sourceIPv6Address
		template->fields[template->field_count].ie.id = 27;
		template->fields[template->field_count].ie.length = 16;
		template->field_count++;
		template->data_length += 16;  
		//destinationIPv6Address	
		template->fields[template->field_count].ie.id = 28;
		template->fields[template->field_count].ie.length = 16;
		template->field_count++;
		template->data_length += 16;  
	} else {
		//sourceIPv4Address
		template->fields[template->field_count].ie.id = 8;
		template->fields[template->field_count].ie.length = 4;
		template->field_count++;
		template->data_length += 4;  
		//destinationIPv4Address	
		template->fields[template->field_count].ie.id = 8;
		template->fields[template->field_count].ie.length = 4;
		template->field_count++;
		template->data_length += 4;  
	}
	template->template_length += 8;
}

//EXTENSION 2
void ext2_fill_tm(uint8_t flags, struct ipfix_template * template){
	//packetDeltaCount
	template->fields[template->field_count].ie.id = 2;
	template->fields[template->field_count].ie.length = 8;
	template->data_length += 8;
	template->template_length += 4;
}

//EXTENSION 3
void ext3_fill_tm(uint8_t flags, struct ipfix_template * template){
	//byteDeltaCount
	template->fields[template->field_count].ie.id = 1;
	template->fields[template->field_count].ie.length = 8;
	template->data_length += 8;
	template->template_length += 4;
}

//OPTIONAL EXTENSIONS
//EXTENSION 4 - interface record (16b ints)
void ext4_fill_tm(uint8_t flags, struct ipfix_template * template){
	template->fields[template->field_count].ie.id = 10;
	template->fields[template->field_count].ie.length = 2;
	template->data_length += 2;
	template->fields[template->field_count].ie.id = 14;
	template->fields[template->field_count].ie.length = 2;
	template->data_length += 2;
	template->template_length += 8;
}

//EXTENSION 5 - interface record (32b ints)
void ext5_fill_tm(uint8_t flags, struct ipfix_template * template){
	template->fields[template->field_count].ie.id = 10;
	template->fields[template->field_count].ie.length = 4;
	template->data_length += 4;
	template->fields[template->field_count].ie.id = 14;
	template->fields[template->field_count].ie.length = 4;
	template->data_length += 4;
	template->template_length += 8;
}

//OPTIONAL EXTENSIONS
//EXTENSION 6 - AS record (16b ints)
void ext6_fill_tm(uint8_t flags, struct ipfix_template * template){
	template->fields[template->field_count].ie.id = 16;
	template->fields[template->field_count].ie.length = 2;
	template->data_length += 2;
	template->fields[template->field_count].ie.id = 17;
	template->fields[template->field_count].ie.length = 2;
	template->data_length += 2;
	template->template_length += 8;
	
}

//EXTENSION 7 - AS record (32b ints)
void ext7_fill_tm(uint8_t flags, struct ipfix_template * template){
	template->fields[template->field_count].ie.id = 16;
	template->fields[template->field_count].ie.length = 4;
	template->data_length += 4;
	template->fields[template->field_count].ie.id = 17;
	template->fields[template->field_count].ie.length = 4;
	template->data_length += 4;
	template->template_length += 8;
	
}

//EXTENSION 8 - dst tos, dir, srcmask, dstmask in one 32b int
void ext8_fill_tm(uint8_t flags, struct ipfix_template * template){
	template->fields[template->field_count].ie.id = 55;
	template->fields[template->field_count].ie.length = 1;
	template->data_length += 1;
	template->fields[template->field_count].ie.id = 61;
	template->fields[template->field_count].ie.length = 1;
	template->data_length += 1;
	
	if(TestFlag(flags, FLAG_IPV6_ADDR)){
		template->fields[template->field_count].ie.id = 29;
		template->fields[template->field_count].ie.length = 1;
		template->data_length += 1;
		template->fields[template->field_count].ie.id = 30;
		template->fields[template->field_count].ie.length = 1;
		template->data_length += 1;
	} else {
		template->fields[template->field_count].ie.id = 9;
		template->fields[template->field_count].ie.length = 1;
		template->data_length += 1;
		template->fields[template->field_count].ie.id = 13;
		template->fields[template->field_count].ie.length = 1;
		template->data_length += 1;
	}
	template->template_length += 16;
}

//EXTENSION 9 - next hop ipv4
void ext9_fill_tm(uint8_t flags, struct ipfix_template * template){
	template->fields[template->field_count].ie.id = 15;
	template->fields[template->field_count].ie.length = 4;
	template->data_length += 4;
	template->template_length += 4;
}


//EXTENSION 10 - next hop ipv6
void ext10_fill_tm(uint8_t flags, struct ipfix_template * template){
	template->fields[template->field_count].ie.id = 62;
	template->fields[template->field_count].ie.length = 16;
	template->data_length += 16;
	template->template_length += 4;
}


//EXTENSION 11 - BGP next hop ipv4
void ext11_fill_tm(uint8_t flags, struct ipfix_template * template){
	template->fields[template->field_count].ie.id = 18;
	template->fields[template->field_count].ie.length = 4;
	template->data_length += 4;
	template->template_length += 4;
	
}


//EXTENSION 12 - BGP next hop ipv6
void ext12_fill_tm(uint8_t flags, struct ipfix_template * template){
	template->fields[template->field_count].ie.id = 63;
	template->fields[template->field_count].ie.length = 16;
	template->data_length += 16;
	template->template_length += 4;
}

//EXTENSION 13 - VLAN record (16b ints)
void ext13_fill_tm(uint8_t flags, struct ipfix_template * template){
	template->fields[template->field_count].ie.id = 58;
	template->fields[template->field_count].ie.length = 2;
	template->data_length += 2;
	template->fields[template->field_count].ie.id = 59;
	template->fields[template->field_count].ie.length = 2;
	template->data_length += 2;
	template->template_length += 8;
}

//EXTENSION 14 - Out packet count (32b int)
void ext14_fill_tm(uint8_t flags, struct ipfix_template * template){
	template->fields[template->field_count].ie.id = 24;
	template->fields[template->field_count].ie.length = 4;
	template->data_length += 4;
	template->template_length += 4;
}

//EXTENSION 15 - Out packet count (64b int)
void ext15_fill_tm(uint8_t flags, struct ipfix_template * template){
	template->fields[template->field_count].ie.id = 24;
	template->fields[template->field_count].ie.length = 8;
	template->data_length += 8;
	template->template_length += 4;
}

//EXTENSION 16 - Out bytes count (32b int)
void ext16_fill_tm(uint8_t flags, struct ipfix_template * template){
	template->fields[template->field_count].ie.id = 23;
	template->fields[template->field_count].ie.length = 4;
	template->data_length += 4;
	template->template_length += 4;
}

//EXTENSION 17 - Out bytes count (64b int)
void ext17_fill_tm(uint8_t flags, struct ipfix_template * template){
	template->fields[template->field_count].ie.id = 23;
	template->fields[template->field_count].ie.length = 8;
	template->data_length += 8;
	template->template_length += 4;
	
}

//EXTENSION 18 - Aggr flows (32b int)
void ext18_fill_tm(uint8_t flags, struct ipfix_template * template){
	template->fields[template->field_count].ie.id = 3;
	template->fields[template->field_count].ie.length = 4;
	template->data_length += 4;
	template->template_length += 4;
	
}

//EXTENSION 19 - Aggr flows (64b int)
void ext19_fill_tm(uint8_t flags, struct ipfix_template * template){
	template->fields[template->field_count].ie.id = 3;
	template->fields[template->field_count].ie.length = 8;
	template->data_length += 8;
	template->template_length += 4;
	
}

//EXTENSION 20 - in src mac, out dst mac (64b int)
void ext20_fill_tm(uint8_t flags, struct ipfix_template * template){
	template->fields[template->field_count].ie.id = 56;
	template->fields[template->field_count].ie.length = 6;
	template->data_length += 6;
	template->fields[template->field_count].ie.id = 57;
	template->fields[template->field_count].ie.length = 6;
	template->data_length += 6;
	template->template_length += 8;
	
}

//EXTENSION 21 - in dst mac, out src mac (64b int)
void ext21_fill_tm(uint8_t flags, struct ipfix_template * template){
	template->fields[template->field_count].ie.id = 80;
	template->fields[template->field_count].ie.length = 6;
	template->data_length += 6;
	template->fields[template->field_count].ie.id = 81;
	template->fields[template->field_count].ie.length = 6;
	template->data_length += 6;	
	template->template_length += 8;
}

//EXTENSION 22 - MPLS (32b ints)
void ext22_fill_tm(uint8_t flags, struct ipfix_template * template){
	int i=0;
	for(i=0;i<10;i++){
		template->fields[template->field_count].ie.id = 70 + i;
		template->fields[template->field_count].ie.length = 3;
		template->data_length += 3;
	}
	template->template_length += 40;
}

//EXTENSION 23 - Router ipv4
void ext23_fill_tm(uint8_t flags, struct ipfix_template * template){
	VERBOSE(CL_WARNING,"There is no element for router ip (this extension is ignored)");
}


//EXTENSION 24 - Router ipv6
void ext24_fill_tm(uint8_t flags, struct ipfix_template * template){
	VERBOSE(CL_WARNING,"There is no element for router ip (this extension is ignored)");
}

//EXTENSION 25 - Router source id
void ext25_fill_tm(uint8_t flags, struct ipfix_template * template){	
	VERBOSE(CL_VERBOSE_ADVANCED,"There is no element for router sourc id (filled as reserved 38 and 39 elements)");
	template->fields[template->field_count].ie.id = 38;
	template->fields[template->field_count].ie.length = 1;
	template->data_length += 1;
	template->fields[template->field_count].ie.id = 39;
	template->fields[template->field_count].ie.length = 1;
	template->data_length += 1;	
	template->template_length += 8;
}

void (*ext_fill_tm[26]) (uint8_t flags, struct ipfix_template * template) = {
	ext0_fill_tm,
	ext1_fill_tm,
	ext2_fill_tm,
	ext3_fill_tm,
	ext4_fill_tm,
	ext5_fill_tm,
	ext6_fill_tm,
	ext7_fill_tm,
	ext8_fill_tm,
	ext9_fill_tm,
	ext10_fill_tm,
	ext11_fill_tm,
	ext12_fill_tm,
	ext13_fill_tm,
	ext14_fill_tm,
	ext15_fill_tm,
	ext16_fill_tm,
	ext17_fill_tm,
	ext18_fill_tm,
	ext19_fill_tm,
	ext20_fill_tm,
	ext21_fill_tm,
	ext22_fill_tm,
	ext23_fill_tm,
	ext24_fill_tm,
	ext25_fill_tm
};

#define HEADER_ELEMENTS 7
int header_elements[][2] = {
	//id,size
	{21,4}, //flowEndSysUpTime MILLISECONDS !
	{22,4}, //flowStartSysUpTime MILLISECONDS !
	{6,1},  //tcpControlBits flags
	{4,1},  //protocolIdentifier
	{5,1},  //ipClassOfService
	{7,2},  //sourceTransportPort
	{11,2} //destinationTransportPort
};

#define ALLOC_FIELDS_SIZE 60

int fill_basic_template(uint8_t flags, struct ipfix_template **template){
	static int template_id_counter = 0;

	(*template) = (struct ipfix_template *) malloc(sizeof(struct ipfix_template) + \
		ALLOC_FIELDS_SIZE * sizeof(template_ie));

	(*template)->template_type = TM_TEMPLATE;
	(*template)->last_transmission = time(NULL);
	(*template)->last_message = 0;
	(*template)->template_id = template_id_counter;
	template_id_counter++;

	(*template)->field_count = 0;
	(*template)->scope_field_count = 0;
	(*template)->template_length = 0; //TODO    /**Length of the template */
	(*template)->data_length = 0;   //TODO     /**Length of the data record specified

	//(*template)->fields = (template_ie *) calloc(ALLOC_FIELDS_SIZE,sizeof(template_ie));

	// add header elements into template
	int i;
	for(i=0;i<HEADER_ELEMENTS;i++){	
		(*template)->fields[(*template)->field_count].ie.id = header_elements[i][0];
		(*template)->fields[(*template)->field_count].ie.length = header_elements[i][1];
		(*template)->field_count++;
		(*template)->data_length += header_elements[i][1];  
		(*template)->template_length += 4;
	}

	//add mandatory extensions elements 
	//Extension 1
	ext_fill_tm[1] (flags, *template);
	//Extension 2
	ext_fill_tm[2] (flags, *template);
	//Extension 3
	ext_fill_tm[3] (flags, *template);
	VERBOSE(CL_VERBOSE_ADVANCED,"BASIC TEMPLATE: 'field count - %i' template_length - %i" \
		,(*template)->field_count, (*template)->template_length);
	return 0;

}

void init_ipfix_msg(struct ipfix_message *ipfix_msg){
	ipfix_msg->pkt_header = (struct ipfix_header *) malloc(sizeof(struct ipfix_header));
	ipfix_msg->pkt_header->version = 0x000a;
	ipfix_msg->pkt_header->length = 16; //header size 
	ipfix_msg->pkt_header->export_time = 0;
	ipfix_msg->pkt_header->sequence_number = 0;
	ipfix_msg->pkt_header->observation_domain_id = 0; 

	ipfix_msg->input_info = NULL;
        memset(ipfix_msg->templ_set,0,sizeof(struct ipfix_template_set *) *1024);
        memset(ipfix_msg->opt_templ_set,0,sizeof(struct ipfix_optional_template_set *) *1024);
        memset(ipfix_msg->data_couple,0,sizeof(struct data_template_couple) *1023);
}

void chagne_endianity(struct ipfix_message *ipfix_msg){
	ipfix_msg->pkt_header->version = htons(ipfix_msg->pkt_header->version);
	ipfix_msg->pkt_header->length = htons(ipfix_msg->pkt_header->length); 
	ipfix_msg->pkt_header->export_time = htonl(ipfix_msg->pkt_header->export_time);
	ipfix_msg->pkt_header->sequence_number = htonl(ipfix_msg->pkt_header->sequence_number);
	ipfix_msg->pkt_header->observation_domain_id = htonl(ipfix_msg->pkt_header->observation_domain_id); 
}

void add_template(struct ipfix_message *ipfix_msg, struct ipfix_template * template){
	int i;
	for(i=0;i<1024;i++){
		if(ipfix_msg->templ_set[i] == NULL){
			ipfix_msg->templ_set[i] = (struct ipfix_template_set *) \
				malloc(sizeof(struct ipfix_options_template_set)+template->data_length);
			ipfix_msg->templ_set[i]->header.flowset_id = 2;
			ipfix_msg->templ_set[i]->header.length = 8 + template->template_length;
			ipfix_msg->templ_set[i]->first_record.template_id = template->template_id;
			ipfix_msg->templ_set[i]->first_record.count = template->field_count;
			memcpy ( ipfix_msg->templ_set[i]->first_record.fields, template->fields, template->data_length);
			ipfix_msg->pkt_header->length += ipfix_msg->templ_set[i]->header.length;
			break;
		}
	}
}

int main(){
	char file[] = "/home/kramolis/nffile/nfcapd.201107200815";
	FILE *f;
	int i;
	char *buffer = NULL;
	int buffer_size = 0;
	struct file_header_s header;
	struct stat_record_s stats;
	struct data_block_header_s block_header;
	struct common_record_s *record;
	struct extension_map_s *extension_map;
	struct extensions ext = {0,20,NULL};
	int read_size;
	void *config;
	void *dlhandle;
	int (*plugin_init) (char *, void **);
	int (*plugin_store) (void *, const struct ipfix_message *, const struct ipfix_template_mgr *);
	char *error;
	struct ipfix_message ipfix_msg;
	struct ipfix_template_mgr template_mgr;


	verbose = CL_VERBOSE_ADVANCED;
	dlhandle = dlopen ("/home/kramolis/git/ipfixcol/src/storage/fastbit/fastbit_output.so", RTLD_LAZY);
	if (!dlhandle) {
	    fputs (dlerror(), stderr);
	    exit(1);
	}

	//storage_init(NULL, &config);
	plugin_init = dlsym(dlhandle, "storage_init");
	if ((error = dlerror()) != NULL)  {
	    fputs(error, stderr);
	    exit(1);
	}

	plugin_store = dlsym(dlhandle, "store_packet");
	if ((error = dlerror()) != NULL)  {
	    fputs(error, stderr);
	    exit(1);
	}
	plugin_init(NULL, &config); //TODO add arguments
	//inital space for extension map
	ext.map = (struct extension *) calloc(sizeof(struct extension),ext.size);
	if(ext.map == NULL){
		VERBOSE(CL_ERROR,"Can't read allocate memory for extension map");
		return 1;
	}
	
	template_mgr.templates = (struct ipfix_template **) calloc(sizeof(struct ipfix_template *),ext.size);
	if(ext.map == NULL){
		VERBOSE(CL_ERROR,"Can't read allocate memory for templates");
		return 1;
	}
	template_mgr.max_length = ext.size;

	f = fopen(file,"r");

	if(f != NULL){
		//read header of nffile
		read_size = fread(&header, sizeof(struct file_header_s), 1,f);
		if (read_size != 1){
			VERBOSE(CL_ERROR,"Can't read file header: %s",file);
			fclose(f);
			return 1;
		}
		VERBOSE(CL_VERBOSE_ADVANCED,"Parsed header from: '%s'",file);
		VERBOSE(CL_VERBOSE_ADVANCED,"\tMAGIC: %x", header.magic);
		VERBOSE(CL_VERBOSE_ADVANCED,"\tVERSION: %i", header.version);
		VERBOSE(CL_VERBOSE_ADVANCED,"\tFLAGS: %i", header.flags);
		VERBOSE(CL_VERBOSE_ADVANCED,"\tNUMBER OF BLOCKS: %i", header.NumBlocks);
		VERBOSE(CL_VERBOSE_ADVANCED,"\tIDENT: '%s'", header.ident);


		read_size = fread(&stats, sizeof(struct stat_record_s), 1,f);
		if (read_size != 1){
			VERBOSE(CL_ERROR,"Can't read file statistics: %s",file);
			fclose(f);
			return 1;
		}
		
		VERBOSE(CL_VERBOSE_ADVANCED,"Parsed statistics from: '%s'",file);
		VERBOSE(CL_VERBOSE_ADVANCED,"\tFLOWS: %lu", stats.numflows);
		VERBOSE(CL_VERBOSE_ADVANCED,"\tBYTES: %lu", stats.numbytes);
		VERBOSE(CL_VERBOSE_ADVANCED,"\tPACKTES: %lu", stats.numpackets);
		VERBOSE(CL_VERBOSE_ADVANCED,"\tFLOWS-TCP: %lu", stats.numflows_tcp);
		VERBOSE(CL_VERBOSE_ADVANCED,"\tFLOWS-UDP: %lu", stats.numflows_udp);
		VERBOSE(CL_VERBOSE_ADVANCED,"\tFLOWS-ICMP: %lu", stats.numflows_icmp);
		VERBOSE(CL_VERBOSE_ADVANCED,"\tFLOWS-OTHER: %lu", stats.numflows_other);
		VERBOSE(CL_VERBOSE_ADVANCED,"\tBYTES-TCP: %lu", stats.numbytes_tcp);
		VERBOSE(CL_VERBOSE_ADVANCED,"\tBYTES-UDP: %lu", stats.numbytes_udp);
		VERBOSE(CL_VERBOSE_ADVANCED,"\tBYTES-ICMP: %lu", stats.numbytes_icmp);
		VERBOSE(CL_VERBOSE_ADVANCED,"\tBYTES-OTHER: %lu", stats.numbytes_other);
		VERBOSE(CL_VERBOSE_ADVANCED,"\tPACKETS-TCP: %lu", stats.numpackets_tcp);
		VERBOSE(CL_VERBOSE_ADVANCED,"\tPACKETS-UDP: %lu", stats.numpackets_udp);
		VERBOSE(CL_VERBOSE_ADVANCED,"\tPACKETS-ICMP: %lu", stats.numpackets_icmp);
		VERBOSE(CL_VERBOSE_ADVANCED,"\tPACKETS-OTHER: %lu", stats.numpackets_other);
		VERBOSE(CL_VERBOSE_ADVANCED,"\tFIRST-SEEN: %u", stats.first_seen);
		VERBOSE(CL_VERBOSE_ADVANCED,"\tLAST-SEEN: %u", stats.last_seen);
		VERBOSE(CL_VERBOSE_ADVANCED,"\tMSEC-FIRST: %hu", stats.msec_first);
		VERBOSE(CL_VERBOSE_ADVANCED,"\tMSEC-LAST: %hu", stats.msec_last);
		VERBOSE(CL_VERBOSE_ADVANCED,"\tSEQUENCE-FAIULURE: %u", stats.sequence_failure);

		
		for(i = 0; i < header.NumBlocks ; i++){
			VERBOSE(CL_VERBOSE_ADVANCED,"---------?C--------");
			read_size = fread(&block_header, sizeof(struct data_block_header_s), 1,f);
			if (read_size != 1){
				VERBOSE(CL_ERROR,"Can't read block header: %s",file);
				fclose(f);
				return 1;
			}

			VERBOSE(CL_VERBOSE_ADVANCED,"BLOCK: %u",i);
			VERBOSE(CL_VERBOSE_ADVANCED,"\tRECORDS: %u", block_header.NumRecords);
			VERBOSE(CL_VERBOSE_ADVANCED,"\tSIZE: %u", block_header.size);
			VERBOSE(CL_VERBOSE_ADVANCED,"\tID (block type): %u", block_header.id);
			VERBOSE(CL_VERBOSE_ADVANCED,"\tPADDING: %u", block_header.pad);

			//force buffer realocation if is too small for record
			if(buffer != NULL && buffer_size < block_header.size){
				free(buffer);      
				buffer = NULL;
				buffer_size = 0;
			}

			if(buffer == NULL){
				buffer = (char *) malloc(block_header.size);
				VERBOSE(CL_VERBOSE_ADVANCED,"Buffer malloc");
				if(buffer == NULL){
					VERBOSE(CL_ERROR,"Can't alocate memory for record data");
					return 1;
				}
				buffer_size = block_header.size;
			}
			VERBOSE(CL_VERBOSE_ADVANCED,"RECORDS OFFSET in file: %lu",ftell(f));

			
			read_size = fread(buffer, block_header.size, 1,f);
			if (read_size != 1){
				VERBOSE(CL_ERROR,"Can't read record data: %s",file);
				fclose(f);
				return 1;
			}
			hex(buffer, block_header.size);
		
			int size=0;
			while (size < block_header.size){
				VERBOSE(CL_VERBOSE_ADVANCED,"OFFSET: %u - %p",size,buffer);
				record = (struct common_record_s *) buffer;

				if(record->type == CommonRecordType){
					VERBOSE(CL_VERBOSE_ADVANCED,"Parsed record from: '%s'",file);
					VERBOSE(CL_VERBOSE_ADVANCED,"\tTYPE: %hu", record->type);
					VERBOSE(CL_VERBOSE_ADVANCED,"\tSIZE: %hu", record->size);
					VERBOSE(CL_VERBOSE_ADVANCED,"\tFLAGS: %hhu", record->flags);
					VERBOSE(CL_VERBOSE_ADVANCED,"\tEXPORTER-REF: %hhu", record->exporter_ref);
					VERBOSE(CL_VERBOSE_ADVANCED,"\tEXT-MAP: %hu", record->ext_map);
					VERBOSE(CL_VERBOSE_ADVANCED,"\tMSEC-FIRST: %hu", record->msec_first);
					VERBOSE(CL_VERBOSE_ADVANCED,"\tMSEC-LAST: %hu", record->msec_last);
					VERBOSE(CL_VERBOSE_ADVANCED,"\tFIRST: %u", record->first);
					VERBOSE(CL_VERBOSE_ADVANCED,"\tLAST: %u", record->last);
					VERBOSE(CL_VERBOSE_ADVANCED,"\tFWD-STATUS: %hhu", record->fwd_status);
					VERBOSE(CL_VERBOSE_ADVANCED,"\tTCP-FLAGS: %hhu", record->tcp_flags);
					VERBOSE(CL_VERBOSE_ADVANCED,"\tPROTOCOL: %hhu", record->prot);
					VERBOSE(CL_VERBOSE_ADVANCED,"\tTOS: %hhu", record->tos);
					VERBOSE(CL_VERBOSE_ADVANCED,"\tSRC-PORT: %hu", record->srcport);
					VERBOSE(CL_VERBOSE_ADVANCED,"\tDST-PORT: %hu", record->dstport);

					hex(record->data,record->size - sizeof(struct common_record_s));
					int data_offset = 0; // record->data = uint32_t
					int id;
					int j,eid;

					//check id -> most extensions should be on its index
					if(ext.map[record->ext_map].id == record->ext_map){
						id = record->ext_map;
						VERBOSE(CL_VERBOSE_ADVANCED,"\tMAP-INDEX-MATCH: %hu", record->ext_map);
					} else { //index does NOT match map id.. we need to find it
						for(j=0;j<ext.filled;j++){
							if(ext.map[j].id == record->ext_map){
								id = j;
								VERBOSE(CL_VERBOSE_ADVANCED,"\tMAP-INDEX-NOT-MATCH: %hu - %hu",j, record->ext_map);
							}
						}
					}
		
					ext_parse[1](record->data, &data_offset, record->flags, NULL); 
					ext_parse[2](record->data, &data_offset, record->flags, NULL); 
					ext_parse[3](record->data, &data_offset, record->flags, NULL); 
					
					for(eid=0;eid<ext.map[id].values_count;eid++){
						VERBOSE(CL_VERBOSE_ADVANCED,"\tMAP: %hu EXT-ID %hu",id ,ext.map[id].value[eid]);
						ext_parse[ext.map[id].value[eid]](record->data, &data_offset, record->flags, NULL); 
					}

				} else if(record->type == ExtensionMapType){
					extension_map = (struct extension_map_s *) buffer;
					hex(buffer,record->size);
					if(ext.filled == ext.size){
						//double the size of extension map array
						if(realloc(ext.map,ext.size*2)==NULL){
							VERBOSE(CL_ERROR,"Can't realloc extension map array");
							fclose(f);
							return 1;
						}
						ext.size*=2;
					}
					ext.filled++;
					ext.map[ext.filled].value = (uint16_t *) malloc(extension_map->extension_size);
					ext.map[ext.filled].values_count = 0;
					ext.map[ext.filled].id = extension_map->map_id;

					if(template_mgr.counter == template_mgr.max_length){
						//double the size of extension map array
						if(realloc(*(template_mgr.templates),template_mgr.max_length*2)==NULL){
							VERBOSE(CL_ERROR,"Can't realloc extension map array");
							fclose(f);
							return 1;
						}
						template_mgr.max_length*=2;
					}
					template_mgr.counter++;
					struct ipfix_template *template1;
					fill_basic_template(0, &(template_mgr.templates[template_mgr.counter])); //template for this record with ipv4
					template1 = template_mgr.templates[template_mgr.counter];

					struct ipfix_template *template2;
					fill_basic_template(1, &(template_mgr.templates[template_mgr.counter])); //template for this record with ipv6
					template2 = template_mgr.templates[template_mgr.counter];

					VERBOSE(CL_VERBOSE_ADVANCED,"RECORD = EXTENSION MAP");
					VERBOSE(CL_VERBOSE_ADVANCED,"\tTYPE: %hu", extension_map->type);
					VERBOSE(CL_VERBOSE_ADVANCED,"\tSIZE: %hu", extension_map->size);
					VERBOSE(CL_VERBOSE_ADVANCED,"\tMAP ID: %hu", extension_map->map_id);
					VERBOSE(CL_VERBOSE_ADVANCED,"\tEXTENSION_SIZE: %hu", extension_map->extension_size);

					int eid=0;
					for(eid = 0; eid < extension_map->extension_size/2;eid++){ // extension id is 2 byte
						VERBOSE(CL_VERBOSE_ADVANCED,"\tEXTENSION_ID: %hu - %p", extension_map->ex_id[eid],&extension_map->ex_id[eid]);
						ext.map[ext.filled].value[eid] = extension_map->ex_id[eid]; 
						ext.map[ext.filled].values_count++;
						ext_fill_tm[eid] (0, template1);
						ext_fill_tm[eid] (1, template2);
					}
					
					init_ipfix_msg(&ipfix_msg);
					add_template(&ipfix_msg,template1);
					add_template(&ipfix_msg,template2);
					chagne_endianity(&ipfix_msg);
					plugin_store (&config, &ipfix_msg, &template_mgr);

				} else if(record->type == ExporterType){
					VERBOSE(CL_VERBOSE_ADVANCED,"RECORD = EXPORTER TYPE");
					VERBOSE(CL_VERBOSE_ADVANCED,"\tTYPE: %hu", record->type);
					VERBOSE(CL_VERBOSE_ADVANCED,"\tSIZE: %hu", record->size);
				} else {
					VERBOSE(CL_VERBOSE_ADVANCED,"UNKNOWN RECORD TYPE");
					VERBOSE(CL_VERBOSE_ADVANCED,"\tTYPE: %hu", record->type);
					VERBOSE(CL_VERBOSE_ADVANCED,"\tSIZE: %hu", record->size);
				}

				size += record->size;
				if(size >= block_header.size){
					VERBOSE(CL_VERBOSE_ADVANCED,"------- SIZE: %u - %u",size,block_header.size);
					break;
				}
				buffer+= record->size;
			}
		}	

		//TERE IS SOMETHING REALY BAD HERE SIGSEV ON FREE
		//VERBOSE(CL_VERBOSE_ADVANCED,"---------===========-----------");
		//if(buffer!=NULL){
 		//	free(buffer);
		//}
		fclose(f);
	} else {
		VERBOSE(CL_ERROR,"Can't open file: %s",file);
	}
	
	return 0;
}
