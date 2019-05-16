#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include "upcn/upcn.h"
#include "upcn/bundle.h"
#include "upcn/sdnv.h"
#include "bundle6/serializer.h"

#define write_bytes(bytes, data) \
	write(cla_obj, data, bytes)
#define serialize_u16(buffer, value) \
	write_bytes(sdnv_write_u16(buffer, value), buffer)
#define serialize_u32(buffer, value) \
	write_bytes(sdnv_write_u32(buffer, value), buffer)
#define serialize_u64(buffer, value) \
	write_bytes(sdnv_write_u64(buffer, value), buffer)

enum upcn_result bundle6_serialize(
	struct bundle *bundle,
	void (*write)(const void *cla_obj, const void *, const size_t),
	const void *cla_obj)
{
	uint8_t buffer[MAX_SDNV_SIZE];
	struct bundle_block_list *cur_entry;
	struct eid_reference *cur_ref;
	uint16_t count;

	/* Write version field */
	write_bytes(1, &(bundle->protocol_version));
	/* Write SDNV values up to dictionary length */
	serialize_u32(buffer, bundle->proc_flags & (
		/* Only RFC 5050 flags */
		BUNDLE_V6_FLAG_IS_FRAGMENT
		| BUNDLE_V6_FLAG_ADMINISTRATIVE_RECORD
		| BUNDLE_V6_FLAG_MUST_NOT_BE_FRAGMENTED
		| BUNDLE_V6_FLAG_CUSTODY_TRANSFER_REQUESTED
		| BUNDLE_V6_FLAG_SINGLETON_ENDPOINT
		| BUNDLE_V6_FLAG_ACKNOWLEDGEMENT_REQUESTED
		| BUNDLE_V6_FLAG_NORMAL_PRIORITY
		| BUNDLE_V6_FLAG_EXPEDITED_PRIORITY
		| BUNDLE_V6_FLAG_REPORT_RECEPTION
		| BUNDLE_V6_FLAG_REPORT_CUSTODY_ACCEPTANCE
		| BUNDLE_V6_FLAG_REPORT_FORWARDING
		| BUNDLE_V6_FLAG_REPORT_DELIVERY
		| BUNDLE_V6_FLAG_REPORT_DELETION
	));
	serialize_u32(buffer, bundle->primary_block_length);
	serialize_u16(buffer, bundle->destination_eid.scheme_offset);
	serialize_u16(buffer, bundle->destination_eid.ssp_offset);
	serialize_u16(buffer, bundle->source_eid.scheme_offset);
	serialize_u16(buffer, bundle->source_eid.ssp_offset);
	serialize_u16(buffer, bundle->report_eid.scheme_offset);
	serialize_u16(buffer, bundle->report_eid.ssp_offset);
	serialize_u16(buffer, bundle->custodian_eid.scheme_offset);
	serialize_u16(buffer, bundle->custodian_eid.ssp_offset);
	serialize_u64(buffer, bundle->creation_timestamp);
	serialize_u16(buffer, bundle->sequence_number);
	serialize_u64(buffer, bundle->lifetime);
	serialize_u16(buffer, bundle->dict_length);
	/* Write dictionary byte array */
	write_bytes(bundle->dict_length, (uint8_t *)bundle->dict);
	/* Write remaining SDNV values */
	if (HAS_FLAG(bundle->proc_flags, BUNDLE_V6_FLAG_IS_FRAGMENT)) {
		serialize_u32(buffer, bundle->fragment_offset);
		serialize_u32(buffer, bundle->total_adu_length);
	}
	/* Serialize bundle blocks */
	cur_entry = bundle->blocks;
	while (cur_entry != NULL) {
		write_bytes(1, &cur_entry->data->type);
		serialize_u32(buffer, cur_entry->data->flags);
		if (HAS_FLAG(cur_entry->data->flags,
			BUNDLE_BLOCK_FLAG_HAS_EID_REF_FIELD)
		) {
			count = cur_entry->data->eid_ref_count;
			serialize_u16(buffer, count);
			cur_ref = cur_entry->data->eid_refs;
			for (; count > 0; count--, cur_ref++) {
				serialize_u16(buffer, cur_ref->scheme_offset);
				serialize_u16(buffer, cur_ref->ssp_offset);
			}
		}
		serialize_u32(buffer, cur_entry->data->length);
		write_bytes(cur_entry->data->length, cur_entry->data->data);
		cur_entry = cur_entry->next;
	}

	/* TODO: Error checking */
	return UPCN_OK;
}
