#include "compilersupport_p.h"  // Private TinyCBOR header, used for endianess
#include "upcn/eidManager.h"
#include "bundle7/allocators.h"
#include "bundle7/parser.h"
#include "bundle7/timestamp.h"


/**
 * Shortcut access to current bundle block
 */
#define BLOCK(state) ((*(state->current_block_entry))->data)


/**
 * Shortcut to transition into error state
 */
#define FAIL(state) (state->basedata->status = PARSER_STATUS_ERROR)


// -----------------------------
// Cyclic Redundancy Check (CRC)
// -----------------------------

static inline void crc_feed_stream(struct bundle7_crc_stream *stream,
	const uint8_t *buffer, size_t length)
{
	for (size_t i = 0; i < length; ++i)
		bundle7_crc_read(stream, buffer[i]);
}


static void crc_feed(struct bundle7_parser *state,
	const uint8_t *buffer, size_t length)
{
	for (size_t i = 0; i < length; ++i) {
		bundle7_crc_read(&state->crc16, buffer[i]);
		bundle7_crc_read(&state->crc32, buffer[i]);
	}
}


static void crc_verify(struct bundle7_parser *state, uint32_t checksum,
	uint32_t expected)
{
	if (checksum != expected)
		state->basedata->flags |= PARSER_FLAG_CRC_INVALID;
}


// --------------------
// Bundle start and end
// --------------------

static CborError bundle_start(struct bundle7_parser *, CborValue *);
static CborError bundle_end(struct bundle7_parser *, CborValue *);


// -------------
// Primary block
// -------------

static CborError primary_block_start(struct bundle7_parser *, CborValue *);
static CborError protocol_version(struct bundle7_parser *, CborValue *);
static CborError primary_proc_flags(struct bundle7_parser *, CborValue *);
static CborError crc_type(struct bundle7_parser *, CborValue *);
static CborError destination_eid(struct bundle7_parser *, CborValue *);
static CborError source_eid(struct bundle7_parser *, CborValue *);
static CborError report_to_eid(struct bundle7_parser *, CborValue *);
static CborError creation_timestamp(struct bundle7_parser *, CborValue *);
static CborError lifetime(struct bundle7_parser *, CborValue *);
static CborError fragment_offset(struct bundle7_parser *, CborValue *);
static CborError primary_block_crc(struct bundle7_parser *, CborValue *);


// ----------------
// Extension Blocks
// ----------------

static CborError block_start(struct bundle7_parser *, CborValue *);
static CborError block_type(struct bundle7_parser *, CborValue *);
static CborError block_number(struct bundle7_parser *, CborValue *);
static CborError block_proc_flags(struct bundle7_parser *, CborValue *);
static CborError block_crc_type(struct bundle7_parser *, CborValue *);
static CborError block_crc(struct bundle7_parser *, CborValue *);
static CborError block_data_length(struct bundle7_parser *, CborValue *);
static CborError block_end(struct bundle7_parser *, CborValue *);


// --------------------
// Bundle start and end
// --------------------

CborError bundle_start(struct bundle7_parser *state, CborValue *it)
{
	if (!cbor_value_is_array(it) || cbor_value_is_length_known(it))
		return CborErrorIllegalType;

	state->next = primary_block_start;
	return cbor_value_enter_container(it, it);
}


CborError bundle_end(struct bundle7_parser *state, CborValue *it)
{
	struct bundle *bundle;

	if (*it->ptr != 0xff)
		return CborErrorIllegalType;
	it->ptr++;

	// Transition into "Done" state
	state->basedata->status = PARSER_STATUS_DONE;

	// Clear bundle reference
	bundle = state->bundle;
	state->bundle = NULL;

	// Call "send" callback if set and all CRCs passed, otherwise discard
	// parsed bundle silently
	if (state->send_callback == NULL
		|| state->basedata->flags & PARSER_FLAG_CRC_INVALID)
		bundle_free(bundle);
	else
		state->send_callback(bundle);

	return CborNoError;
}


// -------------
// Primary Block
// -------------

CborError primary_block_start(struct bundle7_parser *state, CborValue *it)
{
	if (!cbor_value_is_array(it) || !cbor_value_is_length_known(it))
		return CborErrorIllegalType;

	// Primary block CRC
	state->flags |= BUNDLE_V7_PARSER_CRC_FEED;

	bundle7_crc_init(&state->crc16, BUNDLE_V7_CRC16);
	bundle7_crc_init(&state->crc32, BUNDLE_V7_CRC32);

	state->next = protocol_version;
	return cbor_value_enter_container(it, it);
}


CborError protocol_version(struct bundle7_parser *state, CborValue *it)
{
	uint64_t version;

	if (!cbor_value_is_unsigned_integer(it))
		return CborErrorIllegalType;

	cbor_value_get_uint64(it, &version);
	state->bundle->protocol_version = version;

	state->next = primary_proc_flags;
	return cbor_value_advance_fixed(it);
}


CborError primary_proc_flags(struct bundle7_parser *state, CborValue *it)
{
	uint64_t flags;

	if (!cbor_value_is_unsigned_integer(it))
		return CborErrorIllegalType;

	cbor_value_get_uint64(it, &flags);

	// Convert BPv7-specific bundle processing flags into the generic
	// upcn-representation.
	state->bundle->proc_flags = ((uint16_t) flags & (
			BUNDLE_V7_FLAG_IS_FRAGMENT
			| BUNDLE_V7_FLAG_ADMINISTRATIVE_RECORD
			| BUNDLE_V7_FLAG_MUST_NOT_BE_FRAGMENTED
			| BUNDLE_V7_FLAG_ACKNOWLEDGEMENT_REQUESTED))

		// Bit 6 -> 21
		| ((flags & BUNDLE_V7_FLAG_REPORT_STATUS_TIME) << 15)

		// Bit 7 -> 22
		| ((flags & BUNDLE_V7_FLAG_CONTAINS_MANIFEST) << 15)

		// Bit 8 -> 14
		| ((flags & BUNDLE_V7_FLAG_REPORT_RECEPTION) << 6)

		// Bit 10 -> 16
		| ((flags & BUNDLE_V7_FLAG_REPORT_FORWARDING) << 6)

		// Bit 11 -> 17
		| ((flags & BUNDLE_V7_FLAG_REPORT_DELIVERY) << 6)

		// Bit 12 -> 18
		| ((flags & BUNDLE_V7_FLAG_REPORT_DELETION) << 6);

	state->next = crc_type;
	return cbor_value_advance_fixed(it);
}


CborError crc_type(struct bundle7_parser *state, CborValue *it)
{
	uint64_t type;

	if (!cbor_value_is_unsigned_integer(it))
		return CborErrorIllegalType;

	cbor_value_get_uint64(it, &type);

	// Validate CRC type
	if (type > BUNDLE_CRC_TYPE_32)
		return CborErrorIllegalType;

	state->bundle->crc_type = type;

	// Deactivate CRC feeding if CRC is not enabled
	if (state->bundle->crc_type == BUNDLE_CRC_TYPE_NONE)
		state->flags &= ~BUNDLE_V7_PARSER_CRC_FEED;

	state->next = destination_eid;
	return cbor_value_advance_fixed(it);
}


CborError parse_eid(struct bundle7_parser *state, CborValue *it, char **eid,
	CborError (*next)(struct bundle7_parser *, CborValue *))
{
	CborError err = bundle7_eid_parse_cbor(it, eid);

	if (err)
		return err;

	// Allocate zero-copy reference
	*eid = eidmanager_alloc_ref(*eid, true);

	state->next = next;
	return CborNoError;
}


CborError destination_eid(struct bundle7_parser *state, CborValue *it)
{
	return parse_eid(state, it, &state->bundle->destination, source_eid);
}


CborError source_eid(struct bundle7_parser *state, CborValue *it)
{
	return parse_eid(state, it, &state->bundle->source, report_to_eid);
}


CborError report_to_eid(struct bundle7_parser *state, CborValue *it)
{
	return parse_eid(state, it, &state->bundle->report_to,
		creation_timestamp);
}


CborError creation_timestamp(struct bundle7_parser *state, CborValue *it)
{
	CborError err;

	err = bundle7_timestamp_parse(it,
		&state->bundle->creation_timestamp,
		&state->bundle->sequence_number);
	if (err)
		return err;

	state->next = lifetime;
	return CborNoError;
}


CborError lifetime(struct bundle7_parser *state, CborValue *it)
{
	uint64_t value;

	if (!cbor_value_is_unsigned_integer(it))
		return CborErrorIllegalType;

	cbor_value_get_uint64(it, &value);
	state->bundle->lifetime = value;

	const uint8_t *last_ptr = it->ptr;
	CborError err = cbor_value_advance_fixed(it);

	if (bundle_is_fragmented(state->bundle))
		state->next = fragment_offset;
	else if (state->bundle->crc_type != BUNDLE_CRC_TYPE_NONE)
		state->next = primary_block_crc;
	else {
		state->next = block_start;

		// -1 Byte infinite array header
		state->bundle->primary_block_length = state->bundle_size - 1 +
			(it->ptr - last_ptr);
	}

	return err;
}


CborError fragment_offset(struct bundle7_parser *state, CborValue *it)
{
	uint64_t offset;

	if (!cbor_value_is_unsigned_integer(it))
		return CborErrorIllegalType;

	cbor_value_get_uint64(it, &offset);
	state->bundle->fragment_offset = offset;

	const uint8_t *last_ptr = it->ptr;
	CborError err = cbor_value_advance_fixed(it);

	if (state->bundle->crc_type != BUNDLE_CRC_TYPE_NONE)
		state->next = primary_block_crc;
	else {
		state->next = block_start;

		// -1 Byte infinite array header
		state->bundle->primary_block_length = state->bundle_size - 1 +
			(it->ptr - last_ptr);
	}

	return err;
}


CborError primary_block_crc(struct bundle7_parser *state, CborValue *it)
{
	union crc crc;
	size_t len = sizeof(crc);
	CborError err;

	if (!cbor_value_is_byte_string(it))
		return CborErrorIllegalType;

	err = cbor_value_copy_byte_string(it, crc.bytes, &len, it);
	if (err)
		return err;

	// Deactivate CRC feeding for the CRC field itself
	state->flags &= ~BUNDLE_V7_PARSER_CRC_FEED;

	if (state->bundle->crc_type == BUNDLE_CRC_TYPE_16) {
		// Ensure correct CRC 16 length
		if (len != 2)
			return CborErrorIllegalType;

		// CRC field is populated with zero
		bundle7_crc_read(&state->crc16, 0x42);  // CBOR 2-b array header
		bundle7_crc_read(&state->crc16, 0x00);
		bundle7_crc_read(&state->crc16, 0x00);
		bundle7_crc_done(&state->crc16);

		// Swap from network byte order to native order and clear all
		// higher bits
		state->bundle->crc.checksum = cbor_ntohs(crc.checksum) & 0xffff;

		crc_verify(state,
			state->crc16.checksum,
			state->bundle->crc.checksum);

		// -1 Byte infinite array header + 3 Bytes CRC field
		state->bundle->primary_block_length
				= state->bundle_size - 1 + 3;
	} else {
		// Ensure correct CRC 32 length
		if (len != 4)
			return CborErrorIllegalType;

		// CRC field is populated with zero
		bundle7_crc_read(&state->crc32, 0x44);  // CBOR 4-b array header
		bundle7_crc_read(&state->crc32, 0x00);
		bundle7_crc_read(&state->crc32, 0x00);
		bundle7_crc_read(&state->crc32, 0x00);
		bundle7_crc_read(&state->crc32, 0x00);
		bundle7_crc_done(&state->crc32);

		// Swap from network byte order to native order
		state->bundle->crc.checksum = cbor_ntohl(crc.checksum);

		crc_verify(state,
			state->crc32.checksum,
			state->bundle->crc.checksum);

		// -1 Byte infinite array header + 5 Bytes CRC field
		state->bundle->primary_block_length
				= state->bundle_size - 1 + 5;
	}

	state->next = block_start;

	return CborNoError;
}


// ----------------
// Extension Blocks
// ----------------

CborError block_start(struct bundle7_parser *state, CborValue *it)
{
	if (!cbor_value_is_array(it))
		return CborErrorIllegalType;

	// Reset CRC streams
	bundle7_crc_init(&state->crc16, BUNDLE_V7_CRC16);
	bundle7_crc_init(&state->crc32, BUNDLE_V7_CRC32);

	// Enable CRC feeding again
	state->flags |= BUNDLE_V7_PARSER_CRC_FEED;

	state->next = block_type;
	return cbor_value_enter_container(it, it);
}


CborError block_type(struct bundle7_parser *state, CborValue *it)
{
	uint64_t type;

	if (!cbor_value_is_unsigned_integer(it))
		return CborErrorIllegalType;

	cbor_value_get_uint64(it, &type);

	// Create bundle block
	struct bundle_block *block = bundle_block_create(type);

	if (block == NULL)
		return CborErrorOutOfMemory;

	// Create bundle block list entry
	*state->current_block_entry = bundle_block_entry_create(block);

	if (*state->current_block_entry == NULL) {
		bundle_block_free(block);
		return CborErrorOutOfMemory;
	}

	state->next = block_number;
	return cbor_value_advance_fixed(it);
}


CborError block_number(struct bundle7_parser *state, CborValue *it)
{
	uint64_t number;

	if (!cbor_value_is_unsigned_integer(it))
		return CborErrorIllegalType;

	cbor_value_get_uint64(it, &number);
	BLOCK(state)->number = number;

	state->next = block_proc_flags;
	return cbor_value_advance_fixed(it);
}


CborError block_proc_flags(struct bundle7_parser *state, CborValue *it)
{
	uint64_t flags;

	if (!cbor_value_is_unsigned_integer(it))
		return CborErrorIllegalType;

	cbor_value_get_uint64(it, &flags);

	BLOCK(state)->flags = (((uint8_t) flags)
			& BUNDLE_V7_BLOCK_FLAG_MUST_BE_REPLICATED)

		// Bit 1 -> 4
		| ((flags & BUNDLE_V7_BLOCK_FLAG_DISCARD_IF_UNPROC) << 3)

		// Bit 2 -> 1
		| ((flags & BUNDLE_V7_BLOCK_FLAG_REPORT_IF_UNPROC) >> 1)

		// Bit 3 -> 2
		| ((flags & BUNDLE_V7_BLOCK_FLAG_DELETE_BUNDLE_IF_UNPROC)
			>> 1);

	state->next = block_crc_type;
	return cbor_value_advance_fixed(it);
}


CborError block_crc_type(struct bundle7_parser *state, CborValue *it)
{
	uint64_t crc_type;

	if (!cbor_value_is_unsigned_integer(it))
		return CborErrorIllegalType;

	cbor_value_get_uint64(it, &crc_type);
	BLOCK(state)->crc_type = crc_type;

	state->next = block_data_length;

	// Deactivate CRC feeding
	if (crc_type == BUNDLE_CRC_TYPE_NONE)
		state->flags &= ~BUNDLE_V7_PARSER_CRC_FEED;

	return cbor_value_advance_fixed(it);
}


CborError block_crc(struct bundle7_parser *state, CborValue *it)
{
	union crc crc;
	size_t len = sizeof(crc);
	CborError err;

	if (!cbor_value_is_byte_string(it))
		return CborErrorIllegalType;

	// Deactivate CRC feeding for the CRC field itself
	state->flags &= ~BUNDLE_V7_PARSER_CRC_FEED;

	err = cbor_value_copy_byte_string(it, crc.bytes, &len, it);
	if (err)
		return err;

	// Feed CRC with raw CBOR block data
	if (BLOCK(state)->crc_type == BUNDLE_CRC_TYPE_16) {
		// Ensure correct CRC 16 checksum length
		if (len != 2)
			return CborErrorIllegalType;

		crc_feed_stream(&state->crc16,
			BLOCK(state)->data,
			BLOCK(state)->length);

		// CRC field is populated with zero
		bundle7_crc_read(&state->crc16, 0x42); // CBOR byte string(2)
		bundle7_crc_read(&state->crc16, 0x00);
		bundle7_crc_read(&state->crc16, 0x00);
		bundle7_crc_done(&state->crc16);

		// Swap from network byte order to native order and clear all
		// higher bits
		BLOCK(state)->crc.checksum = cbor_ntohs(crc.checksum) & 0xffff;

		crc_verify(state,
			state->crc16.checksum,
			BLOCK(state)->crc.checksum);
	} else {
		// Ensure correct CRC 32 checksum length
		if (len != 4)
			return CborErrorIllegalType;

		crc_feed_stream(&state->crc32,
			BLOCK(state)->data,
			BLOCK(state)->length);

		// CRC field is populated with zero
		bundle7_crc_read(&state->crc32, 0x44); // CBOR byte string(4)
		bundle7_crc_read(&state->crc32, 0x00);
		bundle7_crc_read(&state->crc32, 0x00);
		bundle7_crc_read(&state->crc32, 0x00);
		bundle7_crc_read(&state->crc32, 0x00);
		bundle7_crc_done(&state->crc32);

		// Swap from network byte order to native order
		BLOCK(state)->crc.checksum = cbor_ntohl(crc.checksum);

		crc_verify(state,
			state->crc32.checksum,
			BLOCK(state)->crc.checksum);
	}
	block_end(state, it);

	return CborNoError;
}


CborError block_data_length(struct bundle7_parser *state, CborValue *it)
{
	uint64_t length;
	CborError err;

	if (!cbor_value_is_unsigned_integer(it))
		return CborErrorIllegalType;

	cbor_value_get_uint64(it, &length);

	err = cbor_value_advance_fixed(it);
	if (err)
		return err;

	// Activate CRC feeding again
	state->flags |= BUNDLE_V7_PARSER_CRC_FEED;

	BLOCK(state)->length = length;

	// Block-specific data
	// -------------------
	//
	BLOCK(state)->data = bundle7_malloc(length);
	if (BLOCK(state)->data == NULL)
		return CborErrorOutOfMemory;

	// Enable "bulk read" mode
	state->basedata->next_buffer = BLOCK(state)->data;
	state->basedata->next_bytes = length;
	state->basedata->flags |= PARSER_FLAG_BULK_READ;
	state->bundle_size += length;

	if (BLOCK(state)->crc_type != BUNDLE_CRC_TYPE_NONE)
		state->next = block_crc;
	else
		state->next = block_end;

	return CborNoError;
}


/**
 * Bulk read operation handler
 *
 * This function gets called with an null-pointer intstead of an iterator after
 * the "bulk read" operation was performed and the bulk buffer was filled.
 *
 * @return This function never fails and always returns CborNoError
 */
CborError block_end(struct bundle7_parser *state, CborValue *it)
{
	// Update parsing callback directly because there is no transition for
	// this callback as it gets called after the bulk read operation with
	// a null pointer instead an CBOR iterator.
	//
	// Set the "next" callback to the same function to prevent undesirable
	// transitions to an old callback of a previous stage
	if (BLOCK(state)->type == BUNDLE_BLOCK_TYPE_PAYLOAD) {
		state->bundle->payload_block = BLOCK(state);
		state->parse = bundle_end;
		state->next = bundle_end;
	} else {
		state->parse = block_start;
		state->next = block_start;
	}

	// Iterate to next block entry element
	state->current_block_entry = &(*state->current_block_entry)->next;

	return CborNoError;
}


// -----------
// Parser Core
// -----------

struct parser *bundle7_parser_init(struct bundle7_parser *state,
	void (*send_callback)(struct bundle *))
{
	state->basedata = bundle7_malloc(sizeof(struct parser));
	if (state->basedata == NULL)
		return NULL;

	state->send_callback = send_callback;
	state->bundle = NULL;
	state->next = NULL;  // force reset to do its job

	// Set to error that the reset handler does not abort
	state->basedata->status = PARSER_STATUS_ERROR;

	if (bundle7_parser_reset(state) != UPCN_OK) {
		bundle7_free(state->basedata);
		return NULL;
	}

	return state->basedata;
}


enum upcn_result bundle7_parser_reset(struct bundle7_parser *state)
{
	if (state->basedata->status == PARSER_STATUS_GOOD
		&& state->next == bundle_start)
		return UPCN_OK;

	state->basedata->status = PARSER_STATUS_GOOD;
	state->basedata->flags = PARSER_FLAG_NONE;

	state->parse = bundle_start;
	state->flags = 0;
	state->bundle_size = 0;

	if (state->bundle != NULL)
		bundle_reset(state->bundle);
	else
		state->bundle = bundle_init();

	if (state->bundle == NULL)
		return UPCN_FAIL;

	state->current_block_entry = &state->bundle->blocks;

	return UPCN_OK;
}


size_t bundle7_parser_read(struct bundle7_parser *state,
	const uint8_t *buffer, size_t length)
{
	CborParser parser;
	CborValue it;
	CborError err;
	size_t parsed = 0;

	// Special case:
	//     Bulk read operation was performed and this function gets called
	//     with an empty buffer to continue processing
	if (buffer == NULL) {
		// "block_end" never fails - therfore no error handling
		if (state->next != block_crc)
			state->parse(state, NULL);
		return 0;
	}

	err = cbor_parser_init(buffer, length, 0, &parser, &it);

	// CBOR decoding error
	if (err) {
		// Insufficient data
		if (err == CborErrorUnexpectedEOF)
			return parsed;

		// We allow "unexpected" breaks
		//
		// The bundle is an indefinite array and its end is marked by
		// the "break" symbol. Therefore the buffer could contain a
		// single "break". This happens because we cut the the
		// indefinite array by the chunked reading operation.
		if (err != CborErrorUnexpectedBreak) {
			FAIL(state);
			return parsed;
		}
	}

	while (parsed < length
			&& state->basedata->status == PARSER_STATUS_GOOD) {

		// Bulk read operation requested
		if (state->basedata->flags & PARSER_FLAG_BULK_READ) {
			if (parsed + state->basedata->next_bytes > length)
				break;

			memcpy(state->basedata->next_buffer,
				buffer + parsed,
				state->basedata->next_bytes);

			parsed += state->basedata->next_bytes;

			// Disables bulk read mode again
			state->basedata->flags &= ~PARSER_FLAG_BULK_READ;

			// Process copied data -- this call to "block_end"is
			// always successful
			if (state->next != block_crc)
				state->next(state, NULL);

			// Reinitialize CBOR parser after the skipped bytes
			err = cbor_parser_init(
					buffer + parsed,
					length - parsed,
					0, &parser, &it);

			// For detailed explanation, see init error handling
			// above
			if (err) {
				if (err == CborErrorUnexpectedEOF)
					break;
				if (err != CborErrorUnexpectedBreak) {
					FAIL(state);
					return parsed;
				}
			}
			// Skip parsing and transition
			continue;
		}

		// Parsing step
		err = state->parse(state, &it);

		// Parsing error
		if (err) {
			if (err != CborErrorUnexpectedEOF)
				FAIL(state);
			return parsed;
		}

		// Transition to next stage
		size_t new_parsed = (cbor_value_get_next_byte(&it) -
				     buffer);

		state->bundle_size += new_parsed - parsed;

		// Bundle is larger than allowed
		if (state->bundle_size > BUNDLE_QUOTA) {
			FAIL(state);
			return parsed;
		}

		if (state->flags & BUNDLE_V7_PARSER_CRC_FEED)
			crc_feed(state, buffer + parsed, new_parsed - parsed);

		state->parse = state->next;
		parsed = new_parsed;

		if (cbor_value_at_end(&it)) {
			err = cbor_parser_init(
					buffer + parsed,
					length - parsed,
					0, &parser, &it);

			// "break" symbols are allowed to parse the CRC
			// after the block data field
			if (err && err != CborErrorUnexpectedBreak) {
				if (err != CborErrorUnexpectedEOF)
					FAIL(state);
				return parsed;
			}
		}
	}

	return parsed;
}
