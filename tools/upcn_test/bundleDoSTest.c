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

#define BUNDLE_PAYLOAD_MIN 2
#define BUNDLE_PAYLOAD_MAX 20000
#define BUNDLE_COUNT 500

#define TEST_BEGIN 60000
#define CONTACT_BEGIN 60003
#define CONTACT_DURATION 30
#define CONTACT_SPACING 1000
#define BANDWIDTH 6000000
#define MARGIN 3

#define GS1 "upcn:gs.1"
#define GS2 "upcn:gs.2"
#define EP2 "upcn:ep"

static struct bundle *bdl[BUNDLE_COUNT];

void bdt_init(void)
{
	int i;

	for (i = 0; i < BUNDLE_COUNT; i++) {
		bdl[i] = upcntest_create_bundle6(BUNDLE_PAYLOAD_MIN
			+ (rand() % (BUNDLE_PAYLOAD_MAX - BUNDLE_PAYLOAD_MIN)),
			EP2, 0, rand() % 2);
	}
}

void bdt_cleanup(void)
{
	int i;

	for (i = 0; i < BUNDLE_COUNT; i++)
		bundle_free(bdl[i]);
}

void bdt_run(void)
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
	printf("Sending MANY bundles...\n");
	for (i = 0; i < BUNDLE_COUNT; i++) {
		if (testtime() >= (CONTACT_BEGIN + CONTACT_DURATION - MARGIN))
			break;
		upcntest_send_bundle(bdl[i]);
	}
	printf("Sent %d bundles\n", i);
	wait_for(MARGIN * 1000);
	upcntest_send_rmgs(GS2);
	upcntest_send_storetrace();
	upcntest_send_router_query();
	upcntest_send_query();
}
