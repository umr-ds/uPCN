#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "upcn/upcn.h"
#include "upcn/bundle.h"
#include "upcn/groundStation.h"
#include "upcn/routingTable.h"
#include "upcn/router.h"
#include "upcn/eidManager.h"

static struct router_config RC = {
	.fragment_min_payload = FRAGMENT_MIN_PAYLOAD,
	.min_probability = MIN_PROBABILITY,
	.min_gs_confidence_opportunistic = MIN_GS_CONFIDENCE_OPPORTUNISTIC,
	.min_gs_confidence_deterministic = MIN_GS_CONFIDENCE_DETERMINISTIC,
	.gs_trustworthiness_weight = GS_TRUSTWORTHINESS_WEIGHT,
	.gs_reliability_weight = GS_RELIABILITY_WEIGHT,
	.opt_min_time = OPTIMIZATION_MIN_TIME,
	.opt_max_bundles = OPTIMIZATION_MAX_BUNDLES,
	.opt_max_pre_bundles = OPTIMIZATION_MAX_PRE_BUNDLES,
	.opt_max_pre_bundles_contact = OPTIMIZATION_MAX_PRE_BUNDLES_CONTACT,
	.router_min_contacts_htab = ROUTER_MIN_CONTACTS_HTAB,
	.router_min_contacts_nbf = ROUTER_MIN_CONTACTS_NBF,
	.router_nbf_base_reliability = ROUTER_NBF_BASE_RELIABILITY,
	.router_def_base_reliability = ROUTER_DEF_BASE_RELIABILITY
};

void router_optimizer_update_config_int(struct router_config conf);

struct router_config router_get_config(void)
{
	return RC;
}

enum upcn_result router_update_config(struct router_config conf)
{
	float w = conf.gs_trustworthiness_weight + conf.gs_reliability_weight;

	if (conf.min_probability > 1.0f
		|| conf.min_probability <= 0
		|| conf.min_gs_confidence_opportunistic > 1.0f
		|| conf.min_gs_confidence_deterministic > 1.0f
		|| conf.min_gs_confidence_opportunistic <= 0
		|| conf.min_gs_confidence_deterministic <= 0
		|| conf.min_gs_confidence_opportunistic
			> conf.min_gs_confidence_deterministic
		|| conf.min_gs_confidence_deterministic
			< conf.min_probability
		|| w > 1.0001f || w < 0.9999f
		|| conf.opt_max_bundles < 1
		|| conf.opt_max_pre_bundles_contact > conf.opt_max_pre_bundles
		|| conf.router_nbf_base_reliability > 1.0f
		|| conf.router_def_base_reliability > 1.0f
		|| conf.router_nbf_base_reliability <= 0
		|| conf.router_def_base_reliability <= 0
	) {
		return UPCN_FAIL;
	}
	RC = conf;
	router_optimizer_update_config_int(conf);
	return UPCN_OK;
}

static void add_gs_contacts_to_assoc_list(
	struct associated_contact_list **target,
	struct ground_station **gs, uint8_t c, float reliability)
{
	uint8_t i;
	struct contact_list *contact_entry;

	for (i = 0; i < c; i++) {
		contact_entry = gs[i]->contacts;
		while (contact_entry != NULL) {
			add_contact_to_ordered_assoc_list(
				target, contact_entry->data,
				reliability, 0);
			contact_entry = contact_entry->next;
		}
	}
}

struct associated_contact_list *router_lookup_destination(char *dest)
{
	/* extract machineID from dest */
	char *dest_short = malloc(strlen(dest));
	char *target_dest;

	memcpy(dest_short, dest, strlen(dest));

	char *found = strstr(dest_short, "/");

	if (found != NULL) {
		found = strstr(found+1, "/");
		if (found != NULL)
			found = strstr(found+1, "/");
	}

	if (found != NULL) {
		*found = '\0';
		target_dest = dest_short;
	} else {
		target_dest = dest;
	}

	/* lookup extracted machineID part (i.e. leave out appID) */
	struct node_table_entry *entry = routing_table_lookup_eid(target_dest);

	free(dest_short);

	uint16_t results, c;
	struct ground_station *ptrs[ROUTER_MAX_REQ_GS];
	struct associated_contact_list *result = NULL, *cur;

	if (entry == NULL) {
		results = 0;
		cur = NULL;
	} else {
		results = entry->ref_count;
		cur = entry->contacts;
	}
	while (cur != NULL) {
		add_contact_to_ordered_assoc_list(
			&result, cur->data, cur->p, 0);
		cur = cur->next;
	}

	/* NBF lookup if not enough results */
	if (results < RC.router_min_contacts_htab) {
		c = routing_table_lookup_eid_in_nbf(
			dest, ptrs, ROUTER_MAX_REQ_GS);
		results += c;
		add_gs_contacts_to_assoc_list(
			&result, ptrs, c, RC.router_nbf_base_reliability);
	}

	/* Default gateway lookup if not enough results */
	if (results < RC.router_min_contacts_nbf) {
		c = routing_table_lookup_hot_gs(
			ptrs, ROUTER_MAX_REQ_GS);
		/* results += c; */
		add_gs_contacts_to_assoc_list(
			&result, ptrs, c, RC.router_def_base_reliability);
	}

	return result;
}

static inline uint32_t router_get_max_reasonable_fragment_size(
	struct associated_contact_list *contacts, uint32_t full_size,
	uint32_t max_fragment_min_size, uint32_t payload_size,
	enum bundle_routing_priority priority, uint64_t exp_time)
{
	uint32_t payload_capacity = 0;
	uint32_t max_frag_size = UINT32_MAX;
	uint32_t min_capacity, c_capacity;
	int32_t c_pay_capacity;
	struct contact *c;
	float conf, p;

	min_capacity = payload_size / ROUTER_MAX_FRAGMENTS;
	min_capacity += max_fragment_min_size;
	while (contacts != NULL && payload_capacity < payload_size) {
		c = contacts->data;
		p = contacts->p;
		contacts = contacts->next;
		//if (c->to > exp_time)
		//	break;
		c_capacity = ROUTER_CONTACT_CAPACITY(c, priority);
		if (c_capacity < min_capacity)
			continue;
		conf = ROUTER_CONTACT_CONFIDENCE(c, p);
		if (conf >= RC.min_gs_confidence_deterministic) {
			c_pay_capacity = c_capacity - max_fragment_min_size;
			if (c_pay_capacity > RC.fragment_min_payload) {
				payload_capacity += c_pay_capacity;
				if (c_capacity >= full_size)
					break;
				max_frag_size = MIN(max_frag_size, c_capacity);
			}
		} else if (conf >= RC.min_gs_confidence_opportunistic) {
			c_pay_capacity = c_capacity - max_fragment_min_size;
			if (c_pay_capacity > RC.fragment_min_payload) {
				/* Reasonable? */
				payload_capacity
					+= (uint32_t)(conf * c_pay_capacity);
				max_frag_size = MIN(max_frag_size, c_capacity);
			}
		}
	}
	return (payload_capacity < payload_size ? 0 : max_frag_size);
}

uint8_t router_calculate_fragment_route(
	struct fragment_route *res, uint32_t size,
	struct associated_contact_list *contacts, uint32_t preprocessed_size,
	enum bundle_routing_priority priority, uint64_t exp_time,
	struct contact **excluded_contacts, uint8_t excluded_contacts_count)
{
	uint64_t time = hal_time_get_timestamp_s();
	uint32_t cap;
	uint8_t d, i;
	float conf, p;
	struct contact *c;

	res->contact_count = 0;
	res->probability = 0;
	res->preemption_improved = 0;
	while (contacts != NULL
		&& res->probability < RC.min_probability
		&& res->contact_count != ROUTER_MAX_CONTACTS
	) {
		c = contacts->data;
		p = contacts->p;
		contacts = contacts->next;
		d = 0;
		for (i = 0; i < excluded_contacts_count; i++)
			if (c == excluded_contacts[i])
				d = 1;
		if (d)
			continue;
		//if (c->to > exp_time)
		//	break; /* we can stop here (sorted list) */
		if (c->to <= time)
			continue;
		cap = ROUTER_CONTACT_CAPACITY(c, 0);
		if (preprocessed_size != 0) {
			if (preprocessed_size >= cap) {
				preprocessed_size -= cap;
				continue;
			} else {
				cap -= preprocessed_size;
				/* Don't set to zero here, needed in next if */
			}
		}
		if (cap < size) {
			if ((ROUTER_CONTACT_CAPACITY(c, priority)
					- preprocessed_size) >= size)
				res->preemption_improved++;
			preprocessed_size = 0;
			continue;
		} else {
			preprocessed_size = 0;
		}
		conf = ROUTER_CONTACT_CONFIDENCE(c, p);
		if (conf >= RC.min_gs_confidence_deterministic) {
			/* Deterministic routing */
			res->contact_count = 0;
			res->contacts[res->contact_count++] = c;
			res->probability = conf;
			break;
		} else if (conf >= RC.min_gs_confidence_opportunistic) {
			/* Opportunistic routing */
			res->contacts[res->contact_count++] = c;
			/* P(A u B) = P(A) + P(B) - P(A n B) */
			res->probability = res->probability + conf
				- (res->probability * conf);
		}
	}
	return (res->probability >= RC.min_probability);
}

static inline void router_get_first_route_nonfrag(
	struct router_result *res, struct associated_contact_list *contacts,
	struct bundle *bundle, uint32_t bundle_size, uint64_t expiration_time)
{
	res->fragment_results[0].payload_size
		= bundle->payload_block->length;
	/* Determine route */
	if (router_calculate_fragment_route(
		&res->fragment_results[0], bundle_size,
		contacts, 0, ROUTER_BUNDLE_PRIORITY(bundle), expiration_time,
		NULL, 0)
	) {
		res->fragments = 1;
		res->probability
			= res->fragment_results[0].probability;
		res->preemption_improved
			= res->fragment_results[0].preemption_improved;
	}
}

static inline void router_get_first_route_frag(
	struct router_result *res, struct associated_contact_list *contacts,
	struct bundle *bundle, uint32_t bundle_size, uint64_t expiration_time,
	uint32_t max_frag_sz, uint32_t first_frag_sz, uint32_t last_frag_sz)
{
	uint32_t mid_frag_sz, next_frag_sz, remaining_pay, processed_sz;
	int32_t min_pay, max_pay;
	uint8_t success, index;

	/* Determine frag sizes */
	mid_frag_sz = bundle_get_mid_fragment_min_size(bundle);
	next_frag_sz = first_frag_sz;
	if (next_frag_sz > max_frag_sz || last_frag_sz > max_frag_sz)
		return; /* failed */

	remaining_pay = bundle->payload_block->length;
	while (remaining_pay != 0 && res->fragments < ROUTER_MAX_FRAGMENTS) {
		min_pay = MIN(remaining_pay, RC.fragment_min_payload);
		max_pay = max_frag_sz - next_frag_sz;
		if (max_pay < min_pay)
			break;
		if (remaining_pay <= max_frag_sz - last_frag_sz) {
			/* Last fragment */
			res->fragment_results[res->fragments++].payload_size
				= remaining_pay;
			remaining_pay = 0;
		} else {
			/* Another fragment */
			max_pay = MIN((int32_t)remaining_pay, max_pay);
			res->fragment_results[res->fragments++].payload_size
				= max_pay;
			remaining_pay -= max_pay;
			next_frag_sz = mid_frag_sz;
		}
	}
	if (remaining_pay != 0) {
		res->fragments = 0;
		return; /* failed */
	}

	/* Determine routes */
	success = 0;
	res->probability = 1;
	processed_sz = 0;
	for (index = 0; index < res->fragments; index++) {
		bundle_size = res->fragment_results[index].payload_size;
		if (index == 0)
			bundle_size += first_frag_sz;
		else if (index == res->fragments - 1)
			bundle_size += last_frag_sz;
		else
			bundle_size += mid_frag_sz;
		success += router_calculate_fragment_route(
			&res->fragment_results[index], bundle_size,
			contacts, processed_sz, ROUTER_BUNDLE_PRIORITY(bundle),
			expiration_time, NULL, 0);
		res->probability *= res->fragment_results[index].probability;
		res->preemption_improved
			+= res->fragment_results[index].preemption_improved;
		/* FIXME: Fix this for probabilistic routing! */
		processed_sz += bundle_size;
	}
	if (success != res->fragments) {
		res->fragments = 0;
		res->probability = 0;
	}
}

/* max. ~200 bytes on stack */
struct router_result router_get_first_route(struct bundle *bundle)
{
	uint64_t expiration_time = bundle_get_expiration_time(bundle);
	uint32_t bundle_size, first_frag_sz, last_frag_sz, max_frag_size;
	struct router_result res;
	struct associated_contact_list *contacts
		= router_lookup_destination(bundle->destination);

	res.fragments = 0;
	res.probability = 0.0f;
	if (contacts == NULL)
		return res;
	bundle_size = bundle_get_serialized_size(bundle);
	first_frag_sz = bundle_get_first_fragment_min_size(bundle);
	last_frag_sz = bundle_get_last_fragment_min_size(bundle);

	max_frag_size = router_get_max_reasonable_fragment_size(contacts,
		bundle_size, MAX(first_frag_sz, last_frag_sz),
		bundle->payload_block->length,
		ROUTER_BUNDLE_PRIORITY(bundle), expiration_time);
	if (max_frag_size == 0)
		goto finish;
	else if (max_frag_size != UINT32_MAX)
		LOGI("Router: Determined max. frag size != -1", max_frag_size);

	/* TODO: Use RRND to infer contacts on demand */
	if (bundle_must_not_fragment(bundle) || bundle_size <= max_frag_size)
		router_get_first_route_nonfrag(&res,
			contacts, bundle, bundle_size, expiration_time);
	else
		router_get_first_route_frag(&res,
			contacts, bundle, bundle_size, expiration_time,
			max_frag_size, first_frag_sz, last_frag_sz);

finish:
	list_free(contacts);
	return res;
}

/* For use with caching of routes */
struct router_result router_try_reuse(
	struct router_result route, struct bundle *bundle)
{
	uint64_t time = hal_time_get_timestamp_s();
	uint64_t expiration_time = bundle_get_expiration_time(bundle);
	uint32_t remaining_pay = bundle->payload_block->length;
	uint32_t size, min_cap;
	struct fragment_route *fr;
	uint8_t c, f;

	if (route.fragments == 0 || route.probability < RC.min_probability)
		return route;

	/* Not fragmented */
	if (bundle_must_not_fragment(bundle) || route.fragments == 1) {
		size = bundle_get_serialized_size(bundle);
		fr = &route.fragment_results[0];
		fr->payload_size = remaining_pay;
		for (c = 0; c < fr->contact_count; c++) {
			if (fr->contacts[c]->to <= time
				|| fr->contacts[c]->to > expiration_time
				|| ROUTER_CONTACT_CAPACITY(fr->contacts[c], 0)
					< (int32_t)size
			) {
				route.fragments = 0;
				route.probability = 0;
				return route;
			}
		}
		return route;
	}

	/* Fragmented */
	for (f = 0; f < route.fragments; f++) {
		if (f == 0)
			size = bundle_get_first_fragment_min_size(bundle);
		else if (f == route.fragments - 1)
			size = bundle_get_last_fragment_min_size(bundle);
		else
			size = bundle_get_mid_fragment_min_size(bundle);
		fr = &route.fragment_results[f];
		min_cap = UINT32_MAX;
		for (c = 0; c < fr->contact_count; c++) {
			if (fr->contacts[c]->to <= time
				|| fr->contacts[c]->to > expiration_time
				|| ROUTER_CONTACT_CAPACITY(fr->contacts[c], 0)
					< (int32_t)(size
						+ RC.fragment_min_payload)
			) {
				route.fragments = 0;
				route.probability = 0;
				return route;
			}
			min_cap = MIN(min_cap,
				ROUTER_CONTACT_CAPACITY(fr->contacts[c], 0)
				- size);

		}
		fr->payload_size = MIN(remaining_pay, min_cap);
		remaining_pay -= fr->payload_size;
		if (remaining_pay == 0) {
			route.fragments = f - 1;
			return route;
		}
	}

	if (remaining_pay != 0) {
		route.fragments = 0;
		route.probability = 0;
	}
	return route;
}

enum upcn_result router_add_bundle_to_contact(
	struct contact *contact, struct routed_bundle *rb)
{
	struct routed_bundle_list *new_entry, **cur_entry;

	ASSERT(contact != NULL);
	ASSERT(rb != NULL);
	ASSERT(contact->remaining_capacity_p0 > 0);
	new_entry = malloc(sizeof(struct routed_bundle_list));
	if (new_entry == NULL)
		return UPCN_FAIL;
	new_entry->data = rb;
	new_entry->next = NULL;
	cur_entry = &contact->contact_bundles;
	if (*cur_entry == NULL
			&& HAS_FLAG(contact->ground_station->flags,
				GS_FLAG_DISCOVERED))
		routing_table_create_contacts(contact->ground_station, 0);
	/* Go to end of list (=> FIFO) */
	while (*cur_entry != NULL)
		cur_entry = &(*cur_entry)->next;
	*cur_entry = new_entry;
	contact->bundle_count++;
	contact->remaining_capacity_p0 -= rb->size;
	if (rb->prio > BUNDLE_RPRIO_LOW) {
		contact->remaining_capacity_p1 -= rb->size;
		if (rb->prio != BUNDLE_RPRIO_NORMAL)
			contact->remaining_capacity_p2 -= rb->size;
	}
	return UPCN_OK;
}

struct routed_bundle *router_remove_bundle_from_contact(
	struct contact *contact, bundleid_t id)
{
	struct routed_bundle *rb;
	struct routed_bundle_list **cur_entry, *tmp;

	ASSERT(contact != NULL);
	cur_entry = &contact->contact_bundles;
	/* Find rb */
	while (*cur_entry != NULL) {
		ASSERT((*cur_entry)->data != NULL);
		if ((*cur_entry)->data->id == id) {
			rb = (*cur_entry)->data;
			tmp = *cur_entry;
			*cur_entry = (*cur_entry)->next;
			free(tmp);
			contact->bundle_count--;
			contact->remaining_capacity_p0 += rb->size;
			if (rb->prio > BUNDLE_RPRIO_LOW) {
				contact->remaining_capacity_p1 += rb->size;
				if (rb->prio != BUNDLE_RPRIO_NORMAL)
					contact->remaining_capacity_p2
						+= rb->size;
			}
			return rb;
		}
		cur_entry = &(*cur_entry)->next;
	}
	return NULL;
}

uint8_t router_add_bundle_to_route(struct fragment_route *r, struct bundle *b)
{
	uint8_t success;
	struct routed_bundle *rb;

	ASSERT(r != NULL);
	ASSERT(b != NULL);
	rb = malloc(sizeof(struct routed_bundle));
	if (rb == NULL)
		return 0;
	rb->id = b->id;
	rb->prio = bundle_get_routing_priority(b);
	rb->size = bundle_get_serialized_size(b);
	rb->exp_time = bundle_get_expiration_time(b);
	rb->destination = eidmanager_alloc_ref(b->destination, false);
	if (rb->destination == NULL) {
		free(rb);
		return 0;
	}
	success = router_update_routed_bundle(r, rb);
	if (!success) {
		eidmanager_free_ref(rb->destination);
		free(rb);
		return 0;
	}
	return 1;
}

uint8_t router_update_routed_bundle(
	struct fragment_route *r, struct routed_bundle *rb)
{
	uint8_t c, added_contacts;

	ASSERT(r != NULL);
	ASSERT(rb != NULL);
	ASSERT(r->contact_count != 0);
	rb->preemption_improvement = r->preemption_improved;
	rb->contact_count = r->contact_count;
	rb->contacts = malloc(sizeof(void *) * r->contact_count);
	if (rb->contacts == NULL)
		return 0;
	rb->transmitted = 0;
	rb->serialized = 0;
	added_contacts = 0;
	for (c = 0; c < r->contact_count; c++) {
		if (router_add_bundle_to_contact(r->contacts[c], rb)
				== UPCN_OK)
			rb->contacts[added_contacts++] = r->contacts[c];
	}
	if (added_contacts != rb->contact_count) {
		if (added_contacts == 0) {
			free(rb->contacts);
			return 0;
		}
		rb->contact_count = added_contacts;
	}
	return 1;
}

void router_remove_bundle_from_route(
	struct fragment_route *r, bundleid_t id, uint8_t free_rb)
{
	uint8_t c;
	struct routed_bundle *rb = NULL, *tmp;

	ASSERT(r != NULL);
	for (c = 0; c < r->contact_count; c++) {
		tmp = router_remove_bundle_from_contact(
			r->contacts[c], id);
		if (tmp != NULL)
			rb = tmp;
	}
	if (free_rb && rb != NULL) {
		eidmanager_free_ref(rb->destination);
		free(rb->contacts);
		free(rb);
	}
}
