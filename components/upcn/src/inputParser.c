#include <stddef.h>
#include <stdlib.h>

#include "upcn/upcn.h"
#include "upcn/inputParser.h"
#include "upcn/contactManager.h"
#include "upcn/beaconProcessor.h"

static const uint8_t DATA_ESCAPE_DELIMITER; /* 0x00 */
static const uint8_t DATA_BEGIN_MARKER       = 0xFF;
static const uint8_t DATA_END_MARKER         = 0xFF;
static const uint8_t TYPE_DELIMITER          = ':';

static const uint8_t ECHO_MESSAGE[]        =
	"This is uPCN, compiled " __DATE__ " " __TIME__ ". " \
	"Greetings to you, earthling.";

static void input_parser_read_byte(struct input_parser *parser, uint8_t byte)
{
	switch (parser->stage) {
	case INPUT_EXPECT_DATA_ESCAPE_DELIMITER:
		if (byte == DATA_ESCAPE_DELIMITER)
			parser->stage = INPUT_EXPECT_DATA_BEGIN_MARKER;
		else if (byte == DATA_BEGIN_MARKER)
			parser->stage = INPUT_EXPECT_TYPE;
		/*
		 * We do not need an Input Parser reset here as parsing
		 * has not begun and we can simply wait for the "next"
		 * escape delimiter...
		 */
		break;
	case INPUT_EXPECT_DATA_BEGIN_MARKER:
		if (byte == DATA_BEGIN_MARKER)
			parser->stage = INPUT_EXPECT_TYPE;
		else if (byte != DATA_ESCAPE_DELIMITER)
			parser->stage = INPUT_EXPECT_DATA_ESCAPE_DELIMITER;
		/* See above... */
		break;
	case INPUT_EXPECT_TYPE:
		parser->stage = INPUT_EXPECT_TYPE_DELIMITER;
		if (byte >= INPUT_TYPE_ROUTER_COMMAND_DATA &&
		    byte <= INPUT_TYPE_MAX)
			parser->type = (enum input_type)byte;
		else if (byte == DATA_ESCAPE_DELIMITER)
			parser->stage = INPUT_EXPECT_DATA_BEGIN_MARKER;
		else if (byte != DATA_BEGIN_MARKER)
			parser->basedata->status = PARSER_STATUS_ERROR;
		break;
	case INPUT_EXPECT_TYPE_DELIMITER:
		if (byte == TYPE_DELIMITER) {
			parser->intdata = 0;
			parser->intdata_index = 0;
			if (parser->type == INPUT_TYPE_SET_TIME)
				parser->stage = INPUT_EXPECT_TIME;
			else if (parser->type >= INPUT_TYPE_DBG_QUERY)
				parser->stage = INPUT_EXPECT_DATA_END_MARKER;
			else
				parser->stage = INPUT_EXPECT_DATA;
		} else {
			parser->basedata->status = PARSER_STATUS_ERROR;
		}
		break;
	case INPUT_EXPECT_TIME:
		parser->intdata <<= 8;
		parser->intdata |= byte;
		if (++parser->intdata_index == 4)
			parser->stage = INPUT_EXPECT_DATA_END_MARKER;
		break;
	case INPUT_EXPECT_DATA_END_MARKER:
		if (byte == DATA_END_MARKER) {
			parser->basedata->status = PARSER_STATUS_DONE;
			switch (parser->type) {
			case INPUT_TYPE_RESET_TIME:
				hal_time_init(0);
				contact_manager_reset_time();
				break;
			case INPUT_TYPE_SET_TIME:
				hal_time_init(parser->intdata);
				contact_manager_reset_time();
				LOGI("Time set", parser->intdata);
				hal_io_send_packet("\x01", 1,
					COMM_TYPE_GENERIC_RESULT);
				break;
			case INPUT_TYPE_DBG_QUERY:
#ifdef UPCN_TEST_BUILD
				upcntest_print();
#endif /* UPCN_TEST_BUILD */
				hal_platform_print_system_info();
				upcn_dbg_memprint();
				upcn_dbg_memstat_print();
				upcn_dbg_printtrace();
				upcn_dbg_printtraces();
				upcn_dbg_printlogs();
				break;
			case INPUT_TYPE_RESET_STATS:
				upcn_dbg_resettrace();
				upcn_dbg_memstat_reset();
				hal_io_send_packet("\x01", 1,
					COMM_TYPE_GENERIC_RESULT);
				LOG("Trace stats reset");
				break;
			case INPUT_TYPE_CLEAR_TRACES:
				upcn_dbg_cleartraces();
				hal_io_send_packet("\x01", 1,
					COMM_TYPE_GENERIC_RESULT);
				LOG("Trace storage reset");
				break;
			case INPUT_TYPE_STORE_TRACE:
				upcn_dbg_storetrace();
				hal_io_send_packet("\x01", 1,
					COMM_TYPE_GENERIC_RESULT);
				LOG("Trace stored");
				break;
			case INPUT_TYPE_ECHO:
				hal_io_send_packet(
					ECHO_MESSAGE, sizeof(ECHO_MESSAGE) - 1,
					COMM_TYPE_ECHO);
				break;
			case INPUT_TYPE_RESET:
				LOG("SYSTEM RESET");
				hal_io_send_packet("\x01", 1,
					COMM_TYPE_GENERIC_RESULT);
				hal_platform_restart_upcn();
				break;
			default:
				parser->basedata->status = PARSER_STATUS_ERROR;
				break;
			}
		} else {
			parser->basedata->status = PARSER_STATUS_ERROR;
		}
		break;
	case INPUT_EXPECT_DATA:
		/* Should not get here... */
		parser->basedata->status = PARSER_STATUS_ERROR;
		break;
	default:
		parser->basedata->status = PARSER_STATUS_ERROR;
		break;
	}
}

size_t input_parser_read(struct input_parser *parser,
	const uint8_t *buffer, size_t length)
{
	size_t i = 0;

	while (i < length
			&& parser->basedata->status == PARSER_STATUS_GOOD
			&& parser->stage != INPUT_EXPECT_DATA) {
		input_parser_read_byte(parser, buffer[i]);
		i++;
	}

	return i;
}

void input_parser_reset(struct input_parser *parser)
{
	parser->basedata->status = PARSER_STATUS_GOOD;
	parser->basedata->flags = PARSER_FLAG_NONE;
	parser->stage = INPUT_EXPECT_DATA_ESCAPE_DELIMITER;
	parser->type = INPUT_TYPE_UNDEFINED;
	parser->intdata = 0;
	parser->intdata_index = 0;
}

struct parser *input_parser_init(struct input_parser *parser)
{
	parser->basedata = malloc(sizeof(struct parser));
	if (parser->basedata == NULL)
		return NULL;
	input_parser_reset(parser);
	return parser->basedata;
}
