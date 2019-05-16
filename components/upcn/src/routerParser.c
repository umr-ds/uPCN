#include <stddef.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include "upcn/upcn.h"
#include "upcn/routerParser.h"
#include "upcn/eidManager.h"

static char const EID_START_DELIMITER = '(';
static char const EID_END_DELIMITER = ')';
static char const CLA_ADDR_START_DELIMITER = '(';
static char const CLA_ADDR_END_DELIMITER = ')';
static char const GS_RELIABILITY_SEPARATOR = ',';
static char const GS_CLA_ADDR_SEPARATOR = ':';
static char const CLA_ADDR_NODES_SEPARATOR = ':';
static char const LIST_START_DELIMITER = '[';
static char const LIST_END_DELIMITER = ']';
static char const LIST_ELEMENT_SEPARATOR = ',';
static char const OBJECT_START_DELIMITER = '{';
static char const OBJECT_END_DELIMITER = '}';
static char const OBJECT_ELEMENT_SEPARATOR = ',';
static char const NODES_CONTACTS_SEPARATOR = ':';

static uint8_t const COMMAND_END_MARKER = 0xFF;

static uint8_t const DEFAULT_EID_BUFFER_SIZE = 16;
static uint8_t const DEFAULT_CLA_ADDR_BUFFER_SIZE = 16;
static uint8_t const DEFAULT_INT_BUFFER_SIZE = 16;

static void send_router_command(struct router_parser *parser);

static void begin_read_data_eid(
	struct router_parser *parser, struct endpoint_list **target)
{
	struct endpoint_list *new_entry = malloc(sizeof(struct endpoint_list));
	char *new_eid = malloc(DEFAULT_EID_BUFFER_SIZE * sizeof(char));

	new_eid[0] = '\0';
	new_entry->eid = new_eid;
	new_entry->p = 1.0f;
	new_entry->next = NULL;

	if (parser->current_eid == NULL)
		*target = new_entry;
	else
		parser->current_eid->next = new_entry;
	parser->current_eid = new_entry;
	parser->current_index = 0;
}

static void begin_read_gs_eid(struct router_parser *parser)
{
	parser->router_command->data->eid =
		malloc(DEFAULT_EID_BUFFER_SIZE * sizeof(char));
	parser->router_command->data->eid[0] = '\0';
	parser->current_index = 0;
}

static bool read_eid(
	struct router_parser *parser, char **eid_ptr, const char byte)
{
	/* Check for valid chars (URIs) */
	if (
		/* !"#$%&'()*+,-./0123456789:<=>?@ / A-Z / [\]^_ */
		(byte >= 0x21 && byte <= 0x5F) ||
		/* a-z / ~ */
		(byte >= 0x61 && byte <= 0x7A) || byte == 0x7E
	) {
		(*eid_ptr)[parser->current_index++] = byte;
		if (parser->current_index >= DEFAULT_EID_BUFFER_SIZE)
			(*eid_ptr) = realloc(*eid_ptr,
				(parser->current_index + 1) * sizeof(char));
		(*eid_ptr)[parser->current_index] = '\0';
		return true;
	} else {
		return false;
	}
}

static void end_read_eid(struct router_parser *parser, char **eid_ptr)
{
	(*eid_ptr)[parser->current_index] = '\0';
	*eid_ptr = eidmanager_alloc_ref(*eid_ptr, true);
}

static void begin_read_cla_addr(struct router_parser *parser)
{
	parser->router_command->data->cla_addr =
		malloc(DEFAULT_CLA_ADDR_BUFFER_SIZE * sizeof(char));
	parser->router_command->data->cla_addr[0] = '\0';
	parser->current_index = 0;
}

static void end_read_cla_addr(struct router_parser *parser, char **cla_addr_ptr)
{
	(*cla_addr_ptr)[parser->current_index] = '\0';
}

static bool read_cla_addr(
	struct router_parser *parser, char **cla_addr_ptr, const char byte)
{
	if (
		(byte >= '0' && byte <= '9') ||
		(byte >= 'A' && byte <= 'Z') ||
		(byte >= 'a' && byte <= 'z')
	) {
		(*cla_addr_ptr)[parser->current_index++] = byte;
		if (parser->current_index >= DEFAULT_CLA_ADDR_BUFFER_SIZE)
			(*cla_addr_ptr) = realloc(*cla_addr_ptr,
				(parser->current_index + 1) * sizeof(char));
		return true;
	} else {
		return false;
	}
}

static void begin_read_contact(struct router_parser *parser)
{
	struct contact_list *new_entry = malloc(sizeof(struct contact_list));

	new_entry->next = NULL;
	new_entry->data = contact_create(parser->router_command->data);
	if (parser->current_contact == NULL)
		parser->router_command->data->contacts = new_entry;
	else
		parser->current_contact->next = new_entry;
	parser->current_contact = new_entry;
	/* Reset for contact eids, misc eid reading has finished here */
	parser->current_eid = NULL;
}

static void begin_read_integer(struct router_parser *parser)
{
	parser->current_int_data
		= malloc(DEFAULT_INT_BUFFER_SIZE * sizeof(char));
	parser->current_index = 0;
}

static bool read_integer(struct router_parser *parser, const char byte)
{
	if (byte >= 0x30 && byte <= 0x39) { /* 0..9 */
		parser->current_int_data[parser->current_index++] = byte;
		if (parser->current_index >= DEFAULT_INT_BUFFER_SIZE)
			parser->current_int_data = realloc(
				parser->current_int_data,
				(parser->current_index + 1) * sizeof(char));
		return true;
	} else {
		return false;
	}
}

static void end_read_uint64(struct router_parser *parser, uint64_t *out)
{
	parser->current_int_data[parser->current_index] = '\0';
	/* No further checks because read_integer already checks for digits */
	*out = strtoull(parser->current_int_data, NULL, 10);
	free(parser->current_int_data);
	parser->current_int_data = NULL;
}

static void end_read_uint32(struct router_parser *parser, uint32_t *out)
{
	parser->current_int_data[parser->current_index] = '\0';
	/* No further checks because read_integer already checks for digits */
	*out = strtoul(parser->current_int_data, NULL, 10);
	free(parser->current_int_data);
	parser->current_int_data = NULL;
}

static void end_read_uint16(struct router_parser *parser, uint16_t *out)
{
	parser->current_int_data[parser->current_index] = '\0';
	/* No further checks because read_integer already checks for digits */
	/* XXX: Overflow might be possible here */
	*out = (uint16_t)atoi(parser->current_int_data);
	free(parser->current_int_data);
	parser->current_int_data = NULL;
}

static void read_command(struct router_parser *parser, const uint8_t byte)
{
	uint16_t tmp;
	struct ground_station *cur_gs = parser->router_command->data;

	switch (parser->stage) {
	default:
		parser->basedata->status = PARSER_STATUS_ERROR;
		break;
	case RP_EXPECT_GS_START_DELIMITER:
		if (byte == EID_START_DELIMITER) {
			begin_read_gs_eid(parser);
			parser->stage = RP_EXPECT_GS_EID;
		} else {
			parser->basedata->status = PARSER_STATUS_ERROR;
		}
		break;
	case RP_EXPECT_GS_EID:
		if (byte == EID_END_DELIMITER) {
			end_read_eid(parser, &(cur_gs->eid));
			parser->stage = RP_EXPECT_GS_RELIABILITY_SEPARATOR;
		} else if (!read_eid(parser, &(cur_gs->eid), byte)) {
			parser->basedata->status = PARSER_STATUS_ERROR;
		}
		break;
	case RP_EXPECT_GS_RELIABILITY_SEPARATOR:
		if (byte == GS_RELIABILITY_SEPARATOR) {
			begin_read_integer(parser);
			parser->stage = RP_EXPECT_GS_RELIABILITY;
		} else if (byte == GS_CLA_ADDR_SEPARATOR) {
			parser->stage = RP_EXPECT_CLA_ADDR_START_DELIMITER;
		} else if (byte == COMMAND_END_MARKER) {
			parser->basedata->status = PARSER_STATUS_DONE;
		} else {
			parser->basedata->status = PARSER_STATUS_ERROR;
		}
		break;
	case RP_EXPECT_GS_RELIABILITY:
		if (byte == GS_CLA_ADDR_SEPARATOR
			|| byte == COMMAND_END_MARKER
		) {
			end_read_uint16(parser, &tmp);
			if (tmp < 100 || tmp > 1000) {
				parser->basedata->status = PARSER_STATUS_ERROR;
				break;
			}
			cur_gs->rrnd_info
				= ground_station_rrnd_info_create(cur_gs);
			if (!cur_gs->rrnd_info) {
				parser->basedata->status = PARSER_STATUS_ERROR;
				break;
			}
			cur_gs->rrnd_info->prob_metrics.reliability
				= (float)tmp / 1000.0f;
			parser->stage = RP_EXPECT_CLA_ADDR_START_DELIMITER;
			if (byte == COMMAND_END_MARKER)
				parser->basedata->status = PARSER_STATUS_DONE;
		} else if (!read_integer(parser, byte)) {
			parser->basedata->status = PARSER_STATUS_ERROR;
		}
		break;
	case RP_EXPECT_GS_CLA_ADDR_SEPARATOR:
		if (byte == GS_CLA_ADDR_SEPARATOR)
			parser->stage = RP_EXPECT_CLA_ADDR_START_DELIMITER;
		else if (byte == COMMAND_END_MARKER)
			parser->basedata->status = PARSER_STATUS_DONE;
		else
			parser->basedata->status = PARSER_STATUS_ERROR;
		break;
	case RP_EXPECT_CLA_ADDR_START_DELIMITER:
		if (byte == CLA_ADDR_START_DELIMITER) {
			begin_read_cla_addr(parser);
			parser->stage = RP_EXPECT_CLA_ADDR;
		} else if (byte == CLA_ADDR_NODES_SEPARATOR) {
			parser->stage = RP_EXPECT_NODE_LIST_START_DELIMITER;
		} else if (byte == COMMAND_END_MARKER) {
			parser->basedata->status = PARSER_STATUS_DONE;
		} else {
			parser->basedata->status = PARSER_STATUS_ERROR;
		}
		break;
	case RP_EXPECT_CLA_ADDR:
		if (byte == CLA_ADDR_END_DELIMITER) {
			end_read_cla_addr(parser, &(cur_gs->cla_addr));
			parser->stage = RP_EXPECT_CLA_ADDR_NODES_SEPARATOR;
		} else if (!read_cla_addr(parser, &(cur_gs->cla_addr), byte)) {
			parser->basedata->status = PARSER_STATUS_ERROR;
		}
		break;
	case RP_EXPECT_CLA_ADDR_NODES_SEPARATOR:
		if (byte == CLA_ADDR_NODES_SEPARATOR)
			parser->stage = RP_EXPECT_NODE_LIST_START_DELIMITER;
		else if (byte == COMMAND_END_MARKER)
			parser->basedata->status = PARSER_STATUS_DONE;
		else
			parser->basedata->status = PARSER_STATUS_ERROR;
		break;
	case RP_EXPECT_NODE_LIST_START_DELIMITER:
		if (byte == LIST_START_DELIMITER)
			parser->stage = RP_EXPECT_NODE_START_DELIMITER;
		else if (byte == NODES_CONTACTS_SEPARATOR)
			parser->stage = RP_EXPECT_CONTACT_LIST_START_DELIMITER;
		else if (byte == COMMAND_END_MARKER)
			parser->basedata->status = PARSER_STATUS_DONE;
		else
			parser->basedata->status = PARSER_STATUS_ERROR;
		break;
	case RP_EXPECT_NODE_START_DELIMITER:
		if (byte == EID_START_DELIMITER) {
			begin_read_data_eid(parser, &(cur_gs->endpoints));
			parser->stage = RP_EXPECT_NODE_EID;
		} else if (byte == LIST_END_DELIMITER) {
			parser->stage = RP_EXPECT_NODES_CONTACTS_SEPARATOR;
		} else {
			parser->basedata->status = PARSER_STATUS_ERROR;
		}
		break;
	case RP_EXPECT_NODE_EID:
		if (byte == EID_END_DELIMITER) {
			end_read_eid(parser, &(parser->current_eid->eid));
			parser->stage = RP_EXPECT_NODE_SEPARATOR;
		} else if (!read_eid(parser,
			&(parser->current_eid->eid), byte)
		) {
			parser->basedata->status = PARSER_STATUS_ERROR;
		}
		break;
	case RP_EXPECT_NODE_SEPARATOR:
		if (byte == LIST_ELEMENT_SEPARATOR)
			parser->stage = RP_EXPECT_NODE_START_DELIMITER;
		else if (byte == LIST_END_DELIMITER)
			parser->stage = RP_EXPECT_NODES_CONTACTS_SEPARATOR;
		else
			parser->basedata->status = PARSER_STATUS_ERROR;
		break;
	case RP_EXPECT_NODES_CONTACTS_SEPARATOR:
		if (byte == NODES_CONTACTS_SEPARATOR)
			parser->stage = RP_EXPECT_CONTACT_LIST_START_DELIMITER;
		else if (byte == COMMAND_END_MARKER)
			parser->basedata->status = PARSER_STATUS_DONE;
		else
			parser->basedata->status = PARSER_STATUS_ERROR;
		break;
	case RP_EXPECT_CONTACT_LIST_START_DELIMITER:
		if (byte == LIST_START_DELIMITER)
			parser->stage = RP_EXPECT_CONTACT_START_DELIMITER;
		else if (byte == COMMAND_END_MARKER)
			parser->basedata->status = PARSER_STATUS_DONE;
		else
			parser->basedata->status = PARSER_STATUS_ERROR;
		break;
	case RP_EXPECT_CONTACT_START_DELIMITER:
		if (byte == OBJECT_START_DELIMITER) {
			begin_read_contact(parser);
			begin_read_integer(parser);
			parser->stage = RP_EXPECT_CONTACT_START_TIME;
		} else if (byte == LIST_END_DELIMITER) {
			parser->stage = RP_EXPECT_COMMAND_END_MARKER;
		} else {
			parser->basedata->status = PARSER_STATUS_ERROR;
		}
		break;
	case RP_EXPECT_CONTACT_START_TIME:
		if (byte == OBJECT_ELEMENT_SEPARATOR) {
			end_read_uint64(parser,
				&(parser->current_contact->data->from));
			begin_read_integer(parser);
			parser->stage = RP_EXPECT_CONTACT_END_TIME;
		} else if (!read_integer(parser, byte)) {
			parser->basedata->status = PARSER_STATUS_ERROR;
		}
		break;
	case RP_EXPECT_CONTACT_END_TIME:
		if (byte == OBJECT_ELEMENT_SEPARATOR) {
			end_read_uint64(parser,
				&(parser->current_contact->data->to));
			if (parser->current_contact->data->to >
				parser->current_contact->data->from
				&& parser->current_contact->data->to >
				hal_time_get_timestamp_s()
			) {
				begin_read_integer(parser);
				parser->stage = RP_EXPECT_CONTACT_BITRATE;
			} else {
				parser->basedata->status = PARSER_STATUS_ERROR;
			}
		} else if (!read_integer(parser, byte)) {
			parser->basedata->status = PARSER_STATUS_ERROR;
		}
		break;
	case RP_EXPECT_CONTACT_BITRATE:
		if (byte == OBJECT_ELEMENT_SEPARATOR) {
			end_read_uint32(parser,
				&(parser->current_contact->data->bitrate));
			parser->stage
				= RP_EXPECT_CONTACT_NODE_LIST_START_DELIMITER;
		} else if (byte == OBJECT_END_DELIMITER) {
			end_read_uint32(parser,
				&(parser->current_contact->data->bitrate));
			parser->stage = RP_EXPECT_CONTACT_SEPARATOR;
		} else if (!read_integer(parser, byte)) {
			parser->basedata->status = PARSER_STATUS_ERROR;
		}
		break;
	case RP_EXPECT_CONTACT_NODE_LIST_START_DELIMITER:
		if (byte == LIST_START_DELIMITER)
			parser->stage = RP_EXPECT_CONTACT_NODE_START_DELIMITER;
		else
			parser->basedata->status = PARSER_STATUS_ERROR;
		break;
	case RP_EXPECT_CONTACT_NODE_START_DELIMITER:
		if (byte == EID_START_DELIMITER) {
			begin_read_data_eid(parser, &(parser->current_contact
				->data->contact_endpoints));
			parser->stage = RP_EXPECT_CONTACT_NODE_EID;
		} else if (byte == LIST_END_DELIMITER) {
			parser->stage = RP_EXPECT_CONTACT_END_DELIMITER;
		} else {
			parser->basedata->status = PARSER_STATUS_ERROR;
		}
		break;
	case RP_EXPECT_CONTACT_NODE_EID:
		if (byte == EID_END_DELIMITER) {
			end_read_eid(parser, &(parser->current_eid->eid));
			parser->stage = RP_EXPECT_CONTACT_NODE_SEPARATOR;
		} else if (!read_eid(parser,
			&(parser->current_eid->eid), byte)
		) {
			parser->basedata->status = PARSER_STATUS_ERROR;
		}
		break;
	case RP_EXPECT_CONTACT_NODE_SEPARATOR:
		if (byte == LIST_ELEMENT_SEPARATOR)
			parser->stage = RP_EXPECT_CONTACT_NODE_START_DELIMITER;
		else if (byte == LIST_END_DELIMITER)
			parser->stage = RP_EXPECT_CONTACT_END_DELIMITER;
		else
			parser->basedata->status = PARSER_STATUS_ERROR;
		break;
	case RP_EXPECT_CONTACT_END_DELIMITER:
		if (byte == OBJECT_END_DELIMITER)
			parser->stage = RP_EXPECT_CONTACT_SEPARATOR;
		else
			parser->basedata->status = PARSER_STATUS_ERROR;
		break;
	case RP_EXPECT_CONTACT_SEPARATOR:
		if (byte == LIST_ELEMENT_SEPARATOR)
			parser->stage = RP_EXPECT_CONTACT_START_DELIMITER;
		else if (byte == LIST_END_DELIMITER)
			parser->stage = RP_EXPECT_COMMAND_END_MARKER;
		else
			parser->basedata->status = PARSER_STATUS_ERROR;
		break;
	case RP_EXPECT_COMMAND_END_MARKER:
		if (byte == COMMAND_END_MARKER)
			parser->basedata->status = PARSER_STATUS_DONE;
		else
			parser->basedata->status = PARSER_STATUS_ERROR;
		break;
	}
}

static void router_parser_read_byte(struct router_parser *parser, uint8_t byte)
{
	if (parser->basedata->status != PARSER_STATUS_GOOD)
		return;

	if (parser->stage == RP_EXPECT_COMMAND_TYPE) {
		parser->stage = RP_EXPECT_GS_START_DELIMITER;
		if (byte >= (uint8_t)ROUTER_COMMAND_ADD
			&& byte <= (uint8_t)ROUTER_COMMAND_QUERY
		) {
			parser->router_command->type
				= (enum router_command_type)byte;
		} else {
			parser->basedata->status = PARSER_STATUS_ERROR;
		}
	} else {
		read_command(parser, *(char *)(&byte));
	}

	if (parser->basedata->status == PARSER_STATUS_DONE)
		send_router_command(parser);
}

size_t router_parser_read(struct router_parser *parser,
	const uint8_t *buffer, size_t length)
{
	size_t i = 0;

	while (i < length && parser->basedata->status == PARSER_STATUS_GOOD) {
		router_parser_read_byte(parser, buffer[i]);
		i++;
	}

	return i;
}

/* add = true:
 *   Allocate all EIDs using the EID manager: We can't do this earlier
 *   as we always want to use free() on them to prevent double-free
 *   race conditions.
 * add = false:
 *   Free all EIDs, without EID manager.
 */

static char *process_unmanaged_eid(char *eid, const bool add)
{
	if (add)
		return eidmanager_alloc_ref(eid, 1);
	free(eid);
	return NULL;
}

static void process_unmanaged_eid_list(struct endpoint_list *el, const bool add)
{
	while (el) {
		el->eid = process_unmanaged_eid(el->eid, add);
		el = el->next;
	}
}

void process_unmanaged_eids(struct ground_station *gs, const bool add)
{
	struct contact_list *cl;

	gs->eid = process_unmanaged_eid(gs->eid, add);
	process_unmanaged_eid_list(gs->endpoints, add);
	cl = gs->contacts;
	while (cl) {
		process_unmanaged_eid_list(cl->data->contact_endpoints, add);
		cl = cl->next;
	}
}

enum upcn_result router_parser_reset(struct router_parser *parser)
{
	if (parser->basedata->status == PARSER_STATUS_GOOD &&
			parser->stage == RP_EXPECT_COMMAND_TYPE)
		return UPCN_OK;
	else if (parser->basedata->status == PARSER_STATUS_ERROR)
		(void)parser;
	parser->basedata->status = PARSER_STATUS_GOOD;
	parser->basedata->flags = PARSER_FLAG_NONE;
	parser->stage = RP_EXPECT_COMMAND_TYPE;
	parser->current_eid = NULL;
	parser->current_contact = NULL;
	if (parser->router_command != NULL) {
		process_unmanaged_eids(parser->router_command->data, false);
		free_ground_station(parser->router_command->data);
		free(parser->router_command);
		parser->router_command = NULL;
	}
	if (parser->current_int_data != NULL) {
		free(parser->current_int_data);
		parser->current_int_data = NULL;
	}
	parser->router_command = malloc(sizeof(struct router_command));
	if (parser->router_command == NULL)
		return UPCN_FAIL;
	parser->router_command->type = ROUTER_COMMAND_UNDEFINED;
	parser->router_command->data = ground_station_create(NULL);
	if (parser->router_command->data == NULL)
		return UPCN_FAIL;
	return UPCN_OK;
}

struct parser *router_parser_init(
	struct router_parser *parser,
	void (*send_callback)(struct router_command *))
{
	parser->basedata = malloc(sizeof(struct parser));
	if (parser->basedata == NULL)
		return NULL;
	parser->send_callback = send_callback;
	parser->basedata->status = PARSER_STATUS_ERROR;
	parser->router_command = NULL;
	parser->current_int_data = NULL;
	if (router_parser_reset(parser) != UPCN_OK)
		return NULL;
	return parser->basedata;
}

static void send_router_command(struct router_parser *parser)
{
	struct router_command *ptr;

	if (parser->send_callback == NULL)
		return;
	ptr = parser->router_command;
	process_unmanaged_eids(parser->router_command->data, true);
	/* Unset router cmd, the recipient has to take care of it... */
	parser->router_command = NULL;
	parser->send_callback(ptr);
	/* Don't reset the parser here as the input task must know the state */
}
