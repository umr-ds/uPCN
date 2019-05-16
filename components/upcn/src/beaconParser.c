#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "upcn/upcn.h"
#include "upcn/beaconParser.h"
#include "upcn/sdnv.h"
#include "upcn/eidManager.h"

static void bcp_send(struct beacon_parser *parser);

static inline void bcp_startsdnv16(struct beacon_parser *parser,
	uint16_t *target)
{
	sdnv_reset(&parser->sdnv_state);
	*target = 0;
	parser->cur.sdnv_u16 = target;
}

static inline void bcp_startsdnv32(struct beacon_parser *parser,
	uint32_t *target)
{
	sdnv_reset(&parser->sdnv_state);
	*target = 0;
	parser->cur.sdnv_u32 = target;
}

static inline void bcp_startsdnv64(struct beacon_parser *parser,
	uint64_t *target)
{
	sdnv_reset(&parser->sdnv_state);
	*target = 0;
	parser->cur.sdnv_u64 = target;
}

static inline int bcp_readsdnv16(
	struct beacon_parser *parser, const uint8_t byte)
{
	sdnv_read_u16(&parser->sdnv_state, parser->cur.sdnv_u16, byte);
	if (parser->sdnv_state.status == SDNV_DONE)
		return 1;
	else if (parser->sdnv_state.status == SDNV_ERROR)
		parser->basedata->status = PARSER_STATUS_ERROR;
	return 0;
}

static inline int bcp_readsdnv32(
	struct beacon_parser *parser, const uint8_t byte)
{
	sdnv_read_u32(&parser->sdnv_state, parser->cur.sdnv_u32, byte);
	if (parser->sdnv_state.status == SDNV_DONE)
		return 1;
	else if (parser->sdnv_state.status == SDNV_ERROR)
		parser->basedata->status = PARSER_STATUS_ERROR;
	return 0;
}

static inline int bcp_readsdnv64(
	struct beacon_parser *parser, const uint8_t byte)
{
	sdnv_read_u64(&parser->sdnv_state, parser->cur.sdnv_u64, byte);
	if (parser->sdnv_state.status == SDNV_DONE)
		return 1;
	else if (parser->sdnv_state.status == SDNV_ERROR)
		parser->basedata->status = PARSER_STATUS_ERROR;
	return 0;
}

static inline void bcp_tlv_init_frame(struct beacon_parser *parser)
{
	parser->tlv_cur->bytes_header = 0;
	parser->tlv_cur->bytes_value = 0;
	/* Initialize all bits to 0 (resulting type is BOOLEAN in this case) */
	memset(parser->tlv_cur->tlv, 0, sizeof(struct tlv_definition));
}

static int bcp_tlv_finish_frame(struct beacon_parser *parser,
	struct tlv_stack_frame *frame);

/* 1) at entering a service */
/* 2) after reading length of constructed type */
/* 3) after finishing a deeper frame (calls itself) */
static int bcp_tlv_next(struct beacon_parser *parser)
{
	int result = 1;

	if (parser->tlv_index == -1) {
		++parser->tlv_index;
		parser->tlv_cur = &parser->tlv_stack[parser->tlv_index];
		parser->tlv_cur->tlv = &parser->beacon->services[parser->index];
		bcp_tlv_init_frame(parser);
	} else {
		struct tlv_stack_frame *frame
			= &parser->tlv_stack[parser->tlv_index];
		if (frame->bytes_value < frame->tlv->length) {
			if (parser->tlv_index >= TLV_MAX_DEPTH - 1) {
				parser->basedata->status = PARSER_STATUS_ERROR;
				return 0;
			}

			struct tlv_definition **children
				= &(frame->tlv->value.children.list);
			uint16_t *children_count
				= &(frame->tlv->value.children.count);
			struct tlv_definition *new_children;

			if (*children_count == 0) {
				new_children = malloc(
					sizeof(struct tlv_definition));
			} else {
				new_children
					= realloc(*children,
						  sizeof(struct tlv_definition)
						  * (*children_count + 1));
			}
			if (new_children == NULL) {
				parser->basedata->status = PARSER_STATUS_ERROR;
				return result;
			}
			*children = new_children;
			++*children_count;
			++parser->tlv_index;
			parser->tlv_cur = &parser->tlv_stack[parser->tlv_index];
			parser->tlv_cur->tlv
				= &(*children)[*children_count - 1];
			bcp_tlv_init_frame(parser);
		} else if (frame->bytes_value == frame->tlv->length) {
			result = bcp_tlv_finish_frame(parser, frame);
		} else {
			parser->basedata->status = PARSER_STATUS_ERROR;
		}
	}
	return result;
}

static int bcp_tlv_finish_frame(struct beacon_parser *parser,
	struct tlv_stack_frame *frame)
{
	if (parser->tlv_index == 0) {
		/* Last frame finished */
		++parser->index;
		--parser->remaining;
		return 0;
	}
	--parser->tlv_index;
	parser->tlv_stack[parser->tlv_index].bytes_value
		+= frame->bytes_header + frame->bytes_value;
	return bcp_tlv_next(parser);
}

static void bcp_next(struct beacon_parser *parser,
	enum beacon_parser_stage next_stage)
{
	switch (next_stage) {
	case BCP_EXPECT_EID_LENGTH:
		bcp_startsdnv16(parser, &parser->remaining);
		break;
	case BCP_EXPECT_EID:
		parser->index = 0;
		if (parser->remaining > MAX_EID_LENGTH
				|| parser->remaining == 0)
			parser->beacon->eid = NULL;
		else
			parser->beacon->eid
				= malloc(parser->remaining + 1);
		if (parser->beacon->eid == NULL)
			parser->basedata->status = PARSER_STATUS_ERROR;
		else
			parser->cur.string = parser->beacon->eid;
		break;
	case BCP_EXPECT_SERVICENUM:
		bcp_startsdnv16(parser, &parser->beacon->service_count);
		break;
	case BCP_EXPECT_TLV_TAG: /* Only on start of first service */
		bcp_tlv_next(parser);
		break;
	case BCP_EXPECT_TLV_LENGTH:
		bcp_startsdnv16(parser, &parser->tlv_cur->tlv->length);
		break;
	case BCP_EXPECT_TLV_VALUE:
		if (!TLV_IS_PRIMITIVE(parser->tlv_cur->tlv->tag)) {
			bcp_tlv_next(parser);
			next_stage = BCP_EXPECT_TLV_TAG;
			break;
		}
		switch (parser->tlv_cur->tlv->tag) {
		case TLV_TYPE_BOOLEAN:
			break;
		case TLV_TYPE_UINT64:
		case TLV_TYPE_SINT64:
			bcp_startsdnv64(parser, &parser->tlv_cur->tlv
				->value.u64);
			break;
		case TLV_TYPE_FIXED16:
		case TLV_TYPE_FIXED32:
		case TLV_TYPE_FIXED64:
		case TLV_TYPE_FLOAT:
		case TLV_TYPE_DOUBLE:
			break;
		case TLV_TYPE_STRING:
		case TLV_TYPE_BYTES:
			if (parser->tlv_cur->tlv->length
				> MAX_TLV_VALUE_LENGTH
			) {
				parser->basedata->status
					= PARSER_STATUS_ERROR;
			} else {
				parser->tlv_cur->tlv->value.b
				= malloc(parser->tlv_cur->tlv->length);
			}
			break;
		default:
			break;
		}
		break;
	case BCP_EXPECT_PERIOD:
		parser->beacon->period = 0;
		bcp_startsdnv32(parser,
			(uint32_t *)&parser->beacon->period);
		break;
	default:
		break;
	}
	parser->stage = next_stage;
}

static void beacon_parser_read_byte(struct beacon_parser *parser, uint8_t byte)
{
	int tmp;
	enum beacon_flags f;

	if (parser->basedata->status != PARSER_STATUS_GOOD)
		return;
	switch (parser->stage) {
	case BCP_EXPECT_VERSION:
		parser->beacon->version = byte;
		if (byte != 0x04) /* not supported */
			parser->basedata->status = PARSER_STATUS_ERROR;
		parser->stage = BCP_EXPECT_FLAGS;
		break;
	case BCP_EXPECT_FLAGS:
		parser->beacon->flags = (enum beacon_flags)byte;
		parser->stage = BCP_EXPECT_SEQUENCE_NUMBER_MSB;
		break;
	case BCP_EXPECT_SEQUENCE_NUMBER_MSB:
		parser->beacon->sequence_number = (uint16_t)byte << 8;
		parser->stage = BCP_EXPECT_SEQUENCE_NUMBER_LSB;
		break;
	case BCP_EXPECT_SEQUENCE_NUMBER_LSB:
		parser->beacon->sequence_number |= byte;
		f = parser->beacon->flags;
		// Check for invalid flags
		if ((f & ~(BEACON_FLAG_HAS_EID |
			   BEACON_FLAG_HAS_SERVICES |
			   BEACON_FLAG_HAS_PERIOD |
			   BEACON_FLAG_HAS_NBF)) != BEACON_FLAG_NONE)
			parser->basedata->status = PARSER_STATUS_ERROR;
		if (HAS_FLAG(f, BEACON_FLAG_HAS_EID))
			bcp_next(parser, BCP_EXPECT_EID_LENGTH);
		else if (HAS_FLAG(f, BEACON_FLAG_HAS_SERVICES))
			bcp_next(parser, BCP_EXPECT_SERVICENUM);
		else if (HAS_FLAG(f, BEACON_FLAG_HAS_PERIOD))
			bcp_next(parser, BCP_EXPECT_PERIOD);
		else
			parser->basedata->status = PARSER_STATUS_DONE;
		break;
	case BCP_EXPECT_EID_LENGTH:
		if (bcp_readsdnv16(parser, byte))
			bcp_next(parser, BCP_EXPECT_EID);
		break;
	case BCP_EXPECT_EID:
		parser->beacon->eid[parser->index++] = *(char *)(&byte);
		if (--parser->remaining == 0) {
			parser->beacon->eid[parser->index] = '\0';
			parser->beacon->eid = eidmanager_alloc_ref(
				parser->beacon->eid, true);
			f = parser->beacon->flags;
			if (HAS_FLAG(f, BEACON_FLAG_HAS_SERVICES))
				bcp_next(parser, BCP_EXPECT_SERVICENUM);
			else if (HAS_FLAG(f, BEACON_FLAG_HAS_PERIOD))
				bcp_next(parser, BCP_EXPECT_PERIOD);
			else
				parser->basedata->status
					= PARSER_STATUS_DONE;
		}
		break;
	case BCP_EXPECT_SERVICENUM:
		if (bcp_readsdnv16(parser, byte)) {
			parser->remaining
				= parser->beacon->service_count;
			if (parser->remaining == 0 ||
			    parser->remaining > MAX_SVC_COUNT) {
				parser->basedata->status
					= PARSER_STATUS_ERROR;
			} else {
				parser->index = 0;
				parser->beacon->services
					= malloc(parser->remaining *
					sizeof(struct tlv_definition));
				if (parser->beacon->services == NULL) {
					parser->basedata->status
						= PARSER_STATUS_ERROR;
				} else {
					/* Set all tags to BOOLEAN
					 * b/c in case of reset, free
					 * has to work
					 */
					for (tmp = 0; tmp < parser
						->remaining; ++tmp
					) {
						parser->beacon
							->services[tmp]
							.tag = 0;
					}
					parser->tlv_index = -1;
					bcp_next(parser,
						BCP_EXPECT_TLV_TAG);
				}
			}
		}
		break;
	case BCP_EXPECT_TLV_TAG:
		parser->tlv_cur->tlv->tag = (enum tlv_type)byte;
		parser->tlv_cur->bytes_header++;
		if (TLV_HAS_LENGTH(parser->tlv_cur->tlv->tag))
			bcp_next(parser, BCP_EXPECT_TLV_LENGTH);
		else
			bcp_next(parser, BCP_EXPECT_TLV_VALUE);
		break;
	case BCP_EXPECT_TLV_LENGTH:
		parser->tlv_cur->bytes_header++;
		if (bcp_readsdnv16(parser, byte)) {
			if (parser->tlv_cur->tlv->length == 0)
				parser->basedata->status
					= PARSER_STATUS_ERROR;
			else
				bcp_next(parser, BCP_EXPECT_TLV_VALUE);
		}
		break;
	case BCP_EXPECT_TLV_VALUE: /* == PRIMITIVE_VALUE */
		tmp = 0; /* "finished" */
		parser->tlv_cur->bytes_value++;
		switch (parser->tlv_cur->tlv->tag) {
		case TLV_TYPE_BOOLEAN:
			parser->tlv_cur->tlv->value.u8 = byte;
			tmp = 1;
			break;
		case TLV_TYPE_UINT64:
		case TLV_TYPE_SINT64:
			if (bcp_readsdnv64(parser, byte))
				tmp = 1;
			break;
		case TLV_TYPE_FIXED16:
			parser->tlv_cur->tlv->value.u16 <<= 8;
			parser->tlv_cur->tlv->value.u16 |= byte;
			tmp = (parser->tlv_cur->bytes_value == 2);
			break;
		case TLV_TYPE_FIXED32:
		case TLV_TYPE_FLOAT:
			parser->tlv_cur->tlv->value.u32 <<= 8;
			parser->tlv_cur->tlv->value.u32 |= byte;
			tmp = (parser->tlv_cur->bytes_value == 4);
			break;
		case TLV_TYPE_FIXED64:
		case TLV_TYPE_DOUBLE:
			parser->tlv_cur->tlv->value.u64 <<= 8;
			parser->tlv_cur->tlv->value.u64 |= byte;
			tmp = (parser->tlv_cur->bytes_value == 8);
			break;
		case TLV_TYPE_STRING:
		case TLV_TYPE_BYTES:
			ASSERT(parser->tlv_cur->tlv->length != 0);
			if (parser->tlv_cur->tlv->length == 1
				&& byte == 0
			) {
				parser->tlv_cur->tlv->length = 0;
				free(parser->tlv_cur->tlv->value.b);
				parser->tlv_cur->tlv->value.b = NULL;
				tmp = 1;
			} else {
				parser->tlv_cur->tlv->value.b[
					parser->tlv_cur->bytes_value - 1]
						= byte;
				tmp = (parser->tlv_cur->bytes_value ==
					parser->tlv_cur->tlv->length);
			}
			break;
		default:
			parser->basedata->status = PARSER_STATUS_ERROR;
			break;
		}
		if (tmp) {
			if (bcp_tlv_finish_frame(parser, parser->tlv_cur)) {
				/* Some parent frame is not done
				 * Don't use bcp_next as frame has been
				 * created already...
				 */
				parser->stage = BCP_EXPECT_TLV_TAG;
			} else if (parser->remaining != 0) {
				/* Next service */
				parser->tlv_index = -1;
				bcp_next(parser, BCP_EXPECT_TLV_TAG);
			} else if (HAS_FLAG(parser->beacon->flags,
					BEACON_FLAG_HAS_PERIOD)
			) {
				bcp_next(parser, BCP_EXPECT_PERIOD);
			} else {
				parser->basedata->status = PARSER_STATUS_DONE;
			}
		}
		break;
	case BCP_EXPECT_PERIOD:
		if (bcp_readsdnv32(parser, byte))
			parser->basedata->status = PARSER_STATUS_DONE;
		break;
	default:
		parser->basedata->status = PARSER_STATUS_ERROR;
		break;
	}

	if (parser->basedata->status == PARSER_STATUS_DONE)
		bcp_send(parser);
}

size_t beacon_parser_read(struct beacon_parser *parser,
	const uint8_t *buffer, size_t length)
{
	size_t i = 0;

	while (i < length && parser->basedata->status == PARSER_STATUS_GOOD) {
		beacon_parser_read_byte(parser, buffer[i]);
		i++;
	}

	return i;

}

static void bcp_send(struct beacon_parser *parser)
{
	struct beacon *ptr;

	if (parser->send_callback == NULL)
		return;
	ptr = parser->beacon;
	if (ptr->eid)
		ptr->eid = eidmanager_alloc_ref(ptr->eid, 1);
	/* The recipient now takes care of the beacon... */
	parser->beacon = NULL;
	parser->send_callback(ptr);
}

enum upcn_result beacon_parser_reset(struct beacon_parser *parser)
{
	if (parser->basedata->status == PARSER_STATUS_GOOD &&
			parser->stage == BCP_EXPECT_VERSION)
		return UPCN_OK;
	parser->basedata->status = PARSER_STATUS_GOOD;
	parser->basedata->flags = PARSER_FLAG_NONE;
	parser->stage = BCP_EXPECT_VERSION;
	parser->remaining = 0;
	if (parser->beacon != NULL)
		beacon_free(parser->beacon);
	parser->beacon = beacon_init();
	if (parser->beacon == NULL)
		return UPCN_FAIL;
	return UPCN_OK;
}

struct parser *beacon_parser_init(
	struct beacon_parser *parser, void (*send_callback)(struct beacon *))
{
	parser->basedata = malloc(sizeof(struct parser));
	if (parser->basedata == NULL)
		return NULL;
	parser->basedata->status = PARSER_STATUS_ERROR;
	parser->send_callback = send_callback;
	parser->beacon = NULL;
	if (beacon_parser_reset(parser) != UPCN_OK)
		return NULL;
	return parser->basedata;
}
