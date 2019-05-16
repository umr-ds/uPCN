#include <stdlib.h>
#include <string.h>
#include <unity_fixture.h>
#include "upcn/upcn.h"
#include "upcn/groundStation.h"
#include "upcn/routingTable.h"
#include "upcn/bundleProcessor.h"
#include "drv/llsort.h"
#include "upcn/eidManager.h"
#include "cla_defines.h"

#if CLA_CHANNELS > 1
#warning These tests assume only one cla channel. They will fail for other CLAs.
#endif

TEST_GROUP(routingTable);

static struct contact *createct(struct ground_station *gs,
	uint64_t from, uint64_t to, uint16_t bitrate)
{
	struct contact *c = contact_create(gs);

	c->from = from;
	c->to = to;
	c->bitrate = bitrate;
	recalculate_contact_capacity(c);
	return c;
}

static void addnode(struct endpoint_list **list, char *node)
{
	struct endpoint_list *l;

	l = malloc(sizeof(struct endpoint_list));
	l->eid = eidmanager_alloc_ref(node, 0);
	l->p = 1.0f;
	l->next = *list;
	*list = l;
}

static struct ground_station *gs11, *gs12, *gs13, *gs2, *gs3;
static struct contact *c1, *c2, *c3, *c4, *c5, *c6, *c7,
	*c8, *c9, *c10, *c11, *c12;
static QueueIdentifier_t sig_queue;

TEST_SETUP(routingTable)
{
	/* gs1-1 */
	gs11 = ground_station_create("gs1");
	addnode(&gs11->endpoints, "node1");
	c1 = createct(gs11, 1, 2, 200);
	addnode(&c1->contact_endpoints, "node2");
	c2 = createct(gs11, 2, 3, 300);
	addnode(&c2->contact_endpoints, "node4");
	c3 = createct(gs11, 4, 5, 400);
	add_contact_to_ordered_list(&gs11->contacts, c1, 1);
	add_contact_to_ordered_list(&gs11->contacts, c2, 1);
	add_contact_to_ordered_list(&gs11->contacts, c3, 1);
	/* gs1-2 */
	gs12 = ground_station_create("gs1");
	addnode(&gs12->endpoints, "node3");
	c4 = createct(gs12, 5, 6, 500);
	c5 = createct(gs12, 2, 3, 600);
	c6 = createct(gs12, 4, 6, 700);
	add_contact_to_ordered_list(&gs12->contacts, c4, 1);
	add_contact_to_ordered_list(&gs12->contacts, c5, 1);
	add_contact_to_ordered_list(&gs12->contacts, c6, 1);
	/* gs1-3 */
	gs13 = ground_station_create("gs1");
	addnode(&gs13->endpoints, "node3");
	c9 = createct(gs13, 1, 2, 200);
	c10 = createct(gs13, 2, 3, 300);
	c11 = createct(gs13, 5, 6, 500); // invalid contact (overlapping)
	c12 = createct(gs13, 4, 6, 700);
	add_contact_to_ordered_list(&gs13->contacts, c9, 0);
	add_contact_to_ordered_list(&gs13->contacts, c10, 0);
	add_contact_to_ordered_list(&gs13->contacts, c11, 0);
	add_contact_to_ordered_list(&gs13->contacts, c12, 0);
	/* gs2 */
	gs2 = ground_station_create("gs2");
	addnode(&gs2->endpoints, "node3");
	c7 = createct(gs2, 8, 9, 800);
	c8 = createct(gs2, 6, 8, 900);
	addnode(&c8->contact_endpoints, "node5");
	add_contact_to_ordered_list(&gs2->contacts, c7, 1);
	add_contact_to_ordered_list(&gs2->contacts, c8, 1);
	/* gs3 */
	gs3 = ground_station_create("gs3");
	addnode(&gs3->endpoints, "node6");
	/* queue */
	sig_queue = hal_queue_create(60, 6);
	/* init rt */
	routing_table_init();
	/* clock */
	hal_time_init(0);
}

TEST_TEAR_DOWN(routingTable)
{
	routing_table_free();
	hal_queue_delete(sig_queue);
}

TEST(routingTable, routing_table_add_delete)
{
	struct node_table_entry *nti;

	routing_table_add_gs(gs11, sig_queue, sig_queue);
	routing_table_add_gs(gs2, sig_queue, sig_queue);
	routing_table_add_gs(gs12, sig_queue, sig_queue);
	routing_table_add_gs(gs3, sig_queue, sig_queue);
	TEST_ASSERT_EQUAL_PTR(gs11, routing_table_lookup_ground_station("gs1"));
	TEST_ASSERT_EQUAL_PTR(gs2, routing_table_lookup_ground_station("gs2"));
	TEST_ASSERT_EQUAL_PTR(gs3, routing_table_lookup_ground_station("gs3"));
	/* check lookup */
	nti = routing_table_lookup_eid("node2");
	TEST_ASSERT_NOT_NULL(nti);
	TEST_ASSERT_EQUAL_UINT16(1, nti->ref_count);
	TEST_ASSERT_NOT_NULL(nti->contacts);
	TEST_ASSERT_NULL(nti->contacts->next);
	TEST_ASSERT_NOT_NULL(nti->contacts->data);
	TEST_ASSERT_EQUAL_PTR(c1, nti->contacts->data);
	/* check updated bitrate (merging of c1-1, c1-2) */
	nti = routing_table_lookup_eid("node4");
	TEST_ASSERT_NOT_NULL(nti);
	TEST_ASSERT_EQUAL_UINT16(1, nti->ref_count);
	TEST_ASSERT_NOT_NULL(nti->contacts);
	TEST_ASSERT_NULL(nti->contacts->next);
	TEST_ASSERT_NOT_NULL(nti->contacts->data);
	TEST_ASSERT_EQUAL_PTR(c2, nti->contacts->data);
	TEST_ASSERT_EQUAL_UINT16(600, nti->contacts->data->bitrate);
	/* check some other nodes */
	nti = routing_table_lookup_eid("node5");
	TEST_ASSERT_NOT_NULL(nti);
	TEST_ASSERT_EQUAL_UINT16(1, nti->ref_count);
	TEST_ASSERT_NOT_NULL(nti->contacts);
	TEST_ASSERT_NULL(nti->contacts->next);
	TEST_ASSERT_NOT_NULL(nti->contacts->data);
	TEST_ASSERT_EQUAL_PTR(c8, nti->contacts->data);
	nti = routing_table_lookup_eid("node6");
	TEST_ASSERT_NULL(nti);
	/* delete */
	LLSORT(struct contact_list, data->from, gs13->contacts);
	TEST_ASSERT_EQUAL_INT(1, routing_table_delete_gs(gs13, sig_queue));
	nti = routing_table_lookup_eid("node1");
	TEST_ASSERT_NOT_NULL(nti);
	TEST_ASSERT_EQUAL_UINT16(1, nti->ref_count);
	TEST_ASSERT_NOT_NULL(nti->contacts);
	TEST_ASSERT_NULL(nti->contacts->next);
	TEST_ASSERT_NOT_NULL(nti->contacts->data);
	TEST_ASSERT_EQUAL_PTR(c3, nti->contacts->data);
	nti = routing_table_lookup_eid("node2");
	TEST_ASSERT_NULL(nti);
	nti = routing_table_lookup_eid("node4");
	TEST_ASSERT_NULL(nti);
	/* should be reachable only via gs2 */
	nti = routing_table_lookup_eid("node3");
	TEST_ASSERT_NOT_NULL(nti);
	TEST_ASSERT_EQUAL_UINT16(2, nti->ref_count);
	TEST_ASSERT_NOT_NULL(nti->contacts);
	TEST_ASSERT_NOT_NULL(nti->contacts->next);
	TEST_ASSERT_NULL(nti->contacts->next->next);
	TEST_ASSERT_NOT_NULL(nti->contacts->data);
	TEST_ASSERT_NOT_NULL(nti->contacts->next->data);
	TEST_ASSERT_EQUAL_PTR(c8, nti->contacts->data);
	TEST_ASSERT_EQUAL_PTR(c7, nti->contacts->next->data);
	/* remove contact */
	routing_table_delete_contact(c8);
	nti = routing_table_lookup_eid("node3");
	TEST_ASSERT_NOT_NULL(nti);
	TEST_ASSERT_EQUAL_UINT16(1, nti->ref_count);
	TEST_ASSERT_NOT_NULL(nti->contacts);
	TEST_ASSERT_NULL(nti->contacts->next);
	TEST_ASSERT_NOT_NULL(nti->contacts->data);
	TEST_ASSERT_EQUAL_PTR(c7, nti->contacts->data);
	TEST_ASSERT_EQUAL_INT(1, routing_table_delete_gs(
		ground_station_create("gs1"), sig_queue));
	TEST_ASSERT_EQUAL_INT(1, routing_table_delete_gs(
		ground_station_create("gs2"), sig_queue));
	TEST_ASSERT_EQUAL_INT(1, routing_table_delete_gs(
		ground_station_create("gs3"), sig_queue));
}

TEST(routingTable, routing_table_replace)
{
	struct node_table_entry *nti;

	LLSORT(struct contact_list, data->from, gs13->contacts);
	routing_table_add_gs(gs13, sig_queue, sig_queue);
	nti = routing_table_lookup_eid("node3");
	TEST_ASSERT_EQUAL_UINT16(3, nti->ref_count);
	TEST_ASSERT_NOT_NULL(nti->contacts);
	TEST_ASSERT_EQUAL_PTR(c12, nti->contacts->next->next->data);
	routing_table_replace_gs(gs11, sig_queue, sig_queue);
	nti = routing_table_lookup_eid("node3");
	TEST_ASSERT_NULL(nti);
	TEST_ASSERT_NULL(routing_table_lookup_ground_station("gs2"));
	TEST_ASSERT_EQUAL_INT(1, routing_table_delete_gs(
		ground_station_create("gs1"), sig_queue));
	free_ground_station(gs12);
	free_ground_station(gs2);
	free_ground_station(gs3);
}

TEST_GROUP_RUNNER(routingTable)
{
	RUN_TEST_CASE(routingTable, routing_table_add_delete);
	RUN_TEST_CASE(routingTable, routing_table_replace);
}
