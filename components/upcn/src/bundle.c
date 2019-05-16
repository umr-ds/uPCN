#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "drv/mini-printf.h"

#include "upcn/upcn.h"
#include "upcn/bundle.h"
#include "upcn/sdnv.h"
#include "upcn/eidManager.h"

// RFC 5050
#include <bundle6/bundle6.h>
#include <bundle6/serializer.h>
#include <bundle6/eid.h>

// BPv7-bis
#include <bundle7/bundle7.h>
#include <bundle7/eid.h>
#include <bundle7/serializer.h>

static inline void bundle_reset_internal(struct bundle *bundle)
{
	if (bundle == NULL)
		return;
	bundle->id = BUNDLE_INVALID_ID;
	bundle->protocol_version = 0x06;
	bundle->proc_flags = BUNDLE_FLAG_NONE;
	bundle->ret_constraints = BUNDLE_RET_CONSTRAINT_NONE;

	// EIDs
	bundle->destination = NULL;
	bundle->source = NULL;
	bundle->report_to = NULL;
	bundle->destination_eid.scheme_offset = 0;
	bundle->destination_eid.ssp_offset = 0;
	bundle->source_eid.scheme_offset = 0;
	bundle->source_eid.ssp_offset = 0;
	bundle->report_eid.scheme_offset = 0;
	bundle->report_eid.ssp_offset = 0;
	bundle->custodian_eid.scheme_offset = 0;
	bundle->custodian_eid.ssp_offset = 0;

	bundle->crc_type = BUNDLE_CRC_TYPE_NONE;
	bundle->creation_timestamp = 0;
	bundle->sequence_number = 0;
	bundle->lifetime = 0;
	bundle->dict = NULL;
	bundle->dict_length = 0;
	bundle->fragment_offset = 0;
	bundle->total_adu_length = 0;
	bundle->primary_prefix_length = 0;
	bundle->primary_block_length = 0;
	bundle->blocks = NULL;
	bundle->payload_block = NULL;
}

struct bundle *bundle_init()
{
	struct bundle *bundle;

	bundle = malloc(sizeof(struct bundle));
	bundle_reset_internal(bundle);
	return bundle;
}

inline void bundle_free_dynamic_parts(struct bundle *bundle)
{
	ASSERT(bundle != NULL);
	if (bundle->dict != NULL) {
		free(bundle->dict);
		bundle->dict = NULL;
	}

	// EIDs
	if (bundle->destination != NULL) {
		eidmanager_free_ref(bundle->destination);
		bundle->destination = NULL;
	}
	if (bundle->source != NULL) {
		eidmanager_free_ref(bundle->source);
		bundle->source = NULL;
	}
	if (bundle->report_to != NULL) {
		eidmanager_free_ref(bundle->report_to);
		bundle->report_to = NULL;
	}

	while (bundle->blocks != NULL)
		bundle->blocks = bundle_block_entry_free(bundle->blocks);
}

void bundle_reset(struct bundle *bundle)
{
	bundle_free_dynamic_parts(bundle);
	bundle_reset_internal(bundle);
}

void bundle_free(struct bundle *bundle)
{
	if (bundle == NULL)
		return;
	bundle_free_dynamic_parts(bundle);
	free(bundle);
}

void bundle_drop(struct bundle *bundle)
{
	ASSERT(bundle->ret_constraints == BUNDLE_RET_CONSTRAINT_NONE);
	bundle_free(bundle);
}

void bundle_copy_headers(struct bundle *to, const struct bundle *from)
{
	memcpy(to, from, sizeof(struct bundle));

	// Copy EID dictionary
	if (from->dict != NULL) {
		to->dict = malloc(from->dict_length + 1);
		if (to->dict != NULL)
			memcpy(to->dict, from->dict, from->dict_length + 1);
	}

	// Increase EID referece counters
	if (to->destination != NULL)
		to->destination = eidmanager_alloc_ref(to->destination, 0);
	if (to->source != NULL)
		to->source = eidmanager_alloc_ref(to->source, 0);
	if (to->report_to != NULL)
		to->report_to = eidmanager_alloc_ref(to->report_to, 0);

	// No extension blocks are copied
	to->blocks = NULL;
	to->payload_block = NULL;
}

void bundle_recalculate_header_length(struct bundle *bundle)
{
	// RFC 5050
	if (bundle->protocol_version == 6)
		bundle6_recalculate_header_length(bundle);
	// BPv7
	else
		bundle7_recalculate_primary_block_length(bundle);
}

size_t bundle_sizeof(struct bundle *bundle)
{
	size_t size = 0;
	struct bundle_block_list *cur = bundle->blocks;

	while (cur != NULL) {
		size += sizeof(struct bundle_block_list)
			+ sizeof(struct bundle_block)
			+ cur->data->length;
		cur = cur->next;
	}
	return size + sizeof(struct bundle) + bundle->dict_length;
}


struct bundle *bundle_dup(const struct bundle *bundle)
{
	struct bundle *dup;
	struct bundle_block_list *cur_block;

	ASSERT(bundle != NULL);
	dup = malloc(sizeof(struct bundle));
	if (dup == NULL)
		return NULL;
	memcpy(dup, bundle, sizeof(struct bundle));

	// Allocate new EID references
	if (dup->source)
		dup->source = eidmanager_alloc_ref(dup->source, false);
	if (dup->destination)
		dup->destination = eidmanager_alloc_ref(dup->destination,
			false);
	if (dup->report_to)
		dup->report_to = eidmanager_alloc_ref(dup->report_to, false);

	// Duplicate extension blocks
	dup->blocks = bundle_block_list_dup(bundle->blocks);
	if (bundle->blocks != NULL && dup->blocks == NULL) {
		bundle_free(dup);
		return NULL;
	}

	// Search for payload block
	cur_block = dup->blocks;
	while (cur_block != NULL) {
		// Found it!
		if (cur_block->data->type == BUNDLE_BLOCK_TYPE_PAYLOAD) {
			dup->payload_block = cur_block->data;
			break;
		}
	}

	return dup;
}

enum bundle_routing_priority bundle_get_routing_priority(
	struct bundle *bundle)
{
	// If there are any retention constaints for the bundle or the bundle
	// has expedited priority (RFC 5050-only)
	if (HAS_FLAG(bundle->ret_constraints,
			BUNDLE_RET_CONSTRAINT_FLAG_OWN)
		|| HAS_FLAG(bundle->ret_constraints,
			BUNDLE_RET_CONSTRAINT_CUSTODY_ACCEPTED)
		|| (bundle->protocol_version == 6
			&& HAS_FLAG(bundle->proc_flags,
				BUNDLE_V6_FLAG_EXPEDITED_PRIORITY)))
		return BUNDLE_RPRIO_HIGH;
	// BPv7-bis bundles without retention constraints are routed with
	// normal priority
	else if (bundle->protocol_version == 7
		|| HAS_FLAG(bundle->proc_flags, BUNDLE_V6_FLAG_NORMAL_PRIORITY))
		return BUNDLE_RPRIO_NORMAL;
	else
		return BUNDLE_RPRIO_LOW;

}

uint8_t *bundle_get_payload_data(const struct bundle *bundle, size_t *length)
{
	// RFC 5050
	if (bundle->protocol_version == 6)
		return bundle6_get_payload_data(bundle, length);
	// BPv7
	else
		return bundle7_get_payload_data(bundle, length);
}

size_t bundle_get_serialized_size(struct bundle *bundle)
{
	// RFC 5050
	if (bundle->protocol_version == 6)
		return bundle6_get_serialized_size(bundle);
	// BPv7-bis
	else
		return bundle7_get_serialized_size(bundle);
}

size_t bundle_get_serialized_size_without_payload(struct bundle *bundle)
{
	// RFC 5050
	if (bundle->protocol_version == 6)
		return bundle6_get_serialized_size_without_payload(bundle);
	// BPv7-bis
	else
		return bundle7_get_serialized_size_without_payload(bundle);
}

struct bundle_list *bundle_list_entry_create(struct bundle *bundle)
{
	if (bundle == NULL)
		return NULL;

	struct bundle_list *entry = malloc(sizeof(struct bundle_list));

	if (entry == NULL)
		return NULL;

	entry->data = bundle;
	entry->next = NULL;

	return entry;
}

struct bundle_list *bundle_list_entry_free(struct bundle_list *entry)
{
	struct bundle_list *next;

	ASSERT(entry != NULL);
	next = entry->next;
	bundle_free(entry->data);
	free(entry);
	return next;
}

struct bundle_block *bundle_block_create(enum bundle_block_type t)
{
	struct bundle_block *block = malloc(sizeof(struct bundle_block));

	if (block == NULL)
		return NULL;
	block->type = t;
	block->flags = BUNDLE_BLOCK_FLAG_NONE;
	block->eid_ref_count = 0;
	block->eid_refs = NULL;
	block->crc_type = BUNDLE_CRC_TYPE_NONE;
	block->length = 0;
	block->data = NULL;
	return block;
}

struct bundle_block_list *bundle_block_entry_create(struct bundle_block *b)
{
	struct bundle_block_list *entry;

	if (b == NULL)
		return NULL;
	entry = malloc(sizeof(struct bundle_block_list));
	if (entry == NULL)
		return NULL;
	entry->data = b;
	entry->next = NULL;
	return entry;
}

void bundle_block_free(struct bundle_block *b)
{
	if (b != NULL) {
		if (b->eid_refs != NULL)
			free(b->eid_refs);
		if (b->data != NULL)
			free(b->data);
		free(b);
	}
}

struct bundle_block_list *bundle_block_entry_free(struct bundle_block_list *e)
{
	struct bundle_block_list *next;

	ASSERT(e != NULL);
	next = e->next;
	bundle_block_free(e->data);
	free(e);
	return next;
}

struct bundle_block *bundle_block_dup(struct bundle_block *b)
{
	struct bundle_block *dup;
	size_t s;

	ASSERT(b != NULL);
	dup = malloc(sizeof(struct bundle_block));
	if (dup == NULL)
		return NULL;
	memcpy(dup, b, sizeof(struct bundle_block));
	if (dup->eid_refs != NULL) {
		s = sizeof(struct eid_reference) * b->eid_ref_count;
		dup->eid_refs = malloc(s);
		if (dup->eid_refs == NULL)
			goto err;
		memcpy(dup->eid_refs, b->eid_refs, s);
	}
	dup->data = malloc(b->length);
	if (dup->data == NULL)
		goto err;
	memcpy(dup->data, b->data, b->length);
	return dup;
err:
	if (dup->eid_refs != NULL)
		free(dup->eid_refs);
	free(dup);
	return NULL;
}

struct bundle_block_list *bundle_block_entry_dup(struct bundle_block_list *e)
{
	struct bundle_block *dup;
	struct bundle_block_list *result;

	ASSERT(e != NULL);
	dup = bundle_block_dup(e->data);
	if (dup == NULL)
		return NULL;
	result = bundle_block_entry_create(dup);
	if (result == NULL)
		bundle_block_free(dup);
	return result;
}

struct bundle_block_list *bundle_block_list_dup(struct bundle_block_list *e)
{
	struct bundle_block_list *dup = NULL, **cur = &dup;

	while (e != NULL) {
		*cur = bundle_block_entry_dup(e);
		if (!*cur) {
			while (dup)
				dup = bundle_block_entry_free(dup);
			return NULL;
		}
		cur = &(*cur)->next;
		e = e->next;
	}
	return dup;
}

enum upcn_result bundle_serialize(struct bundle *bundle,
	void (*write)(const void *cla_obj, const void *, const size_t),
	const void *cla_obj)
{
	// RFC 5050
	if (bundle->protocol_version == 6)
		return bundle6_serialize(bundle, write, cla_obj);
	// BPv7
	else
		return bundle7_serialize(bundle, write, cla_obj);
}

size_t bundle_get_first_fragment_min_size(struct bundle *bundle)
{
	// RFC 5050
	if (bundle->protocol_version == 6)
		return bundle6_get_first_fragment_min_size(bundle);
	// BPv7
	else
		return bundle7_get_first_fragment_min_size(bundle);
}

size_t bundle_get_mid_fragment_min_size(struct bundle *bundle)
{
	// RFC 5050
	if (bundle->protocol_version == 6)
		return bundle6_get_mid_fragment_min_size(bundle);
	// BPv7
	//
	// The payload block is always the last extension block of a bundle.
	// All extension blocks are sent with the first fragment. Therefore
	// there is no distinction between last and middle fragment.
	else
		return bundle7_get_last_fragment_min_size(bundle);
}

size_t bundle_get_last_fragment_min_size(struct bundle *bundle)
{
	// RFC 5050
	if (bundle->protocol_version == 6)
		return bundle6_get_last_fragment_min_size(bundle);
	// BPv7
	else
		return bundle7_get_last_fragment_min_size(bundle);
}



#ifdef INCLUDE_ADVANCED_COMM

/* This possibly needs more than 600 bytes of stack space */
void bundle_print(struct bundle *bundle)
{
	char *tmp;

	hal_io_lock_com_semaphore();
	hal_debug_printf("\nBUNDLE\n======\nPointer:   %p\n", bundle);
	hal_debug_printf("Bdl. ID:   %"PRIu16"\n", bundle->id);
	hal_debug_printf("Headr.Sz.: %"PRIu16" + %d\n",
		bundle->primary_block_length, bundle->primary_prefix_length);
	hal_debug_printf("Seria.Sz.: %"PRIu32"\n",
			 bundle_get_serialized_size(bundle));
	hal_debug_printf("Frag1.Sz.: %"PRIu32"\n",
		bundle_get_first_fragment_min_size(bundle));
	hal_debug_printf("Flags:     %#x\n", bundle->proc_flags);

	hal_debug_printf("Dest.:     %s\n", bundle->destination);
	hal_debug_printf("Source:    %s\n", bundle->source);
	hal_debug_printf("Report to: %s\n", bundle->report_to);

	// RFC 5050
	if (bundle->protocol_version == 6) {
		hal_debug_printf("Custodian: %s\n",
			tmp = bundle6_read_eid(bundle, bundle->custodian_eid));
		free(tmp);
	}
	hal_debug_printf("Timestamp: %"PRIu64"\n", bundle->creation_timestamp);
	hal_debug_printf("SequenceN: %"PRIu16"\n", bundle->sequence_number);
	hal_debug_printf("Lifetime:  %"PRIu64"\n", bundle->lifetime);
	hal_debug_printf("Dict.Len.: %"PRIu16"\n", bundle->dict_length);
	hal_debug_printf("Frag.Off.: %"PRIu32"\n", bundle->fragment_offset);
	hal_debug_printf("Total ADU: %"PRIu32"\n", bundle->total_adu_length);
	hal_debug_printf("Pay.Flags: %#x\n", bundle->payload_block->flags);
	hal_debug_printf("Pay.Len.:  %"PRIu32"\n",
			 bundle->payload_block->length);
	hal_io_unlock_com_semaphore();
}

/* This will use ~80 bytes of stack space */
void bundle_print_stack_conserving(struct bundle *bundle)
{
	char buffer[16];

	hal_io_lock_com_semaphore();
	hal_io_write_string("\nBUNDLE\n======\nPointer:   ");
	hal_platform_sprintu32x(buffer, (uintptr_t)bundle);
	hal_io_write_string(buffer);
	hal_io_write_string("\nBdl. ID:   ");
	hal_platform_sprintu32(buffer, (uint32_t)bundle->id);
	hal_io_write_string(buffer);
	hal_io_write_string("\nHeadr.Sz.: ");
	hal_platform_sprintu32(buffer, bundle->primary_block_length);
	hal_io_write_string(buffer);
	hal_io_write_string(" + ");
	hal_platform_sprintu32(buffer, bundle->primary_prefix_length);
	hal_io_write_string(buffer);
	hal_io_write_string("\nSeria.Sz.: ");
	hal_platform_sprintu32(buffer, bundle_get_serialized_size(bundle));
	hal_io_write_string(buffer);
	hal_io_write_string("\nFlags:     ");
	hal_platform_sprintu32(buffer, bundle->proc_flags);
	hal_io_write_string(buffer);
	hal_io_write_string("\nTimestamp: ");
	hal_platform_sprintu64(buffer, bundle->creation_timestamp);
	hal_io_write_string(buffer);
	hal_io_write_string("\nFrag.Off.: ");
	hal_platform_sprintu32(buffer, bundle->fragment_offset);
	hal_io_write_string(buffer);
	hal_io_write_string("\nTotal ADU: ");
	hal_platform_sprintu32(buffer, bundle->total_adu_length);
	hal_io_write_string(buffer);
	hal_io_write_string("\nPay.Flags: ");
	hal_platform_sprintu32x(buffer, bundle->payload_block->flags);
	hal_io_write_string(buffer);
	hal_io_write_string("\nPay.Len.:  ");
	hal_platform_sprintu32(buffer, bundle->payload_block->length);
	hal_io_write_string(buffer);
	hal_io_write_string("\n");
	hal_io_unlock_com_semaphore();
}

#endif /* INCLUDE_ADVANCED_COMM */
