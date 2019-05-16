#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include <hal_debug.h>

#include "upcn/upcn.h"
#include "upcn/bundle.h"
#include "upcn/groundStation.h"
#include "upcn/simplehtab.h"
#include "upcn/routingTable.h"
#include "upcn/bundleProcessor.h"
#include "upcn/bundleStorageManager.h"
#include "upcn/bloomFilter.h"
#include "upcn/eidManager.h"
#include "upcn/router.h"
#include "upcn/rrnd.h"
#include "upcn/satpos.h"

#include "cla_defines.h"

static struct ground_station_list *station_list;
static struct contact_list *contact_list;

static struct htab_entrylist *htab_elem[NODE_HTAB_SLOT_COUNT];
static struct htab eid_table;
static uint8_t eid_table_initialized;

/* INIT */

enum upcn_result routing_table_init(void)
{
	if (eid_table_initialized != 0)
		return UPCN_OK;
	station_list = NULL;
	contact_list = NULL;
	/* Alternative initialization ... HTAB in SRAM */
	eid_table.slot_count = NODE_HTAB_SLOT_COUNT;
	eid_table.elements = htab_elem;
	htab_init(&eid_table);
	eid_table_initialized = 1;
	return UPCN_OK;
}

void routing_table_free(void)
{
	struct ground_station_list *next;

	while (contact_list != NULL)
		routing_table_delete_contact(contact_list->data);
	while (station_list != NULL) {
		free_ground_station(station_list->station);
		next = station_list->next;
		free(station_list);
		station_list = next;
	}
}

/* LOOKUP */

static struct ground_station_list **get_ground_station_entry_ptr_by_eid(
	const char *eid)
{
	struct ground_station_list **cur = &station_list;

	if (eid == NULL)
		return NULL;
	/* Linear search for now */
	while ((*cur) != NULL) {
		if (strcmp((*cur)->station->eid, eid) == 0)
			return cur;
		cur = &((*cur)->next);
	}
	return NULL;
}

static struct ground_station_list *get_ground_station_entry_by_eid(
	const char *eid)
{
	struct ground_station_list **entry_ptr
		= get_ground_station_entry_ptr_by_eid(eid);

	if (entry_ptr == NULL)
		return NULL;
	else
		return *entry_ptr;
}

struct ground_station *routing_table_lookup_ground_station(const char *eid)
{
	struct ground_station_list *entry
		= get_ground_station_entry_by_eid(eid);

	if (entry == NULL)
		return NULL;
	else
		return entry->station;
}

struct node_table_entry *routing_table_lookup_eid(const char *eid)
{
	return (struct node_table_entry *)htab_get(&eid_table, eid);
}

uint8_t routing_table_lookup_eid_in_nbf(
	char *eid, struct ground_station **target, uint8_t max)
{
	struct ground_station_list *cur = station_list;
	uint8_t c = 0;

	while (cur != NULL) {
		if (cur->station->nbf != NULL) {
			if (bloom_filter_contains(
				cur->station->nbf, eid)
			) {
				target[c] = cur->station;
				if (++c == max)
					break;
			}
		}
		cur = cur->next;
	}
	return c;
}

uint8_t routing_table_lookup_hot_gs(
	struct ground_station **target, uint8_t max)
{
	struct ground_station_list *cur = station_list;
	uint8_t c = 0;

	while (cur != NULL) {
		if (HAS_FLAG(cur->station->flags, GS_FLAG_INTERNET_ACCESS)) {
			target[c] = cur->station;
			if (++c == max)
				break;
		}
		cur = cur->next;
	}
	return c;
}

/* GS LIST MODIFICATION */

static uint16_t count_unused_contacts(struct ground_station *gs);

static void add_gs_to_tables(struct ground_station *gs);
static void remove_gs_from_tables(struct ground_station *gs);

static void drop_contacts(struct contact_list **gs_clist,
			  QueueIdentifier_t bproc_signaling_queue);
static void reschedule_bundles(
	struct contact *contact, QueueIdentifier_t bproc_signaling_queue);

static enum upcn_result add_new_gs(
	struct ground_station *new_gs, QueueIdentifier_t signaling_queue)
{
	struct ground_station_list *new_elem;

	new_elem = malloc(sizeof(struct ground_station_list));
	if (new_elem == NULL) {
		free_ground_station(new_gs);
		return UPCN_FAIL;
	}
	new_elem->station = new_gs;
	new_elem->next = station_list;
	station_list = new_elem;
	if (HAS_FLAG(new_gs->flags, GS_FLAG_DISCOVERED))
		routing_table_create_contacts(new_gs, 1);
	add_gs_to_tables(new_gs);
	return UPCN_OK;
}

void routing_table_add_gs(
	struct ground_station *new_gs, QueueIdentifier_t router_signaling_queue,
	QueueIdentifier_t bproc_signaling_queue)
{
	struct ground_station_list *entry;
	struct ground_station *cur_gs;
	struct contact_list *cap_modified = NULL, *cur_contact, *next;

	ASSERT(new_gs != NULL);
	entry = get_ground_station_entry_by_eid(new_gs->eid);
	if (entry == NULL) {
		add_new_gs(new_gs, router_signaling_queue);
	} else {
		cur_gs = entry->station;
		/* Should not be needed here because we only ADD */
		/* remove_gs_from_htab(cur_gs); */
		cur_gs->endpoints = endpoint_list_union(
			cur_gs->endpoints, new_gs->endpoints, 0);
		cur_gs->contacts = contact_list_union(
			cur_gs->contacts, new_gs->contacts,
			&cap_modified);
		/* Fix ground station assignment for all contacts */
		/* (normally only needed for new contacts) */
		cur_contact = cur_gs->contacts;
		while (cur_contact != NULL) {
			cur_contact->data->ground_station = cur_gs;
			cur_contact = cur_contact->next;
		}
		/* Process contacts with modified capacity */
		while (cap_modified != NULL) {
			/* Check for all GS with modified bandwidth if now */
			/* the remaining capacity for p0 is negative: */
			/* We assume that the new contact capacity */
			/* has already been calculated */
			/* TODO: This can be improved! */
			if (cap_modified->data->remaining_capacity_p0 < 0) {
				reschedule_bundles(cap_modified->data,
					bproc_signaling_queue);
			}
			next = cap_modified->next;
			free(cap_modified);
			cap_modified = next;
		}
		if (HAS_FLAG(cur_gs->flags, GS_FLAG_DISCOVERED))
			routing_table_create_contacts(cur_gs, 1);
		add_gs_to_tables(cur_gs);
		eidmanager_free_ref(new_gs->eid);
		free(new_gs);
	}
}

void routing_table_replace_gs(
	struct ground_station *gs, QueueIdentifier_t router_signaling_queue,
	QueueIdentifier_t bproc_signaling_queue)
{
	struct ground_station_list *entry;

	ASSERT(gs != NULL);
	entry = get_ground_station_entry_by_eid(gs->eid);
	if (entry == NULL) {
		add_new_gs(gs, router_signaling_queue);
	} else {
		drop_contacts(&entry->station->contacts, bproc_signaling_queue);
		remove_gs_from_tables(entry->station);
		free_ground_station(entry->station);
		entry->station = gs;
		if (HAS_FLAG(gs->flags, GS_FLAG_DISCOVERED))
			routing_table_create_contacts(gs, 1);
		add_gs_to_tables(gs);
	}
}

int routing_table_delete_gs_by_eid(
	char *eid, QueueIdentifier_t bproc_signaling_queue)
{
	struct ground_station_list **entry_ptr, *old_gs_entry;

	ASSERT(eid != NULL);
	entry_ptr = get_ground_station_entry_ptr_by_eid(eid);
	if (entry_ptr != NULL) {
		/* Delete whole GS */
		old_gs_entry = *entry_ptr;
		*entry_ptr = old_gs_entry->next;
		drop_contacts(&old_gs_entry->station->contacts,
			bproc_signaling_queue);
		remove_gs_from_tables(old_gs_entry->station);
		free_ground_station(old_gs_entry->station);
		free(old_gs_entry);
		return 1;
	}
	return 0;
}

int routing_table_delete_gs(
	struct ground_station *new_gs, QueueIdentifier_t bproc_signaling_queue)
{
	struct ground_station_list **entry_ptr, *old_gs_entry;
	struct ground_station *cur_gs;
	struct contact_list *modified = NULL, *deleted = NULL, *next, *tmp;

	ASSERT(new_gs != NULL);
	entry_ptr = get_ground_station_entry_ptr_by_eid(new_gs->eid);
	if (entry_ptr != NULL) {
		cur_gs = (*entry_ptr)->station;
		if (new_gs->endpoints == NULL && new_gs->contacts == NULL) {
			/* Delete whole GS */
			old_gs_entry = *entry_ptr;
			*entry_ptr = old_gs_entry->next;
			drop_contacts(&old_gs_entry->station->contacts,
				bproc_signaling_queue);
			remove_gs_from_tables(old_gs_entry->station);
			free_ground_station(old_gs_entry->station);
			free(old_gs_entry);
			free_ground_station(new_gs);
		} else {
			/* Delete contacts/nodes */
			remove_gs_from_tables(cur_gs);
			cur_gs->endpoints = endpoint_list_difference(
				cur_gs->endpoints, new_gs->endpoints, 1);
			cur_gs->contacts = contact_list_difference(
				cur_gs->contacts, new_gs->contacts, 1,
				&modified, &deleted);
			/* Process modified contacts */
			while (modified != NULL) {
				reschedule_bundles(
					modified->data, bproc_signaling_queue);
				next = modified->next;
				free(modified);
				modified = next;
			}
			/* Process deleted contacts */
			while (deleted != NULL) {
				reschedule_bundles(
					deleted->data, bproc_signaling_queue);
				if (deleted->data->active) {
					tmp = deleted;
					deleted = tmp->next;
					free(tmp);
				} else {
					deleted = contact_list_free(deleted);
				}
			}
			add_gs_to_tables(cur_gs);
			eidmanager_free_ref(new_gs->eid);
			free(new_gs);
		}
		return 1;
	}
	free_ground_station(new_gs);
	return 0;
}

static bool add_contact_to_node_in_htab(char *eid, struct contact *c, float p);
static bool remove_contact_from_node_in_htab(char *eid, struct contact *c);
static bool check_for_invalid_overlaps(struct contact *c);

static void add_gs_to_tables(struct ground_station *gs)
{
	struct contact_list *cur_contact;
	struct endpoint_list *cur_persistent_node, *cur_contact_node;

	ASSERT(gs != NULL);
	cur_contact = gs->contacts;
	while (cur_contact != NULL) {
		/* TODO: Try to add contact but reduce timespan */
		if (check_for_invalid_overlaps(cur_contact->data)) {
			cur_contact = cur_contact->next;
			continue;
		}
		add_contact_to_node_in_htab(gs->eid, cur_contact->data, 1.0f);
		cur_persistent_node = gs->endpoints;
		while (cur_persistent_node != NULL) {
			add_contact_to_node_in_htab(
				cur_persistent_node->eid, cur_contact->data,
				cur_persistent_node->p);
			cur_persistent_node = cur_persistent_node->next;
		}
		cur_contact_node = cur_contact->data->contact_endpoints;
		while (cur_contact_node != NULL) {
			add_contact_to_node_in_htab(
				cur_contact_node->eid, cur_contact->data,
				cur_contact_node->p);
			cur_contact_node = cur_contact_node->next;
		}
		add_contact_to_ordered_list(
			&contact_list, cur_contact->data, 1);
		recalculate_contact_capacity(cur_contact->data);
		cur_contact = cur_contact->next;
	}
}

static void remove_gs_from_tables(struct ground_station *gs)
{
	struct contact_list *cur_contact;
	struct endpoint_list *cur_persistent_node, *cur_contact_node;

	ASSERT(gs != NULL);
	cur_contact = gs->contacts;
	while (cur_contact != NULL) {
		remove_contact_from_node_in_htab(gs->eid, cur_contact->data);
		cur_persistent_node = gs->endpoints;
		while (cur_persistent_node != NULL) {
			remove_contact_from_node_in_htab(
				cur_persistent_node->eid, cur_contact->data);
			cur_persistent_node = cur_persistent_node->next;
		}
		cur_contact_node = cur_contact->data->contact_endpoints;
		while (cur_contact_node != NULL) {
			remove_contact_from_node_in_htab(
				cur_contact_node->eid, cur_contact->data);
			cur_contact_node = cur_contact_node->next;
		}
		remove_contact_from_list(&contact_list, cur_contact->data);
		cur_contact = cur_contact->next;
	}
}

static bool add_contact_to_node_in_htab(char *eid, struct contact *c, float p)
{
	struct node_table_entry *entry;

	ASSERT(eid != NULL);
	ASSERT(p > 0.0f && p <= 1.0f);
	ASSERT(c != NULL);
	entry = (struct node_table_entry *)htab_get(&eid_table, eid);
	if (entry == NULL) {
		entry = malloc(sizeof(struct node_table_entry));
		if (entry == NULL)
			return false;
		entry->ref_count = 0;
		entry->contacts = NULL;
		htab_add(&eid_table, eid, entry);
	}
	if (add_contact_to_ordered_assoc_list(&(entry->contacts), c, p, 0)) {
		entry->ref_count++;
		return true;
	}
	return false;
}

static bool remove_contact_from_node_in_htab(char *eid, struct contact *c)
{
	struct node_table_entry *entry;

	ASSERT(eid != NULL);
	ASSERT(c != NULL);
	entry = (struct node_table_entry *)htab_get(&eid_table, eid);
	if (entry == NULL)
		return false;
	if (remove_contact_from_assoc_list(&(entry->contacts), c)) {
		entry->ref_count--;
		if (entry->ref_count <= 0) {
			htab_remove(&eid_table, eid);
			free(entry);
		}
		return true;
	}
	return false;
}

static bool check_for_invalid_overlaps(struct contact *c)
{
	uint8_t overlaps = 0;
	struct contact_list *cur = contact_list;

	while (cur != NULL && cur->data->to < c->from)
		cur = cur->next;
	if (cur == NULL || cur->data->from >= c->to)
		return false;
	do {
		if (cur->data->to > c->from) {
			/*
			 * There cannot be two overlapping contacts with
			 * the same ground station b/c this would use the
			 * same CLA channel.
			 */
			if (cur->data->ground_station == c->ground_station)
				return true;
			overlaps++;
		}
		cur = cur->next;
	} while (cur != NULL && cur->data->from < c->to);
	return (bool)(overlaps >= CLA_CHANNELS);
}

/* CONTACT LIST */

static uint16_t count_unused_contacts(struct ground_station *gs)
{
	uint16_t unused_contacts = 0;
	struct contact_list *list = gs->contacts;

	while (list != NULL) {
		if (list->data->bundle_count == 0)
			unused_contacts++;
		list = list->next;
	}
	return unused_contacts;
}

void routing_table_integrate_inferred_contact(
	struct ground_station *gs, uint8_t initially)
{
	struct rrnd_gs_info *rgs = gs->rrnd_info;
	struct contact *contact = contact_from_rrnd_contact_info(gs,
		&rgs->predictions.last);

	if (!initially)
		add_contact_to_ordered_list(&contact_list, contact, 1);
	add_contact_to_ordered_list(&gs->contacts, contact, 1);
}

void routing_table_create_contacts(struct ground_station *gs, uint8_t initially)
{
	uint16_t length = count_unused_contacts(gs);
	uint64_t time = hal_time_get_timestamp_ms();
	enum rrnd_status status;

	if (length < MINIMUM_GENERATED_CONTACTS && gs->rrnd_info != NULL) {
		length = MINIMUM_GENERATED_CONTACTS - length;
		while (length--) {
			status = rrnd_infer_contact(gs->rrnd_info, time,
				satpos_get, satpos_get_age(time));
			if (HAS_FLAG(status, RRND_STATUS_FAILED))
				break;
			routing_table_integrate_inferred_contact(
				gs, initially);
			hal_io_send_packet(&status, RRND_STATUS_SIZEOF,
				COMM_TYPE_RRND_STATUS);
		}
	}
}

struct contact_list **routing_table_get_raw_contact_list_ptr(void)
{
	return &contact_list;
}

struct ground_station_list *routing_table_get_station_list(void)
{
	return station_list;
}

void routing_table_delete_contact(struct contact *contact)
{
	struct endpoint_list *cur_eid;

	ASSERT(contact != NULL);
	ASSERT(contact->contact_bundles == NULL);
	if (contact->ground_station != NULL) {
		remove_contact_from_node_in_htab(
			contact->ground_station->eid, contact);
		/* Remove contact from GS nodes */
		cur_eid = contact->ground_station->endpoints;
		while (cur_eid != NULL) {
			remove_contact_from_node_in_htab(
				cur_eid->eid, contact);
			cur_eid = cur_eid->next;
		}
		/* Remove from GS list */
		remove_contact_from_list(
			&contact->ground_station->contacts, contact);
	}
	/* Remove contact from contact nodes and free list */
	cur_eid = contact->contact_endpoints;
	while (cur_eid != NULL) {
		remove_contact_from_node_in_htab(
			cur_eid->eid, contact);
		cur_eid = endpoint_list_free(cur_eid);
	}
	contact->contact_endpoints = NULL;
	/* Remove from global list */
	remove_contact_from_list(&contact_list, contact);
	/* Free contact itself */
	free_contact(contact);
}

void routing_table_contact_passed(
	struct contact *contact, QueueIdentifier_t bproc_signaling_queue)
{
	struct routed_bundle_list *tmp;

	if (contact->ground_station != NULL) {
		if (HAS_FLAG(contact->ground_station->flags,
				GS_FLAG_DISCOVERED))
			routing_table_create_contacts(
				contact->ground_station, 0);
		while (contact->contact_bundles != NULL) {
			/* TODO: Transmit struct routed_bundle? */
			bundle_processor_inform(
				bproc_signaling_queue,
				contact->contact_bundles->data->id,
				BP_SIGNAL_RESCHEDULE_BUNDLE,
				BUNDLE_SR_REASON_NO_INFO);
			tmp = contact->contact_bundles->next;
			free(contact->contact_bundles->data);
			free(contact->contact_bundles);
			contact->contact_bundles = tmp;
		}
	}
	routing_table_delete_contact(contact);
}

/* RE-SCHEDULING */

static void drop_contacts(
	struct contact_list **gs_clist, QueueIdentifier_t bproc_signaling_queue)
{
	struct contact_list *tmp;

	while (*gs_clist != NULL) {
		reschedule_bundles((*gs_clist)->data, bproc_signaling_queue);
		if ((*gs_clist)->data->active) {
			/* Un-associate contact */
			tmp = *gs_clist;
			/* Add GS nodes to contact */
			tmp->data->contact_endpoints = endpoint_list_union(
				tmp->data->contact_endpoints,
				tmp->data->ground_station->endpoints, 1);
			tmp->data->ground_station = NULL;
			*gs_clist = tmp->next;
			free(tmp);
		} else {
			gs_clist = &(*gs_clist)->next;
		}
	}
}

static void reschedule_bundles(
	struct contact *contact, QueueIdentifier_t bproc_signaling_queue)
{
	struct routed_bundle *rb;
	struct fragment_route fr;
	bundleid_t id;
	uint8_t c;

	ASSERT(contact != NULL);
	/* Empty the bundle list and queue them in for re-scheduling */
	while (contact->contact_bundles != NULL) {
		rb = contact->contact_bundles->data;
		ASSERT(rb->contact_count <= ROUTER_MAX_CONTACTS);
		for (c = 0; c < rb->contact_count; c++)
			fr.contacts[c] = rb->contacts[c];
		fr.contact_count = rb->contact_count;
		fr.payload_size = rb->size;
		id = rb->id;
		router_remove_bundle_from_route(&fr, id, 1);
		bundle_processor_inform(
			bproc_signaling_queue, id,
			BP_SIGNAL_RESCHEDULE_BUNDLE,
			BUNDLE_SR_REASON_NO_INFO
		);
	}
}

/* DEBUG INFO */

#ifdef VERBOSE

void routing_table_print_debug_info(void)
{
	struct ground_station_list *cur;
	struct endpoint_list *curep, *curcep;
	struct contact_list *curct, *curctl;
	struct routed_bundle_list *curcbd;
	struct bundle *bdl;
	char out1[32], out2[32];

	hal_io_lock_com_semaphore();
	hal_io_write_string("\n>>>>>>>>>> BEGIN RT INFO\n");
	hal_io_write_string("\nGround station list\n-------------------\n");
	cur = station_list;
	while (cur != NULL) {
		hal_debug_printf("GS: %p => EID: %s, CL ADDR: %s\n",
				 (void *)cur->station,
			cur->station->eid, cur->station->cla_addr
				? cur->station->cla_addr
				: "<unknown>");
		curep = cur->station->endpoints;
		while (curep != NULL) {
			hal_debug_printf("> EPNT: %s\n", curep->eid);
			curep = curep->next;
		}
		curct = cur->station->contacts;
		while (curct != NULL) {
			hal_platform_sprintu64(out1, curct->data->from);
			hal_platform_sprintu64(out2, curct->data->to);
			hal_debug_printf("> CONT: %p => { %s, %s, %d, %d }\n",
				(void *)curct->data, out1, out2,
				(int)curct->data->bitrate,
				(int)curct->data->remaining_capacity_p0);
			curcep = curct->data->contact_endpoints;
			while (curcep != NULL) {
				hal_debug_printf("  +-> EPNT: %s\n",
						 curcep->eid);
				curcep = curcep->next;
			}
			curcbd = curct->data->contact_bundles;
			while (curcbd != NULL) {
				bdl = bundle_storage_get(curcbd->data->id);
				hal_platform_sprintu64(out1,
						       bdl->creation_timestamp);
				hal_debug_printf("  +-> BDLE: %p => { "
					"time: %s, to: %s, pay_l: %d... }\n",
					(void *)bdl, out1, bdl->destination,
					(int)bdl->payload_block->length);
				curcbd = curcbd->next;
			}
			curct = curct->next;
		}
		cur = cur->next;
	}
	hal_io_write_string("\nContact list\n------------\n");
	curctl = contact_list;
	while (curctl != NULL) {
		hal_platform_sprintu64(out1, curctl->data->from);
		hal_platform_sprintu64(out2, curctl->data->to);
		hal_debug_printf("> CONT: %p => { %s, %s, %d, %d }\n",
			(void *)curctl->data, out1, out2,
			(int)curctl->data->bitrate,
			(int)curctl->data->remaining_capacity_p0);
		curctl = curctl->next;
	}
	hal_io_write_string("\n<<<<<<<<<< END RT INFO\n\n");
	hal_io_unlock_com_semaphore();
}

#endif
