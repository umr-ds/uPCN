#ifndef ROUTINGTABLE_H_INCLUDED
#define ROUTINGTABLE_H_INCLUDED

#include "upcn/node.h"
#include "upcn/result.h"

#include "platform/hal_types.h"

#include <stddef.h>
#include <stdint.h>

struct node_table_entry {
	uint16_t ref_count;
	struct associated_contact_list *contacts;
};

enum upcn_result routing_table_init(void);
void routing_table_free(void);

struct node *routing_table_lookup_node(const char *eid);
struct node_table_entry *routing_table_lookup_eid(const char *eid);
uint8_t routing_table_lookup_eid_in_nbf(
	char *eid, struct node **target, uint8_t max);
uint8_t routing_table_lookup_hot_node(
	struct node **target, uint8_t max);

void routing_table_add_node(
	struct node *new_node, QueueIdentifier_t bproc_signaling_queue);
void routing_table_replace_node(
	struct node *node, QueueIdentifier_t bproc_signaling_queue);
int routing_table_delete_node(
	struct node *new_node, QueueIdentifier_t bproc_signaling_queue);
int routing_table_delete_node_by_eid(
	char *eid, QueueIdentifier_t bproc_signaling_queue);

struct contact_list **routing_table_get_raw_contact_list_ptr(void);
struct node_list *routing_table_get_node_list(void);
void routing_table_delete_contact(struct contact *contact);
void routing_table_contact_passed(
	struct contact *contact, QueueIdentifier_t bproc_signaling_queue);

#endif /* ROUTINGTABLE_H_INCLUDED */
