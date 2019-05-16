#ifndef ROUTINGTABLE_H_INCLUDED
#define ROUTINGTABLE_H_INCLUDED

#include "upcn/upcn.h"
#include "upcn/groundStation.h"

struct node_table_entry {
	uint16_t ref_count;
	struct associated_contact_list *contacts;
};

enum upcn_result routing_table_init(void);
void routing_table_free(void);

struct ground_station *routing_table_lookup_ground_station(const char *eid);
struct node_table_entry *routing_table_lookup_eid(const char *eid);
uint8_t routing_table_lookup_eid_in_nbf(
	char *eid, struct ground_station **target, uint8_t max);
uint8_t routing_table_lookup_hot_gs(
	struct ground_station **target, uint8_t max);

void routing_table_add_gs(
	struct ground_station *new_gs, QueueIdentifier_t router_signaling_queue,
	QueueIdentifier_t bproc_signaling_queue);
void routing_table_replace_gs(
	struct ground_station *gs, QueueIdentifier_t router_signaling_queue,
	QueueIdentifier_t bproc_signaling_queue);
int routing_table_delete_gs(
	struct ground_station *new_gs, QueueIdentifier_t bproc_signaling_queue);
int routing_table_delete_gs_by_eid(
	char *eid, QueueIdentifier_t bproc_signaling_queue);

void routing_table_integrate_inferred_contact(
	struct ground_station *gs, uint8_t initially);
void routing_table_create_contacts(
	struct ground_station *gs, uint8_t initially);
struct contact_list **routing_table_get_raw_contact_list_ptr(void);
struct ground_station_list *routing_table_get_station_list(void);
void routing_table_delete_contact(struct contact *contact);
void routing_table_contact_passed(
	struct contact *contact, QueueIdentifier_t bproc_signaling_queue);

#ifdef INCLUDE_ADVANCED_COMM
void routing_table_print_debug_info(void);
#else /* INCLUDE_ADVANCED_COMM */
#define routing_table_print_debug_info()
#endif /* INCLUDE_ADVANCED_COMM */

#endif /* ROUTINGTABLE_H_INCLUDED */
