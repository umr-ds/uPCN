/* System includes */
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

/* uPCN includes */
#include <upcn/upcn.h>
#include <upcn/bundle.h>
#include "upcn/routerTask.h" // enum router_command_type

/* Test includes */
#include <upcnTest.h>
#include <testlib.h>

#define BUNDLE_PAYLOAD_MIN 50
#define BUNDLE_PAYLOAD_MAX 200
#define BUNDLE_COUNT 4

#define GS1 "upcn:gs.1"
#define GS2 "upcn:gs.2"
#define EP1 "upcn:ep1"
#define EP2 "upcn:ep2"
#define EP3 "upcn:ep3"
#define EP4 "upcn:ep4"
#define DST "upcn:dst"

static struct bundle *bdl[BUNDLE_COUNT];

void sbt_init(void)
{
	int i;

	for (i = 0; i < BUNDLE_COUNT; i++) {
		bdl[i] = upcntest_create_bundle6(BUNDLE_PAYLOAD_MIN
			+ (rand() % (BUNDLE_PAYLOAD_MAX - BUNDLE_PAYLOAD_MIN)),
			DST, 0, rand() % 2);
	}
}

void sbt_cleanup(void)
{
	int i;

	for (i = 0; i < BUNDLE_COUNT; i++)
		bundle_free(bdl[i]);
}

void sbt_run(void)
{
	int i;

	upcntest_send_query();
	upcntest_send_resetstats();
	upcntest_send_settime(0);
	resettime(0);

	printf("Adding ground stations...\n");
	/* GS 1: Test default nodes, fragmentation */
	upcntest_begin_send(INPUT_TYPE_ROUTER_COMMAND_DATA);
	upcntest_send_command(ROUTER_COMMAND_ADD,
		"(%s),%d:(%s):[(%s),(%s)]",
		GS1, 1000, "QSPN", EP1, DST);
	upcntest_finish_send();
	/* Add something to existing GS */
	upcntest_begin_send(INPUT_TYPE_ROUTER_COMMAND_DATA);
	upcntest_send_command(ROUTER_COMMAND_ADD,
		"(%s):::[{3,5,400},{4,5,800,[(%s),(%s)]}]",
		GS1, EP1, EP3, EP4);
	upcntest_finish_send();
	/* GS 2: Test replace option, delete parts */
	upcntest_begin_send(INPUT_TYPE_ROUTER_COMMAND_DATA);
	upcntest_send_command(ROUTER_COMMAND_ADD,
		"(%s):(%s):[(%s),(%s)]:[{1,2,800,[(%s)]}]",
		GS2, "XZUS", EP3, EP4, EP2);
	upcntest_finish_send();
	/* Remove node.4 and cnode.3 (from contact) */
	upcntest_begin_send(INPUT_TYPE_ROUTER_COMMAND_DATA);
	upcntest_send_command(ROUTER_COMMAND_DELETE,
		"(%s)::[(%s)]:[{1,2,100,[(%s)]}]",
		GS2, EP1, EP2);
	upcntest_finish_send();
	/* Replace GS completely */
	upcntest_begin_send(INPUT_TYPE_ROUTER_COMMAND_DATA);
	upcntest_send_command(ROUTER_COMMAND_UPDATE,
		"(%s):(%s):[(%s)]:[{2,3,300,[(%s),(%s)]},{100,200,50,[]}]",
		GS2, "JOXZ", EP4, DST, EP1);
	upcntest_finish_send();
	/* Update contact bandwidth */
	upcntest_begin_send(INPUT_TYPE_ROUTER_COMMAND_DATA);
	upcntest_send_command(ROUTER_COMMAND_ADD, "(%s):::[{2,3,400}]", GS2);
	upcntest_finish_send();
	/* Wait a bit */
	wait_until(1);
	printf("Sending bundles...\n");
	/* Send bundles */
	for (i = 0; i < BUNDLE_COUNT; i++)
		upcntest_send_bundle(bdl[i]);
	/* Wait till end */
	wait_until(5);
	printf("Deleting stuff...\n");
	/* Delete 2 nodes from GS 1 */
	upcntest_begin_send(INPUT_TYPE_ROUTER_COMMAND_DATA);
	upcntest_send_command(ROUTER_COMMAND_DELETE,
		"(%s)::[(%s),(%s)]",
		GS1, EP1, DST);
	upcntest_finish_send();
	/* Delete the last normal node from GS 2, try delete contacts */
	upcntest_begin_send(INPUT_TYPE_ROUTER_COMMAND_DATA);
	upcntest_send_command(ROUTER_COMMAND_DELETE,
		"(%s)::[(%s),(%s)]:[{100,200,100},{900,905,77,[]}]",
		GS2, EP4, DST);
	upcntest_finish_send();
	wait_until(6);
	printf("Deleting GS...\n");
	upcntest_send_rmgs(GS1);
	upcntest_send_rmgs(GS2);

	upcntest_send_storetrace();
	upcntest_send_query();
}
