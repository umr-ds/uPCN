#include <stddef.h>
#include <string.h>
#include "upcn/upcn.h"
#include "upcn/bundle.h"
#include "upcn/sdnv.h"
#include "upcn/eidManager.h"
#include "bundle6/bundle6.h"


uint8_t *bundle6_get_payload_data(const struct bundle *bundle, size_t *length)
{
	if (length)
		*length = bundle->payload_block->length;
	return bundle->payload_block->data;
}


// ----------------------
// Bundle and block sizes
// ----------------------

void bundle6_recalculate_header_length(struct bundle *bundle)
{
	bundle->primary_block_length
		= sdnv_get_size_u32(bundle->destination_eid.scheme_offset)
		+ sdnv_get_size_u32(bundle->destination_eid.ssp_offset)
		+ sdnv_get_size_u32(bundle->source_eid.scheme_offset)
		+ sdnv_get_size_u32(bundle->source_eid.ssp_offset)
		+ sdnv_get_size_u32(bundle->report_eid.scheme_offset)
		+ sdnv_get_size_u32(bundle->report_eid.ssp_offset)
		+ sdnv_get_size_u32(bundle->custodian_eid.scheme_offset)
		+ sdnv_get_size_u32(bundle->custodian_eid.ssp_offset)
		+ sdnv_get_size_u32(bundle->creation_timestamp)
		+ sdnv_get_size_u32(bundle->sequence_number)
		+ sdnv_get_size_u32(bundle->lifetime)
		+ sdnv_get_size_u32(bundle->dict_length)
		+ bundle->dict_length
		+ (HAS_FLAG(bundle->proc_flags, BUNDLE_V6_FLAG_IS_FRAGMENT)
			? (sdnv_get_size_u32(bundle->fragment_offset)
				+ sdnv_get_size_u32(bundle->total_adu_length))
			: 0);
	bundle->primary_prefix_length = 1
		+ sdnv_get_size_u32(bundle->proc_flags)
		+ sdnv_get_size_u32(bundle->primary_block_length);
}


size_t bundle6_block_get_size(struct bundle_block *block)
{
	int i;
	uint16_t eid_size = 0;

	if (HAS_FLAG(block->flags, BUNDLE_BLOCK_FLAG_HAS_EID_REF_FIELD)) {
		eid_size += sdnv_get_size_u16(block->eid_ref_count);
		for (i = 0; i < block->eid_ref_count; i++) {
			eid_size += sdnv_get_size_u16(
				block->eid_refs[i].scheme_offset);
			eid_size += sdnv_get_size_u16(
				block->eid_refs[i].ssp_offset);
		}
	}
	return 1
		+ sdnv_get_size_u32(block->flags)
		+ sdnv_get_size_u32(block->length)
		+ block->length
		+ eid_size;
}


size_t bundle6_get_serialized_size(struct bundle *bundle)
{
	size_t s = 0;
	struct bundle_block_list *cur = bundle->blocks;

	while (cur != NULL) {
		s += bundle6_block_get_size(cur->data);
		cur = cur->next;
	}
	return bundle->primary_prefix_length + bundle->primary_block_length + s;
}


size_t bundle6_get_serialized_size_without_payload(struct bundle *bundle)
{
	size_t s = 0;
	struct bundle_block_list *cur = bundle->blocks;

	while (cur != NULL) {
		if (cur->data->type != BUNDLE_BLOCK_TYPE_PAYLOAD)
			s += bundle6_block_get_size(cur->data);
		cur = cur->next;
	}
	return bundle->primary_prefix_length + bundle->primary_block_length + s;
}


// -------------------
// Fragmentation sizes
// -------------------

size_t bundle6_get_first_fragment_min_size(struct bundle *bundle)
{
	size_t s = 0;
	int_fast8_t payload_reached = 0;
	struct bundle_block_list *cur = bundle->blocks;

	while (cur != NULL) {
		if (cur->data->type == BUNDLE_BLOCK_TYPE_PAYLOAD) {
			payload_reached = 1;
			/* Payload header size */
			s += 1  + sdnv_get_size_u32(cur->data->flags)
				+ sdnv_get_size_u32(cur->data->length);
		} else if (!payload_reached
			|| HAS_FLAG(cur->data->flags,
				BUNDLE_BLOCK_FLAG_MUST_BE_REPLICATED)
		) {
			s += bundle6_block_get_size(cur->data);
		}
		cur = cur->next;
	}
	/* Add sizes of total ADU len (= PL) + 1 byte for offset 0 */
	if (!HAS_FLAG(bundle->proc_flags, BUNDLE_V6_FLAG_IS_FRAGMENT))
		s += sdnv_get_size_u32(bundle->payload_block->length) + 1;
	return bundle->primary_prefix_length + bundle->primary_block_length + s;
}


size_t bundle6_get_mid_fragment_min_size(struct bundle *bundle)
{
	size_t s = 0;
	struct bundle_block_list *cur = bundle->blocks;

	while (cur != NULL) {
		if (cur->data->type == BUNDLE_BLOCK_TYPE_PAYLOAD) {
			/* Payload header size */
			s += 1  + sdnv_get_size_u32(cur->data->flags)
				+ sdnv_get_size_u32(cur->data->length);
		} else if (HAS_FLAG(cur->data->flags,
			BUNDLE_BLOCK_FLAG_MUST_BE_REPLICATED)
		) {
			s += bundle6_block_get_size(cur->data);
		}
		cur = cur->next;
	}
	/* Add sizes of total ADU len/offset, assume offset SDNV has PL len */
	if (!HAS_FLAG(bundle->proc_flags, BUNDLE_V6_FLAG_IS_FRAGMENT))
		s += sdnv_get_size_u32(bundle->payload_block->length) * 2;
	return bundle->primary_prefix_length + bundle->primary_block_length + s;
}


size_t bundle6_get_last_fragment_min_size(struct bundle *bundle)
{
	size_t s = 0;
	int_fast8_t payload_reached = 0;
	struct bundle_block_list *cur = bundle->blocks;

	while (cur != NULL) {
		if (cur->data->type == BUNDLE_BLOCK_TYPE_PAYLOAD) {
			payload_reached = 1;
			/* Payload header size */
			s += 1  + sdnv_get_size_u32(cur->data->flags)
				+ sdnv_get_size_u32(cur->data->length);
		} else if (
			payload_reached
			|| HAS_FLAG(cur->data->flags,
				BUNDLE_BLOCK_FLAG_MUST_BE_REPLICATED)
		) {
			s += bundle6_block_get_size(cur->data);
		}
		cur = cur->next;
	}
	/* Add sizes of total ADU len/offset, assume offset SDNV has PL len */
	if (!HAS_FLAG(bundle->proc_flags, BUNDLE_V6_FLAG_IS_FRAGMENT))
		s += sdnv_get_size_u32(bundle->payload_block->length) * 2;
	return bundle->primary_prefix_length + bundle->primary_block_length + s;
}


enum upcn_result bundle6_set_current_custodian(struct bundle *bundle,
	const char *schema, const char *ssp)
{
	size_t old_length = bundle->dict_length;
	size_t schema_length = strlen(schema);
	size_t ssp_length = strlen(ssp);

	// Allocate enough memory for the custodian EID in dictionary
	bundle->dict_length += schema_length + ssp_length;
	bundle->dict = realloc(bundle->dict, bundle->dict_length);
	if (bundle->dict == NULL)
		return UPCN_FAIL;

	// Copy schema
	bundle->custodian_eid.scheme_offset = old_length;
	memcpy(bundle->dict + old_length, schema, schema_length);

	// Copy SSP
	old_length += schema_length;
	bundle->custodian_eid.ssp_offset = old_length;
	memcpy(bundle->dict + old_length, ssp, ssp_length);

	// Dictionary changed, recalculate header size
	bundle_recalculate_header_length(bundle);

	return UPCN_OK;
}
