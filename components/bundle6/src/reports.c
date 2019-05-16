#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "upcn/upcn.h"
#include "upcn/sdnv.h"
#include "drv/mini-printf.h"
#include "hal_time.h"
#include "upcn/parser.h"
#include "upcn/reportManager.h"
#include "bundle6/eid.h"
#include "bundle6/reports.h"


// -------------------------------
// Administrative Record Generator
// -------------------------------

static struct bundle *encapsulate_record(
	const struct bundle * const bundle,
	const struct eid_reference dest,
	uint8_t *payload, const int payload_len)
{
	static const char my_scheme[] = UPCN_SCHEME;
	static const char my_ssp[] = UPCN_SSP;
	uint16_t dest_sch_len, dest_ssp_len, second_eid_off, third_eid_off;
	int32_t lifetime;
	uint32_t dict_len;
	char *dict, *cur;
	struct bundle *ret;

	if (strcmp(bundle->destination, "dtn:none") == 0
		|| strcmp(bundle->destination, UPCN_SCHEME":"UPCN_SSP)
	) {
		return NULL;
	}
	ret = bundle_init();
	if (ret == NULL)
		return NULL;
	ret->proc_flags
		= BUNDLE_FLAG_ADMINISTRATIVE_RECORD
		| BUNDLE_V6_FLAG_SINGLETON_ENDPOINT;
	ret->ret_constraints = BUNDLE_RET_CONSTRAINT_FLAG_OWN;
	/* Dictionary */
	dest_sch_len = strlen(bundle->dict + dest.scheme_offset);
	dest_ssp_len = strlen(bundle->dict + dest.ssp_offset);
	dict_len = sizeof(my_scheme) + sizeof(my_ssp) + 2
		+ dest_sch_len + dest_ssp_len + 2
		+ 3 + 1 + 4 + 1;
	dict = (char *)malloc(dict_len);
	if (dict == NULL) {
		free(ret);
		return NULL;
	}
	cur = dict;
	memcpy(cur, my_scheme, sizeof(my_scheme));
	cur += sizeof(my_scheme);
	(*cur++) = '\0';
	memcpy(cur, my_ssp, sizeof(my_ssp));
	cur += sizeof(my_ssp);
	(*cur++) = '\0';
	memcpy(cur, bundle->dict + dest.scheme_offset, dest_sch_len);
	cur += dest_sch_len;
	(*cur++) = '\0';
	memcpy(cur, bundle->dict + dest.ssp_offset, dest_ssp_len);
	cur += dest_ssp_len;
	(*cur++) = '\0';
	mini_snprintf(cur, 4, "dtn");
	cur += 3;
	(*cur++) = '\0';
	mini_snprintf(cur, 5, "none");
	ret->dict_length = dict_len;
	ret->dict = dict;
	/* Dict offsets */
	second_eid_off = sizeof(my_scheme) + sizeof(my_ssp) + 2;
	third_eid_off = second_eid_off + dest_sch_len + dest_ssp_len + 2;
	ret->destination_eid.scheme_offset = second_eid_off;
	ret->destination_eid.ssp_offset = third_eid_off - dest_ssp_len - 1;
	ret->source_eid.scheme_offset = 0;
	ret->source_eid.ssp_offset = sizeof(my_scheme) + 1;
	ret->report_eid.scheme_offset = third_eid_off;
	ret->report_eid.ssp_offset = third_eid_off + 4;
	ret->custodian_eid.scheme_offset = third_eid_off;
	ret->custodian_eid.ssp_offset = third_eid_off + 4;
	/* Others */
	ret->creation_timestamp = hal_time_get_timestamp_s();
	ret->sequence_number = 1;
	lifetime = bundle->lifetime /* ?? */
		- ret->creation_timestamp + bundle->creation_timestamp;
	ret->lifetime = (lifetime > 0) ? (uint32_t)lifetime : 0;
	struct bundle_block *pl_block
		= bundle_block_create(BUNDLE_BLOCK_TYPE_PAYLOAD);
	if (pl_block == NULL) {
		free(dict);
		free(ret);
		return NULL;
	}
	pl_block->flags = BUNDLE_BLOCK_FLAG_LAST_BLOCK;
	pl_block->length = payload_len;
	pl_block->data = payload;
	ret->blocks = bundle_block_entry_create(pl_block);
	if (ret->blocks == NULL) {
		bundle_block_free(pl_block);
		free(dict);
		free(ret);
		return NULL;
	}
	ret->payload_block = pl_block;
	bundle_recalculate_header_length(ret);
	return ret;
}


#define LENGTH_MAX_SIZE 4
#define DTN_TIME_MAX_SIZE 9
#define SEQ_NUM_MAX_SIZE 4
#define EID_LENGTH_MAX_SIZE 2
#define EID_DEFAULT_LENGTH 40

#define ADMINISTRATIVE_HEADER_SIZE 1
#define ADMINISTRATVE_RECORD_MAX_SIZE \
	(ADMINISTRATIVE_HEADER_SIZE \
	+ 2 + (LENGTH_MAX_SIZE * 2) + (DTN_TIME_MAX_SIZE * 2) \
	+ SEQ_NUM_MAX_SIZE + EID_LENGTH_MAX_SIZE + EID_DEFAULT_LENGTH)


static struct bundle *generate_record(
		const struct bundle * const bundle,
		const struct eid_reference dest, const bool is_custody_signal,
		const uint8_t prefix_1, const uint8_t prefix_2)
{
	uint8_t *buffer = (uint8_t *)malloc(ADMINISTRATVE_RECORD_MAX_SIZE);
	uint8_t *cur = buffer;
	bool fragment;
	uint16_t sch_len, ssp_len, eid_length, cur_length;
	uint64_t millitime;
	struct eid_reference eid;
	struct bundle *ret;

	if (buffer == NULL)
		return NULL;

	/* Write record type, flags, prefixes (status flags, reason, ...) */
	fragment = bundle_is_fragmented(bundle);
	(*cur++) = (is_custody_signal ? 0x20 : 0x10) | (fragment ? 0x01 : 0x00);
	(*cur++) = prefix_1;
	if (is_custody_signal)
		(*cur++) = prefix_2;
	/* Write fragment info if present */
	if (fragment) {
		cur += sdnv_write_u32(cur, bundle->fragment_offset);
		cur += sdnv_write_u32(cur, bundle->payload_block->length);
	}
	/* Use the current time as generation time, append bundle times */
	millitime = hal_time_get_timestamp_ms();
	/* Add a "DTN time": 1) TS, 2) Nanoseconds since start of cur. second */
	cur += sdnv_write_u64(cur, (millitime / 1000));
	cur += sdnv_write_u32(cur, (millitime % 1000) * 1000); /* "ns" */
	/* Copy bundle data */
	cur += sdnv_write_u32(cur, bundle->creation_timestamp);
	cur += sdnv_write_u32(cur, bundle->sequence_number);
	/* Bundle source EID (length + data) */
	eid = bundle->source_eid;
	sch_len = strlen(bundle->dict + eid.scheme_offset);
	ssp_len = strlen(bundle->dict + eid.ssp_offset);
	eid_length = sch_len + ssp_len + 1;
	cur += sdnv_write_u32(cur, eid_length);
	cur_length = cur - buffer;
	buffer = realloc(buffer, cur_length + eid_length);
	cur = buffer + cur_length;
	memcpy(cur, bundle->dict + eid.scheme_offset, sch_len);
	cur += sch_len;
	(*cur++) = ':';
	memcpy(cur, bundle->dict + eid.ssp_offset, ssp_len);
	cur += ssp_len;
	/* Build the bundle around our generated payload */
	ret = encapsulate_record(bundle, dest, buffer, cur - buffer);
	if (ret == NULL)
		free(buffer);
	return ret;
}


struct bundle *bundle6_generate_status_report(
	const struct bundle * const bundle,
	const struct bundle_status_report *report)
{
	return generate_record(bundle, bundle->report_eid, true,
		(uint8_t)(report->status), (uint8_t)(report->reason));
}


struct bundle *bundle6_generate_custody_signal(
	const struct bundle * const bundle,
	const struct bundle_custody_signal *signal)
{
	return generate_record(bundle, bundle->custodian_eid, false,
		((uint8_t)(signal->reason) & 0x7F)
			| ((signal->type == BUNDLE_CS_TYPE_ACCEPTANCE)
				? 0x80 : 0x00),
		0);
}


// ---------------------------------------
// Administrative Record Parser (RFC 5050)
// ---------------------------------------

struct record_parser {
	enum parser_status status;
	enum record_parser_stage {
		RP_EXPECT_TYPE,
		RP_EXPECT_REPORT_FLAGS,
		RP_EXPECT_REPORT_REASON,
		RP_EXPECT_CUSTODY_STATUS,
		RP_EXPECT_FRAGMENT_OFFSET,
		RP_EXPECT_FRAGMENT_LENGTH,
		RP_EXPECT_RECORD_DTN_SECONDS,
		RP_EXPECT_RECORD_DTN_NANOSECONDS,
		RP_EXPECT_BUNDLE_CREATION_TIMESTAMP,
		RP_EXPECT_BUNDLE_CREATION_SEQUENCE,
		RP_EXPECT_BUNDLE_SOURCE_LENGTH,
		RP_EXPECT_BUNDLE_SOURCE_EID
	} stage;
	struct sdnv_state sdnv_state;
	struct bundle_administrative_record *record;
	uint16_t current_index, bytes_remaining;
};

static enum upcn_result record_parser_init(struct record_parser *parser)
{
	parser->status = PARSER_STATUS_GOOD;
	parser->stage = RP_EXPECT_TYPE;
	parser->record = malloc(sizeof(struct bundle_administrative_record));
	if (parser->record == NULL)
		return UPCN_FAIL;
	parser->record->status_report = NULL;
	parser->record->custody_signal = NULL;
	parser->record->fragment_offset = 0;
	parser->record->fragment_length = 0;
	parser->record->event_timestamp = 0;
	parser->record->event_nanoseconds = 0;
	parser->record->bundle_creation_timestamp = 0;
	parser->record->bundle_sequence_number = 0;
	parser->record->bundle_source_eid_length = 0;
	parser->record->bundle_source_eid = NULL;
	return UPCN_OK;
}

static void record_parser_next(
	struct record_parser *parser, enum record_parser_stage next)
{
	parser->stage = next;
	sdnv_reset(&parser->sdnv_state);
}

static int record_parser_wait_for_sdnv(
	struct record_parser *parser, enum record_parser_stage next)
{
	switch (parser->sdnv_state.status) {
	case SDNV_IN_PROGRESS:
		break;
	case SDNV_DONE:
		record_parser_next(parser, next);
		return 1;
	case SDNV_ERROR:
		parser->status = PARSER_STATUS_ERROR;
		break;
	}
	return 0;
}

static void record_parser_read_byte(struct record_parser *parser, uint8_t byte)
{
	switch (parser->stage) {
	case RP_EXPECT_TYPE:
		parser->record->type = (byte >> 4) & 0x0F;
		parser->record->flags = byte & 0x0F;
		if (parser->record->type == BUNDLE_AR_STATUS_REPORT) {
			parser->record->status_report =
				(struct bundle_status_report *)malloc(
				sizeof(struct bundle_status_report));
			record_parser_next(
				parser, RP_EXPECT_REPORT_FLAGS);
		} else if (parser->record->type
			== BUNDLE_AR_CUSTODY_SIGNAL
		) {
			parser->record->custody_signal =
				(struct bundle_custody_signal *)malloc(
				sizeof(struct bundle_custody_signal));
			record_parser_next(
				parser, RP_EXPECT_CUSTODY_STATUS);
		} else {
			/* Can't parse other types */
			parser->status = PARSER_STATUS_ERROR;
		}
		break;
	case RP_EXPECT_REPORT_FLAGS:
		parser->record->status_report->status = byte;
		record_parser_next(parser, RP_EXPECT_REPORT_REASON);
		break;
	case RP_EXPECT_REPORT_REASON:
		parser->record->status_report->reason = byte;
		if (HAS_FLAG(parser->record->flags,
			BUNDLE_AR_FLAG_FRAGMENT)
		) {
			record_parser_next(parser,
				RP_EXPECT_FRAGMENT_OFFSET);
		} else {
			record_parser_next(parser,
				RP_EXPECT_RECORD_DTN_SECONDS);
		}
		break;
	case RP_EXPECT_CUSTODY_STATUS:
		parser->record->custody_signal->type = (byte >> 7)
			? BUNDLE_CS_TYPE_ACCEPTANCE
			: BUNDLE_CS_TYPE_REFUSAL;
		parser->record->custody_signal->reason = byte & 0x7F;
		if (HAS_FLAG(parser->record->flags,
				BUNDLE_AR_FLAG_FRAGMENT)
		) {
			record_parser_next(parser,
				RP_EXPECT_FRAGMENT_OFFSET);
		} else {
			record_parser_next(parser,
				RP_EXPECT_RECORD_DTN_SECONDS);
		}
		break;
	case RP_EXPECT_FRAGMENT_OFFSET:
		sdnv_read_u32(&parser->sdnv_state,
			&parser->record->fragment_offset, byte);
		record_parser_wait_for_sdnv(
			parser, RP_EXPECT_FRAGMENT_LENGTH);
		break;
	case RP_EXPECT_FRAGMENT_LENGTH:
		sdnv_read_u32(&parser->sdnv_state,
			&parser->record->fragment_length, byte);
		record_parser_wait_for_sdnv(
			parser, RP_EXPECT_RECORD_DTN_SECONDS);
		break;
	case RP_EXPECT_RECORD_DTN_SECONDS:
		sdnv_read_u64(&parser->sdnv_state,
			&parser->record->event_timestamp, byte);
		record_parser_wait_for_sdnv(
			parser, RP_EXPECT_RECORD_DTN_NANOSECONDS);
		break;
	case RP_EXPECT_RECORD_DTN_NANOSECONDS:
		sdnv_read_u32(&parser->sdnv_state,
			&parser->record->event_nanoseconds, byte);
		record_parser_wait_for_sdnv(
			parser, RP_EXPECT_BUNDLE_CREATION_TIMESTAMP);
		break;
	case RP_EXPECT_BUNDLE_CREATION_TIMESTAMP:
		sdnv_read_u64(&parser->sdnv_state,
			&parser->record->bundle_creation_timestamp,
			byte);
		record_parser_wait_for_sdnv(
			parser, RP_EXPECT_BUNDLE_CREATION_SEQUENCE);
		break;
	case RP_EXPECT_BUNDLE_CREATION_SEQUENCE:
		sdnv_read_u16(&parser->sdnv_state,
			&parser->record->bundle_sequence_number,
			byte);
		record_parser_wait_for_sdnv(
			parser, RP_EXPECT_BUNDLE_SOURCE_LENGTH);
		break;
	case RP_EXPECT_BUNDLE_SOURCE_LENGTH:
		sdnv_read_u16(&parser->sdnv_state,
			&parser->record->bundle_source_eid_length,
			byte);
		if (record_parser_wait_for_sdnv(
			parser, RP_EXPECT_BUNDLE_SOURCE_EID)
		) {
			parser->record->bundle_source_eid =
				malloc(parser->record
					->bundle_source_eid_length + 1);
			if (!parser->record->bundle_source_eid)
				parser->status = PARSER_STATUS_ERROR;
			parser->current_index = 0;
			parser->bytes_remaining = parser->record
					->bundle_source_eid_length;
		}
		break;
	case RP_EXPECT_BUNDLE_SOURCE_EID:
		parser->bytes_remaining--;
		parser->record->bundle_source_eid
			[parser->current_index++] = (char)byte;
		if (parser->bytes_remaining == 0) {
			parser->record->bundle_source_eid
			[parser->current_index] = '\0';
			parser->status = PARSER_STATUS_DONE;
		}
		fallthrough;
	default:
		parser->status = PARSER_STATUS_ERROR;
		break;
	}
}

struct bundle_administrative_record *bundle6_parse_administrative_record(
	const struct bundle * const record_bundle)
{
	struct record_parser parser;
	uint32_t i = 0;
	uint8_t *cur_byte;

	ASSERT(record_bundle != NULL);
	ASSERT(record_bundle->payload_block != NULL);
	if (record_parser_init(&parser) != UPCN_OK)
		return NULL;
	cur_byte = record_bundle->payload_block->data;
	while (parser.status == PARSER_STATUS_GOOD
		&& i < record_bundle->payload_block->length
	) {
		record_parser_read_byte(&parser, *cur_byte);
		i++;
		cur_byte++;
	}
	if (parser.status == PARSER_STATUS_DONE)
		return parser.record;
	free_administrative_record(parser.record);
	return NULL;
}
