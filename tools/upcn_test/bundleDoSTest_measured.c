/* System includes */
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

/* uPCN includes */
#include <upcn/upcn.h>
#include <upcn/bundle.h>

/* Test includes */
#include <upcnTest.h>
#include <testlib.h>

#define BUNDLE_PAYLOAD 50
#define BUNDLE_COUNT 600

#define TEST_BEGIN 90000
#define CONTACT_BEGIN 90003
#define CONTACT_DURATION 10
#define CONTACT_SPACING 1000
#define BANDWIDTH 6000000

#define GS1 "upcn:gs.1"
#define GS2 "upcn:gs.2"
#define EP2 "upcn:ep"

static struct bundle *bdl[BUNDLE_COUNT];

void bdt2_init(void)
{
	int i;

	for (i = 0; i < BUNDLE_COUNT; i++) {
		bdl[i] = upcntest_create_bundle6(BUNDLE_PAYLOAD,
			EP2, 0, rand() % 2);
	}
}

void bdt2_cleanup(void)
{
	int i;

	for (i = 0; i < BUNDLE_COUNT; i++)
		bundle_free(bdl[i]);
}

void bdt2_run(void)
{
	const uint64_t c2 = CONTACT_BEGIN + CONTACT_DURATION + CONTACT_SPACING;
	int i;

	upcntest_send_query();
	upcntest_send_settime(TEST_BEGIN);
	resettime(TEST_BEGIN);
	printf("Configuring system...\n");
	upcntest_send_mkgs(GS1, "AAAAA", 1);
	upcntest_send_mkgs(GS2, "BBBBB", 1);
	upcntest_send_mkep(GS2, EP2);
	upcntest_send_mkct(GS1, CONTACT_BEGIN,
		CONTACT_BEGIN + CONTACT_DURATION, BANDWIDTH);
	upcntest_send_mkct(GS2, c2,
		c2 + CONTACT_DURATION, BANDWIDTH);
	wait_until(CONTACT_BEGIN);
	upcntest_send_resetstats();
	printf("Sending %d bundles...\n", BUNDLE_COUNT);
	for (i = 0; i < BUNDLE_COUNT; i++) {
		if (testtime() >= (CONTACT_BEGIN + CONTACT_DURATION))
			break;
		upcntest_send_bundle(bdl[i]);
	}
	upcntest_send_storetrace();
	printf("Sent %d bundles\n", i);
	upcntest_send_router_query();
	upcntest_send_rmgs(GS1);
	upcntest_send_rmgs(GS2);
	upcntest_send_query();
}
