#include <stdlib.h>

#include "upcn/upcn.h"
#include "upcn/beacon.h"
#include "upcn/eidManager.h"

struct beacon *beacon_init(void)
{
	struct beacon *ret = malloc(sizeof(struct beacon));

	if (ret == NULL)
		return NULL;
	ret->version = 0;
	ret->flags = BEACON_FLAG_NONE;
	ret->sequence_number = 0;
	ret->eid = NULL;
	ret->service_count = 0;
	ret->services = NULL;
	ret->period = -1;
	return ret;
}

static inline void tlv_children_free(
	const uint16_t count, struct tlv_definition *list);

static inline void tlv_child_free_value(struct tlv_definition *const ptr)
{
	ASSERT(ptr != NULL);
	if (ptr->tag >= 64) {
		tlv_children_free(
			ptr->value.children.count,
			ptr->value.children.list
		);
	} else if (ptr->tag == 8 || ptr->tag == 9) {
		free(ptr->value.b);
	}
}

static inline void tlv_children_free(
	const uint16_t count, struct tlv_definition *list)
{
	// If the list could not be allocated (MAX_SVC_COUNT reached), it will
	// be NULL though beacon->service_count is greater than 0.
	if (list == NULL)
		return;
	for (int i = 0; i < count; i++)
		tlv_child_free_value(&list[i]);
	free(list);
}

void beacon_free(struct beacon *beacon)
{
	ASSERT(beacon != NULL);
	tlv_children_free(beacon->service_count, beacon->services);
	if (beacon->eid != NULL)
		eidmanager_free_ref(beacon->eid);
	free(beacon);
}

void tlv_free(struct tlv_definition *tlv)
{
	tlv_child_free_value(tlv);
	free(tlv);
}

enum upcn_result tlv_populate(struct tlv_definition *const tlv_list,
	const uint8_t child, const enum tlv_type tag, const uint16_t length,
	const union tlv_value value)
{
	struct tlv_definition *const cur = &tlv_list[child];

	cur->tag = tag;
	cur->length = length;
	cur->value = value;
	return UPCN_OK;
}

struct tlv_definition *tlv_populate_ct(struct tlv_definition *const tlv_list,
	const uint8_t child, const enum tlv_type tag, const uint8_t children)
{
	struct tlv_definition *const cur = &tlv_list[child];

	if (tag < 64)
		return NULL;
	cur->tag = tag;
	cur->length = 0;
	cur->value.children.count = children;
	/* Should be zeroed => calloc */
	cur->value.children.list
		= calloc(children, sizeof(struct tlv_definition));
	return cur->value.children.list;
}

struct tlv_definition *beacon_get_service(
	const struct beacon *const beacon, const enum tlv_type tag)
{
	ASSERT(beacon != NULL);
	if (!HAS_FLAG(beacon->flags, BEACON_FLAG_HAS_SERVICES)
			|| beacon->services == NULL)
		return NULL;
	for (int i = 0; i < beacon->service_count; i++) {
		if (beacon->services[i].tag == tag)
			return &beacon->services[i];
	}
	return NULL;
}

struct tlv_definition *tlv_get_child(
	const struct tlv_definition *const tlv, const enum tlv_type tag)
{
	if (tlv == NULL)
		return NULL;
	if (tlv->tag < 64)
		return NULL;
	for (int i = 0; i < tlv->value.children.count; i++) {
		if (tlv->value.children.list[i].tag == tag)
			return &tlv->value.children.list[i];
	}
	return NULL;
}
