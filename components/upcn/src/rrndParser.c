#include <stdlib.h>

#include "upcn/rrndParser.h"
#include "upcn/upcn.h"

#include <stdint.h>
#include <string.h>

static void rrnd_parser_allocate_string(struct rrnd_parser *const parser)
{
	if (parser->current_int <= 0 || parser->current_int >= 1024) {
		parser->current_string = NULL;
		parser->basedata->status = PARSER_STATUS_ERROR;
		return;
	}
	parser->current_string = malloc(parser->current_int + 1);
	if (parser->current_string == NULL)
		parser->basedata->status = PARSER_STATUS_ERROR;
}

static void rrnd_parser_next(struct rrnd_parser *const parser)
{
	// Check the current stage and infer the next stage/status
	switch (parser->stage) {
	case RRNDP_EXPECT_COMMAND_TYPE:
		switch (parser->command->type) {
		case RRND_COMMAND_INITIALIZE_SATPOS:
			parser->stage = RRNDP_EXPECT_TLE_LENGTH;
			break;
		case RRND_COMMAND_INFER_CONTACT:
		case RRND_COMMAND_QUERY_GS:
		case RRND_COMMAND_INTEGRATE_METRICS:
			parser->stage = RRNDP_EXPECT_GS_EID_LENGTH;
			break;
		default:
			parser->basedata->status = PARSER_STATUS_ERROR;
			break;
		}
		parser->current_int = 0;
		parser->current_index = 0;
		break;
	case RRNDP_EXPECT_GS_EID_LENGTH:
		rrnd_parser_allocate_string(parser);
		parser->command->gs = parser->current_string;
		parser->stage = RRNDP_EXPECT_GS_EID;
		parser->current_index = 0;
		break;
	case RRNDP_EXPECT_SOURCE_GS_EID_LENGTH:
		rrnd_parser_allocate_string(parser);
		parser->command->source_gs = parser->current_string;
		parser->stage = RRNDP_EXPECT_SOURCE_GS_EID;
		parser->current_index = 0;
		break;
	case RRNDP_EXPECT_TLE_LENGTH:
		rrnd_parser_allocate_string(parser);
		parser->command->tle = parser->current_string;
		parser->stage = RRNDP_EXPECT_TLE;
		parser->current_index = 0;
		break;
	case RRNDP_EXPECT_GS_EID:
		switch (parser->command->type) {
		case RRND_COMMAND_INFER_CONTACT:
		case RRND_COMMAND_QUERY_GS:
			parser->basedata->status = PARSER_STATUS_DONE;
			break;
		case RRND_COMMAND_INTEGRATE_METRICS:
			parser->stage = RRNDP_EXPECT_SOURCE_GS_EID_LENGTH;
			parser->current_int = 0;
			parser->current_index = 0;
			break;
		default:
			parser->basedata->status = PARSER_STATUS_ERROR;
			break;
		}
		break;
	case RRNDP_EXPECT_SOURCE_GS_EID:
		parser->stage = RRNDP_EXPECT_RELIABILITY;
		parser->current_int = 0;
		parser->current_index = 0;
		break;
	case RRNDP_EXPECT_TLE:
		parser->basedata->status = PARSER_STATUS_DONE;
		break;
	case RRNDP_EXPECT_RELIABILITY:
		// The float has been stored as uint32_t before
		ASSERT(sizeof(float) == sizeof(uint32_t));
		memcpy(&parser->command->gs_reliability, &parser->current_int,
		       sizeof(float));
		parser->basedata->status = PARSER_STATUS_DONE;
		break;
	default:
		parser->basedata->status = PARSER_STATUS_ERROR;
		break;
	}
	// Send the command if we're done
	if (parser->basedata->status == PARSER_STATUS_DONE) {
		parser->send_callback(parser->command);
		parser->command = NULL;
	}
}

static void rrnd_parser_read_byte(
	struct rrnd_parser *const parser, const uint8_t byte)
{
	switch (parser->stage) {
	case RRNDP_EXPECT_COMMAND_TYPE:
		parser->command->type = (enum rrnd_command_type)byte;
		if (parser->command->type > RRND_COMMAND_NONE &&
		    parser->command->type < _RRND_COMMAND_LAST)
			rrnd_parser_next(parser);
		else
			parser->basedata->status = PARSER_STATUS_ERROR;
		break;
	case RRNDP_EXPECT_GS_EID_LENGTH:
	case RRNDP_EXPECT_SOURCE_GS_EID_LENGTH:
	case RRNDP_EXPECT_TLE_LENGTH:
		parser->current_int <<= 8;
		parser->current_int |= byte;
		if (++parser->current_index == 2)
			rrnd_parser_next(parser);
		break;
	case RRNDP_EXPECT_GS_EID:
	case RRNDP_EXPECT_SOURCE_GS_EID:
	case RRNDP_EXPECT_TLE:
		parser->current_string[parser->current_index] = (char)byte;
		parser->current_index++;
		parser->current_int--;
		if (parser->current_int == 0) {
			parser->current_string[parser->current_index] = '\0';
			rrnd_parser_next(parser);
		}
		break;
	case RRNDP_EXPECT_RELIABILITY:
		// 32 bit IEEE 754 float, big endian
		parser->current_int <<= 8;
		parser->current_int |= byte;
		if (++parser->current_index == 4)
			rrnd_parser_next(parser);
		break;
	default:
		parser->basedata->status = PARSER_STATUS_ERROR;
		break;
	}
}

size_t rrnd_parser_read(
	struct rrnd_parser *const parser,
	const uint8_t *buffer, size_t length)
{
	size_t i = 0;

	while (i < length && parser->basedata->status == PARSER_STATUS_GOOD) {
		rrnd_parser_read_byte(parser, buffer[i]);
		i++;
	}

	return i;
}

struct parser *rrnd_parser_init(
	struct rrnd_parser *const parser,
	void (*send_callback)(struct rrnd_command *))
{
	parser->basedata = malloc(sizeof(struct parser));
	if (parser->basedata == NULL)
		return NULL;
	ASSERT(send_callback != NULL);
	parser->send_callback = send_callback;
	parser->basedata->status = PARSER_STATUS_ERROR;
	parser->command = NULL;
	if (rrnd_parser_reset(parser) != UPCN_OK)
		return NULL;
	return parser->basedata;
}

enum upcn_result rrnd_parser_reset(struct rrnd_parser *const parser)
{
	if (parser->basedata->status == PARSER_STATUS_GOOD &&
	    parser->stage == RRNDP_EXPECT_COMMAND_TYPE)
		return UPCN_OK;
	parser->basedata->status = PARSER_STATUS_GOOD;
	parser->basedata->flags = PARSER_FLAG_NONE;
	parser->stage = RRNDP_EXPECT_COMMAND_TYPE;
	if (parser->command == NULL) {
		parser->command = malloc(sizeof(struct rrnd_command));
		if (parser->command == NULL)
			return UPCN_FAIL;
		parser->command->gs = NULL;
		parser->command->source_gs = NULL;
		parser->command->tle = NULL;
	} else {
		if (parser->command->gs != NULL) {
			free(parser->command->gs);
			parser->command->gs = NULL;
		}
		if (parser->command->source_gs != NULL) {
			free(parser->command->source_gs);
			parser->command->source_gs = NULL;
		}
		if (parser->command->tle != NULL) {
			free(parser->command->tle);
			parser->command->tle = NULL;
		}
	}
	parser->command->type = RRND_COMMAND_NONE;
	return UPCN_OK;
}
