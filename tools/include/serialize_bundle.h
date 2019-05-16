#ifndef SERIALIZE_BUNDLE_H_INCLUDED
#define SERIALIZE_BUNDLE_H_INCLUDED

#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

#include "serialize_helper.h"

#include "upcn/upcn.h"
#include "upcn/bundle.h"

static uint8_t *_bdl_tempbuf;
static size_t _bdl_written;
static void _bdl_write_to_tempbuf(const void *config,
				  const void *bytes,
				  const size_t len)
{
	memcpy(_bdl_tempbuf, bytes, len);
	_bdl_tempbuf += len;
	_bdl_written += len;
}

static size_t _bdl_eid_ssp_index(const char *eid)
{
	size_t index = 0;

	while (*eid != '\0' && *eid != ':') {
		eid++;
		index++;
	}
	return index;
}

static uint64_t _bdl_dtn_timestamp(void)
{
	return DTN_TIMESTAMP_OFFSET + (uint64_t)time(NULL);
}

// TODO: Support BPv7-bis bundles. For now, only RFC 5050 bundles are tested
static struct serialized_data serialize_new_bundle(
	size_t payload_size, const char *src, const char *dest,
	int fragmentable, int high_prio, int lifetime)
{
	static uint16_t seqnum;
	struct bundle *b = bundle_init();
	size_t src_len = strlen(src);
	size_t src_ssp_index = _bdl_eid_ssp_index(src);
	char *null = "dtn:none";
	size_t null_len = strlen(null);
	size_t null_ssp_index = _bdl_eid_ssp_index(null);
	size_t dst_len = strlen(dest);
	size_t dst_ssp_index = _bdl_eid_ssp_index(dest);
	size_t dict_len = null_len + src_len + dst_len + 3;
	char *dictionary = malloc(dict_len);
	uint8_t *payload = malloc(payload_size);
	size_t length;
	uint8_t *bin;

	/* Dictionary */
	memcpy(&dictionary[0], null, null_len);
	dictionary[null_ssp_index] = '\0';
	dictionary[null_len] = '\0';
	memcpy(&dictionary[null_len + 1], src, src_len);
	dictionary[null_len + 1 + src_ssp_index] = '\0';
	dictionary[null_len + 1 + src_len] = '\0';
	memcpy(&dictionary[null_len + 1 + src_len + 1], dest, dst_len);
	dictionary[null_len + 1 + src_len + 1 + dst_ssp_index] = '\0';
	dictionary[null_len + 1 + src_len + 1 + dst_len] = '\0';
	b->dict = dictionary;
	b->dict_length = dict_len;
	/* Offsets */
	b->source_eid.scheme_offset = null_len + 1;
	b->source_eid.ssp_offset = null_len + 1 + src_ssp_index + 1;
	b->destination_eid.scheme_offset = null_len + 1 + src_len + 1;
	b->destination_eid.ssp_offset
		= null_len + 1 + src_len + 1 + dst_ssp_index + 1;
	b->report_eid.scheme_offset = 0;
	b->report_eid.ssp_offset = null_ssp_index + 1;
	b->custodian_eid.scheme_offset = 0;
	b->custodian_eid.ssp_offset = null_ssp_index + 1;
	/* Flags, prio, ... */
	b->proc_flags = BUNDLE_V6_FLAG_SINGLETON_ENDPOINT;
	b->proc_flags |= (high_prio
		? BUNDLE_FLAG_NORMAL_PRIORITY
		: BUNDLE_FLAG_EXPEDITED_PRIORITY);
	if (!fragmentable)
		b->proc_flags |= BUNDLE_FLAG_MUST_NOT_BE_FRAGMENTED;
	/* Times */
	b->creation_timestamp = _bdl_dtn_timestamp();
	b->sequence_number = seqnum++;
	b->lifetime = lifetime;
	/* Payload */
	for (size_t p = 0; p < payload_size; p++)
		payload[p] = (uint8_t)(rand() % 256);
	struct bundle_block *pb
		= bundle_block_create(BUNDLE_BLOCK_TYPE_PAYLOAD);
	pb->flags |= BUNDLE_BLOCK_FLAG_LAST_BLOCK;
	pb->data = payload;
	pb->length = (uint32_t)payload_size;
	b->blocks = bundle_block_entry_create(pb);
	b->payload_block = pb;
	/* Serialize */
	bundle_recalculate_header_length(b);
	length = bundle_get_serialized_size(b);
	bin = malloc(length);
	_bdl_tempbuf = bin;
	_bdl_written = 0;
	bundle_serialize(b, &_bdl_write_to_tempbuf, NULL);
	bundle_free(b);
	_bdl_tempbuf = NULL;
	ASSERT(_bdl_written == length);
	return (struct serialized_data){ bin, (size_t)length };
}

#endif /* SERIALIZE_BUNDLE_H_INCLUDED */
