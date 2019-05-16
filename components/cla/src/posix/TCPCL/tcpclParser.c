#include <stddef.h>
#include <stdlib.h>

#include "upcn/upcn.h"
#include <tcpclParser.h>
#include "upcn/contactManager.h"
#include "upcn/groundStation.h"
#include "upcn/routingTable.h"
#include "upcn/rrnd.h"
#include "upcn/satpos.h"
#include "drv/mini-printf.h"
#include "upcn/beaconProcessor.h"

void tcpcl_parser_read_byte(struct tcpcl_parser *parser, uint8_t byte)
{
	switch (parser->stage) {
	case TCPCL_EXPECT_TYPE_FLAGS:
		if (byte == TCPCL_TYPE_DATA_SEGMENT) {
			parser->type = TCPCL_TYPE_DATA_SEGMENT;
			parser->stage = TCPCL_GET_SIZE;
		} else if (byte == TCPCL_TYPE_MANAGEMENT_DATA) {
			parser->type = TCPCL_TYPE_MANAGEMENT_DATA;
			parser->stage = TCPCL_GET_SIZE;
		} else {
			parser->basedata->status = PARSER_STATUS_ERROR;
			tcpcl_parser_reset(parser);
		}
		/*
		 * Something went wrong and we have to reset the parser
		 * and wait for another proper tcpcl packet
		 */
		break;

	case TCPCL_GET_SIZE:
		sdnv_read_u32(
			&parser->sdnv_state,
			&parser->fragment_size,
			byte);

		switch (parser->sdnv_state.status) {
		case SDNV_IN_PROGRESS:
			/* intentional, do nothing unless
			 * a termination state is reached
			 */
			break;
		case SDNV_DONE:
			if (parser->type == TCPCL_TYPE_DATA_SEGMENT) {
				parser->stage = TCPCL_FORWARD_BUNDLE;
			} else if (parser->type == TCPCL_TYPE_MANAGEMENT_DATA) {
				parser->stage = TCPCL_FORWARD_MANAGEMENT;
			} else {
				LOG("wrong!");
				parser->basedata->status = PARSER_STATUS_ERROR;
				tcpcl_parser_reset(parser);
			}
			sdnv_reset(&parser->sdnv_state);
			break;
		case SDNV_ERROR:
			LOG("SDNV-ERROR!");
			sdnv_reset(&parser->sdnv_state);
			parser->basedata->status = PARSER_STATUS_ERROR;
			break;
		}
		break;

	default:
		parser->basedata->status = PARSER_STATUS_ERROR;
		break;
	}
}

void tcpcl_parser_reset(struct tcpcl_parser *parser)
{
	parser->basedata->status = PARSER_STATUS_GOOD;
	parser->basedata->flags = PARSER_FLAG_NONE;
	parser->stage = TCPCL_EXPECT_TYPE_FLAGS;
	parser->type = TCPCL_TYPE_UNDEFINED;
	free(parser->string);
	parser->string = NULL;
	parser->intdata = 0;
	parser->intdata_index = 0;
}

struct parser *tcpcl_parser_init(struct tcpcl_parser *parser)
{
	parser->basedata = malloc(sizeof(struct parser));
	if (parser->basedata == NULL)
		return NULL;
	parser->string = NULL;
	tcpcl_parser_reset(parser);
	sdnv_reset(&parser->sdnv_state);
	return parser->basedata;
}

size_t tcpcl_parser_read(struct tcpcl_parser *parser,
	const uint8_t *buffer, size_t length)
{
	size_t i = 0;

	while (i < length
		&& parser->basedata->status == PARSER_STATUS_GOOD
		&& parser->stage != TCPCL_FORWARD_BUNDLE
		&& parser->stage != TCPCL_FORWARD_MANAGEMENT) {
		tcpcl_parser_read_byte(parser, buffer[i]);
		i++;
	}

	return i;
}
