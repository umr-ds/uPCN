#ifndef INPUTPARSER_H_INCLUDED
#define INPUTPARSER_H_INCLUDED

#include <stdint.h>

#include "upcn/parser.h"

enum input_type {
	INPUT_TYPE_UNDEFINED           = 0x00,
	INPUT_TYPE_ROUTER_COMMAND_DATA = 0x01,
	INPUT_TYPE_BUNDLE_VERSION      = 0x02,

	/* Sub states to distinguish betwwen Bundle Protocol v6 (RFC 5050) and
	 * BP v7 (IETF bis)
	 */
	INPUT_TYPE_BUNDLE_V6 = 0x10,
	INPUT_TYPE_BUNDLE_V7 = 0x11,

	INPUT_TYPE_BEACON_DATA         = 0x03,
	INPUT_TYPE_RRND_DATA           = 0x04,
	/* Some basic commands handles in inputParser */
	INPUT_TYPE_DBG_QUERY           = 0x11,
	INPUT_TYPE_RESET_TIME          = 0x12,
	INPUT_TYPE_SET_TIME            = 0x13,
	INPUT_TYPE_RESET_STATS         = 0x14,
	INPUT_TYPE_CLEAR_TRACES        = 0x15,
	INPUT_TYPE_STORE_TRACE         = 0x16,
	INPUT_TYPE_ECHO                = 0x17,
	INPUT_TYPE_RESET               = 0x18,
	/* For RRND commands there is a separate parser */
	INPUT_TYPE_MAX                 = 0x18
};

enum input_parser_stage {
	INPUT_EXPECT_DATA_ESCAPE_DELIMITER,
	INPUT_EXPECT_DATA_BEGIN_MARKER,
	INPUT_EXPECT_TYPE,
	INPUT_EXPECT_TYPE_DELIMITER,
	INPUT_EXPECT_TIME,
	INPUT_EXPECT_DATA_END_MARKER,
	INPUT_EXPECT_DATA,
};

struct input_parser {
	struct parser *basedata;
	enum input_parser_stage stage;
	enum input_type type;
	uint32_t intdata;
	uint16_t intdata_index;
};

struct parser *input_parser_init(struct input_parser *parser);
size_t input_parser_read(struct input_parser *parser,
	const uint8_t *buffer, size_t length);
void input_parser_reset(struct input_parser *parser);

#endif /* INPUTPARSER_H_INCLUDED */
