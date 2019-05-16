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

/* => 477 Bytes */
#define BUNDLE_PAYLOAD_MIN 430
#define BUNDLE_PAYLOAD_MAX 430
#define BUNDLE_COUNT 4

#define TEST_BEGIN 30000
#define CONTACT_BEGIN 30002
#define CONTACT_DURATION 1
#define CONTACT_SPACING 5
#define BANDWIDTH1 9000
#define BANDWIDTH2 450

#define GS1 "upcn:gs.1"
#define GS2 "upcn:gs.2"
#define EP2 "upcn:ep"

static struct bundle *bdl[BUNDLE_COUNT];
static struct bundle *bdl_rec[BUNDLE_COUNT];

void bft_init(void)
{
	int i;

	for (i = 0; i < BUNDLE_COUNT; i++) {
#if (BUNDLE_PAYLOAD_MAX != BUNDLE_PAYLOAD_MIN)
		bdl[i] = upcntest_create_bundle6(BUNDLE_PAYLOAD_MIN
			+ (rand() % (BUNDLE_PAYLOAD_MAX - BUNDLE_PAYLOAD_MIN)),
			EP2, 1, 0);
#else
		bdl[i] = upcntest_create_bundle6(BUNDLE_PAYLOAD_MIN,
			EP2, 1, 0);
#endif
		bdl_rec[i] = NULL;
	}
}

void bft_cleanup(void)
{
	int i;

	for (i = 0; i < BUNDLE_COUNT; i++) {
		bundle_free(bdl[i]);
		if (bdl_rec[i] != NULL)
			bundle_free(bdl_rec[i]);
	}
}

void bft_run(void)
{
	uint64_t next = CONTACT_BEGIN;
	int i;

	upcntest_send_query();
	upcntest_send_settime(TEST_BEGIN);
	resettime(TEST_BEGIN);
	printf("Configuring system...\n");
	upcntest_send_mkgs(GS1, "AAAAA", 1);
	upcntest_send_mkgs(GS2, "BBBBB", 1);
	upcntest_send_mkep(GS2, EP2);
	upcntest_send_mkct(GS1, CONTACT_BEGIN,
		CONTACT_BEGIN + CONTACT_DURATION, BANDWIDTH1);
	next += CONTACT_DURATION + CONTACT_SPACING;
	upcntest_send_mkct(GS2, next,
		next + CONTACT_DURATION, BANDWIDTH2);
	next += CONTACT_DURATION + CONTACT_SPACING;
	upcntest_send_mkct(GS2, next,
		next + CONTACT_DURATION, BANDWIDTH2);
	next += CONTACT_DURATION + CONTACT_SPACING;
	upcntest_send_mkct(GS2, next,
		next + CONTACT_DURATION, BANDWIDTH2);
	next += CONTACT_DURATION + CONTACT_SPACING;
	upcntest_send_mkct(GS2, next,
		next + CONTACT_DURATION, BANDWIDTH2);
	next += CONTACT_DURATION + CONTACT_SPACING;
	upcntest_send_mkct(GS2, next,
		next + CONTACT_DURATION, BANDWIDTH2);
	next += CONTACT_DURATION + CONTACT_SPACING;
	upcntest_send_mkct(GS2, next,
		next + CONTACT_DURATION, BANDWIDTH2);
	wait_until(CONTACT_BEGIN);
	upcntest_send_resetstats();
	for (i = 0; i < BUNDLE_COUNT; i++)
		upcntest_send_bundle(bdl[i]);
	wait_until(next);
	/* TODO: Receive and check */
	upcntest_send_storetrace();
	upcntest_send_query();
}
