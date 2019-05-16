#ifndef TCPCLPARSER_H_INCLUDED
#define TCPCLPARSER_H_INCLUDED

#include <stdint.h>

#include "upcn/parser.h"
#include "upcn/sdnv.h"


enum tcpcl_type {
	TCPCL_TYPE_UNDEFINED           = 0x00,
	TCPCL_TYPE_DATA_SEGMENT	       = 0x13,
	TCPCL_TYPE_MANAGEMENT_DATA     = 0x73
};

enum tcpcl_parser_stage {
	TCPCL_EXPECT_TYPE_FLAGS,
	TCPCL_GET_SIZE,
	TCPCL_FORWARD_BUNDLE,
	TCPCL_FORWARD_MANAGEMENT
};

struct tcpcl_parser {
	struct parser *basedata;
	enum tcpcl_parser_stage stage;
	enum tcpcl_type type;
	char *string;
	struct sdnv_state sdnv_state;
	uint32_t fragment_size;
	uint32_t intdata;
	uint16_t intdata_index;
};

struct parser *tcpcl_parser_init(struct tcpcl_parser *parser);
void tcpcl_parser_read_byte(struct tcpcl_parser *parser, uint8_t byte);
void tcpcl_parser_reset(struct tcpcl_parser *parser);
size_t tcpcl_parser_read(struct tcpcl_parser *parser,
			 const uint8_t *buffer, size_t length);

#endif /* INPUTPARSER_H_INCLUDED */
