#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "upcn/upcn.h"
#include <cla.h>
#include "upcn/groundStation.h"
#include "drv/llsort.h"
#include "upcn/eidManager.h"
#include "upcn/bloomFilter.h"

/* TODO: Synchronize access. NOT THREAD SAFE ATM! */
struct bloom_filter *eidlist_bf;

struct ground_station *ground_station_create(char *eid)
{
	struct ground_station *ret = malloc(sizeof(struct ground_station));

	if (ret == NULL)
		return NULL;

	/* Setup CLA struct */
	ret->cla_config = malloc(sizeof(struct cla_config));

	if (ret->cla_config == NULL) {
		free(ret);
		return NULL;
	}
	ret->cla_config = cla_allocate_cla_config();
	cla_init_config_struct(ret->cla_config);

	ret->cla = CLA_UNKNOWN;
	ret->cla_addr = NULL;
	ret->flags = GS_FLAG_NONE;
	ret->trustworthiness = 1.0f;
	ret->endpoints = NULL;
	ret->contacts = NULL;
	ret->nbf = NULL;
	ret->rrnd_info = NULL;
	if (eid == NULL)
		ret->eid = NULL;
	else
		ret->eid = eidmanager_alloc_ref(eid, false);
	return ret;
}

struct rrnd_gs_info *ground_station_rrnd_info_create(
	struct ground_station *const gs)
{
	struct rrnd_gs_info *ret = malloc(sizeof(struct rrnd_gs_info));

	if (ret == NULL)
		return NULL;
	memset(ret, 0, sizeof(struct rrnd_gs_info));
	ret->gs_reference = gs;
	return ret;
}

struct contact *contact_create(struct ground_station *station)
{
	struct contact *ret = malloc(sizeof(struct contact));

	if (ret == NULL)
		return NULL;
	ret->ground_station = station;
	ret->from = 0;
	ret->to = 0;
	ret->bitrate = 0;
	ret->total_capacity = 0;
	ret->remaining_capacity_p0 = 0;
	ret->remaining_capacity_p1 = 0;
	ret->remaining_capacity_p2 = 0;
	ret->contact_endpoints = NULL;
	ret->contact_bundles = NULL;
	ret->bundle_count = 0;
	ret->active = 0;
	return ret;
}

static void free_contact_internal(
	struct contact *contact, int free_eid_list)
{
	struct endpoint_list *cur_eid;
	struct routed_bundle_list *next, *cur_bundle;

	if (contact == NULL)
		return;
	ASSERT(contact->active == 0);
	if (free_eid_list) {
		cur_eid = contact->contact_endpoints;
		while (cur_eid != NULL)
			cur_eid = endpoint_list_free(cur_eid);
	}
	/* Free associated bundle list (not bundles themselves) */
	cur_bundle = contact->contact_bundles;
	while (cur_bundle != NULL) {
		next = cur_bundle->next;
		free(cur_bundle);
		cur_bundle = next;
	}
	free(contact);
}

void free_contact(struct contact *contact)
{
	free_contact_internal(contact, 1);
}

static struct contact_list *contact_list_free_internal(
	struct contact_list *e, int free_eid_list);

void free_ground_station(struct ground_station *gs)
{
	struct endpoint_list *cur_eid;
	struct contact_list *cur_contact;

	if (gs == NULL)
		return;
	free(gs->nbf);
	free(gs->rrnd_info);
	cur_eid = gs->endpoints;
	while (cur_eid != NULL)
		cur_eid = endpoint_list_free(cur_eid);
	cur_contact = gs->contacts;
	while (cur_contact != NULL)
		cur_contact = contact_list_free_internal(cur_contact, 1);
	free(gs->cla_addr);
	eidmanager_free_ref(gs->eid);
	free(gs);
}

struct endpoint_list *endpoint_list_free(struct endpoint_list *e)
{
	struct endpoint_list *next;

	if (e == NULL)
		return NULL;
	next = e->next;
	eidmanager_free_ref(e->eid);
	free(e);
	return next;
}

struct endpoint_list *endpoint_list_add(
	struct endpoint_list **list, char *eid)
{
	struct endpoint_list **cur_entry, *new_entry;

	ASSERT(list != NULL);
	ASSERT(eid != NULL);
	eid = eidmanager_get_ref(eid);
	ASSERT(eid != NULL);
	cur_entry = list;
	while (*cur_entry != NULL) {
		if ((*cur_entry)->eid == eid)
			return NULL;
		if ((*cur_entry)->eid > eid)
			break;
		cur_entry = &(*cur_entry)->next;
	}
	new_entry = malloc(sizeof(struct endpoint_list));
	if (new_entry == NULL)
		return NULL;
	new_entry->eid = eid;
	new_entry->next = *cur_entry;
	*cur_entry = new_entry;
	return new_entry;
}

int endpoint_list_remove(struct endpoint_list **list, char *eid)
{
	struct endpoint_list **cur_entry, *tmp;

	ASSERT(list != NULL);
	ASSERT(eid != NULL);
	eid = eidmanager_get_ref(eid);
	ASSERT(eid != NULL);
	cur_entry = list;
	while (*cur_entry != NULL) {
		if ((*cur_entry)->eid == eid) {
			tmp = *cur_entry;
			*cur_entry = (*cur_entry)->next;
			free(tmp);
			return 1;
		}
		cur_entry = &(*cur_entry)->next;
	}
	return 0;
}

int endpoint_list_sorted(struct endpoint_list *list)
{
	char *last = NULL;

	while (list != NULL) {
		if (list->eid < last)
			return 0;
		last = list->eid;
		list = list->next;
	}
	return 1;
}

static struct endpoint_list *create_epl(
	char *dynamic_eid, struct endpoint_list *successor)
{
	struct endpoint_list *ret = malloc(sizeof(struct endpoint_list));

	if (ret == NULL)
		return NULL;
	ret->eid = eidmanager_alloc_ref(dynamic_eid, true);
	ret->next = successor;
	return ret;
}

struct endpoint_list *endpoint_list_union(
	struct endpoint_list *a, struct endpoint_list *b, const int copy_b)
{
	struct endpoint_list **cur_slot = &a;
	struct endpoint_list *cur_can = b, *next_can;

	ASSERT(endpoint_list_sorted(a));
	ASSERT(endpoint_list_sorted(b));
	if (a == NULL && !copy_b)
		return b;
	else if (b == NULL)
		return a;

	while (*cur_slot != NULL && cur_can != NULL) {
		/* Traverse b linearly and insert EIDs "smaller" than the
		 * current one in a before it.
		 */
		while (cur_can != NULL && *cur_slot != NULL
				&& cur_can->eid <= (*cur_slot)->eid) {
			next_can = cur_can->next;
			if (cur_can->eid < (*cur_slot)->eid) {
				/* < : Insert before */
				if (!copy_b) {
					cur_can->next = *cur_slot;
					*cur_slot = cur_can;
					cur_slot = &cur_can->next;
				} else {
					*cur_slot = create_epl(
						cur_can->eid, *cur_slot);
					if (*cur_slot) /* FIXME */
						cur_slot = &(*cur_slot)->next;
				}
			} else if (!copy_b) {
				endpoint_list_free(cur_can);
			}
			cur_can = next_can;
		}
		if (*cur_slot != NULL)
			cur_slot = &(*cur_slot)->next;
	}
	if (!copy_b) {
		/* Concatenate list ends */
		*cur_slot = cur_can;
	} else {
		while (cur_can != NULL) {
			*cur_slot = create_epl(cur_can->eid, NULL);
			if (!*cur_slot)
				break; /* FIXME */
			cur_slot = &(*cur_slot)->next;
		}
	}
	return a;
}

struct endpoint_list *endpoint_list_difference(
	struct endpoint_list *a, struct endpoint_list *b, const int free_b)
{
	struct endpoint_list **cur_slot = &a;
	struct endpoint_list *cur_can = b;

	ASSERT(endpoint_list_sorted(a));
	ASSERT(endpoint_list_sorted(b));
	if (a == NULL || b == NULL)
		return a;
	while (*cur_slot != NULL) {
		while (*cur_slot != NULL
			&& cur_can != NULL && cur_can->eid <= (*cur_slot)->eid
		) {
			if (cur_can->eid == (*cur_slot)->eid)
				*cur_slot = endpoint_list_free(*cur_slot);
			if (free_b)
				cur_can = endpoint_list_free(cur_can);
			else
				cur_can = cur_can->next;
		}
		if (*cur_slot != NULL)
			cur_slot = &(*cur_slot)->next;
	}
	return a;
}

int contact_list_sorted(struct contact_list *cl, const int order_by_from)
{
	uint64_t last = 0;

	if (order_by_from) {
		while (cl != NULL) {
			if (cl->data->from < last)
				return 0;
			last = cl->data->from;
			cl = cl->next;
		}
	} else {
		while (cl != NULL) {
			if (cl->data->to < last)
				return 0;
			last = cl->data->to;
			cl = cl->next;
		}
	}
	return 1;
}

struct contact_list *contact_list_free(struct contact_list *e)
{
	struct contact_list *next;

	if (e == NULL)
		return NULL;
	next = e->next;
	free_contact(e->data);
	free(e);
	return next;
}

static struct contact_list *contact_list_free_internal(
	struct contact_list *e, int free_eid_list)
{
	struct contact_list *next;

	if (e == NULL)
		return NULL;
	next = e->next;
	free_contact_internal(e->data, free_eid_list);
	free(e);
	return next;
}

static inline void add_to_modified_list(
	struct contact *c, struct contact_list **modified)
{
	struct contact_list *l;

	if (modified != NULL) {
		l = malloc(sizeof(struct contact_list));
		if (l != NULL) {
			l->next = *modified;
			l->data = c;
			*modified = l;
		}
	}
}

static inline bool merge_contacts(struct contact *old, struct contact *new)
{
	/* Union EID lists */
	old->contact_endpoints = endpoint_list_union(
		old->contact_endpoints, new->contact_endpoints, 0);
	/* Update bitrate, if modified (cap changed) => return true */
	if (old->bitrate != new->bitrate) {
		old->bitrate = new->bitrate;
		recalculate_contact_capacity(old);
		return true;
	}
	return false;
}

struct contact_list *contact_list_union(
	struct contact_list *a, struct contact_list *b,
	struct contact_list **modf)
{
	struct contact_list **cur_slot = &a;
	struct contact_list *cur_can = b, *next_can;
	uint64_t cur_from;
	bool modified;

	ASSERT(contact_list_sorted(a, 1));
	ASSERT(contact_list_sorted(b, 1));
	if (a == NULL)
		return b;
	else if (b == NULL)
		return a;
	while (*cur_slot != NULL) {
		cur_from = (*cur_slot)->data->from;
		/* Traverse b linearly and insert contacts "smaller" than the
		 * current one in a before it.
		 */
		while (cur_can != NULL && cur_can->data->from <= cur_from) {
			next_can = cur_can->next;
			if (cur_can->data->from < cur_from) {
				/* < : Insert before */
				cur_can->next = *cur_slot;
				*cur_slot = cur_can;
				cur_slot = &cur_can->next;
			} else {
				/* == : Update existing */
				if (cur_can->data->to == (*cur_slot)->data->to
				) {
					modified = merge_contacts(
						(*cur_slot)->data,
						cur_can->data);
					if (modified)
						add_to_modified_list(
							(*cur_slot)->data,
							modf);
				}
				/* Remove redundant list element */
				/* XXX Currently contacts beginning at the
				 * same time are considered invalid and
				 * are thus deleted
				 * TODO: Check if same station
				 */
				contact_list_free_internal(cur_can, 0);
			}
			cur_can = next_can;
		}
		cur_slot = &(*cur_slot)->next;
	}
	/* Concatenate list ends */
	*cur_slot = cur_can;
	return a;
}

struct contact_list *contact_list_difference(
	struct contact_list *a, struct contact_list *b, const int free_b,
	struct contact_list **modf, struct contact_list **deleted)
{
	struct contact_list **cur_slot = &a;
	struct contact_list *cur_can = b, *l;

	ASSERT(contact_list_sorted(a, 1));
	ASSERT(contact_list_sorted(b, 1));
	if (a == NULL || b == NULL)
		return a;
	while (*cur_slot != NULL) {
		while (*cur_slot != NULL
			&& cur_can != NULL && cur_can->data->from
				<= (*cur_slot)->data->from
		) {
			if (cur_can->data->from == (*cur_slot)->data->from
				&& cur_can->data->to == (*cur_slot)->data->to
			) {
				if (cur_can->data->contact_endpoints == NULL) {
					/* Add to "deleted" list and rm */
					if (deleted != NULL) {
						l = *cur_slot;
						*cur_slot = l->next;
						l->next = *deleted;
						*deleted = l;
					} else if ((*cur_slot)->data->active) {
						l = *cur_slot;
						*cur_slot = l->next;
						free(l);
					} else {
						*cur_slot
						= contact_list_free_internal(
							*cur_slot, 1);
					}
				} else {
					/* Calculate contact difference */
					(*cur_slot)->data->contact_endpoints =
						endpoint_list_difference(
						(*cur_slot)->data
							->contact_endpoints,
						cur_can->data
							->contact_endpoints, 0);
					/* Add to "modified" list */
					add_to_modified_list(
						(*cur_slot)->data, modf);
					cur_slot = &((*cur_slot)->next);
				}
			}
			if (free_b)
				cur_can = contact_list_free_internal(
					cur_can, 1);
			else
				cur_can = cur_can->next;
		}
		if (*cur_slot != NULL)
			cur_slot = &(*cur_slot)->next;
	}
	return a;
}

struct endpoint_list *endpoint_list_strip_and_sort(struct endpoint_list *el)
{
	struct endpoint_list *cur = el, **i;

	while (cur != NULL) {
		i = &cur->next;
		while (*i != NULL) {
			if (strcmp((*i)->eid, cur->eid) == 0)
				*i = endpoint_list_free(*i);
			else
				i = &(*i)->next;
		}
		cur = cur->next;
	}
	/* Sorted by ptr value */
	LLSORT(struct endpoint_list, eid, el);
	return el;
}

static int contacts_overlap(struct contact *a, struct contact *b)
{
	return (
		(a->from >= b->from && a->from <= b->to) ||
		(a->to >= b->from && a->to <= b->to)
	);
}

int ground_station_prepare_and_verify(struct ground_station *gs)
{
	struct contact_list *cl, *i;

	LLSORT(struct contact_list, data->from, gs->contacts);
	cl = gs->contacts;
	gs->endpoints = endpoint_list_strip_and_sort(gs->endpoints);
	while (cl != NULL) {
		cl->data->contact_endpoints = endpoint_list_strip_and_sort(
			cl->data->contact_endpoints);
		i = cl->next;
		while (i != NULL) {
			if (contacts_overlap(cl->data, i->data))
				return 0;
			i = i->next;
		}
		cl = cl->next;
	}
	return 1;
}

void recalculate_contact_capacity(struct contact *contact)
{
	uint64_t duration, new_capacity;
	int32_t capacity_difference;

	ASSERT(contact != NULL);
	duration = contact->to - contact->from;
	// NOTE: We don't support contacts longer than 4 M. s
	if (duration > UINT32_MAX)
		duration = UINT32_MAX;
	new_capacity = duration * contact->bitrate;
	// NOTE: We don't support transferring > 2.147 GB during a contact
	if (new_capacity > INT32_MAX)
		new_capacity = INT32_MAX;
	capacity_difference = new_capacity - (int32_t)contact->total_capacity;
	contact->total_capacity = (uint32_t)new_capacity;
	contact->remaining_capacity_p0 += capacity_difference;
	contact->remaining_capacity_p1 += capacity_difference;
	contact->remaining_capacity_p2 += capacity_difference;
}

int32_t contact_get_cur_remaining_capacity(
	struct contact *contact, enum bundle_routing_priority prio)
{
	uint64_t time;
	uint64_t cap_left;
	int32_t cap_result;

	ASSERT(contact != NULL);
	time = hal_time_get_timestamp_s();
	if (time >= contact->to)
		return 0;
	if (time <= contact->from)
		return CONTACT_CAPACITY(contact, prio);
	cap_left = contact->total_capacity
		* (uint64_t)(contact->to - time)
		/ (uint64_t)(contact->to - contact->from);
	// NOTE: We don't support transferring > 2.147 GB during a contact
	if (cap_left > INT32_MAX)
		cap_left = INT32_MAX;
	cap_result = cap_left;
	return MIN(cap_result, CONTACT_CAPACITY(contact, prio));
}

int add_contact_to_ordered_list(
	struct contact_list **list, struct contact *contact,
	const int order_by_from)
{
	struct contact_list **cur_entry, *new_entry;

	ASSERT(list != NULL);
	ASSERT(contact != NULL);
	cur_entry = list;
	while (*cur_entry != NULL) {
		if ((*cur_entry)->data == contact)
			return 0;
		if (order_by_from) {
			if ((*cur_entry)->data->from > contact->from)
				break;
		} else {
			if ((*cur_entry)->data->to > contact->to)
				break;
		}
		cur_entry = &(*cur_entry)->next;
	}
	new_entry = malloc(sizeof(struct contact_list));
	if (new_entry == NULL)
		return 0;
	new_entry->data = contact;
	new_entry->next = *cur_entry;
	*cur_entry = new_entry;
	return 1;
}

int remove_contact_from_list(
	struct contact_list **list, struct contact *contact)
{
	struct contact_list **cur_entry, *tmp;

	ASSERT(list != NULL);
	ASSERT(contact != NULL);
	cur_entry = list;
	while (*cur_entry != NULL) {
		if ((*cur_entry)->data == contact) {
			tmp = *cur_entry;
			*cur_entry = (*cur_entry)->next;
			free(tmp);
			return 1;
		}
		cur_entry = &(*cur_entry)->next;
	}
	return 0;
}

/* TODO: This is a bit ugly as it is copy-pasted. Find a way to unify... */
int add_contact_to_ordered_assoc_list(
	struct associated_contact_list **list, struct contact *contact,
	float p, const int order_by_from)
{
	struct associated_contact_list **cur_entry, *new_entry;

	ASSERT(list != NULL);
	ASSERT(contact != NULL);
	ASSERT(p >= 0.0f && p <= 1.0f);
	cur_entry = list;
	while (*cur_entry != NULL) {
		if ((*cur_entry)->data == contact)
			return 0;
		if (order_by_from) {
			if ((*cur_entry)->data->from > contact->from)
				break;
		} else {
			if ((*cur_entry)->data->to > contact->to)
				break;
		}
		cur_entry = &(*cur_entry)->next;
	}
	new_entry = malloc(sizeof(struct associated_contact_list));
	if (new_entry == NULL)
		return 0;
	new_entry->data = contact;
	new_entry->p = p;
	new_entry->next = *cur_entry;
	*cur_entry = new_entry;
	return 1;
}

/* TODO: This is a bit ugly as it is copy-pasted. Find a way to unify... */
int remove_contact_from_assoc_list(
	struct associated_contact_list **list, struct contact *contact)
{
	struct associated_contact_list **cur_entry, *tmp;

	ASSERT(list != NULL);
	ASSERT(contact != NULL);
	cur_entry = list;
	while (*cur_entry != NULL) {
		if ((*cur_entry)->data == contact) {
			tmp = *cur_entry;
			*cur_entry = (*cur_entry)->next;
			free(tmp);
			return 1;
		}
		cur_entry = &(*cur_entry)->next;
	}
	return 0;
}

struct contact *contact_from_rrnd_contact_info(
	struct ground_station *station, struct rrnd_contact_info *rci)
{
	struct contact *ret = contact_create(station);

	/* TODO: honor probability and dev. of capacity */
	ret->from = rci->start / 1000;
	ret->to = (rci->start + rci->duration) / 1000;
	ret->bitrate = rci->capacity.mean / rci->duration / 8;
	recalculate_contact_capacity(ret);
	return ret;
}
