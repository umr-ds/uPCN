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

#define BUNDLE_PAYLOAD_MIN 5
#define BUNDLE_PAYLOAD_MAX 10
#define BUNDLE_COUNT 100

/* If not defined, the optimizer can not run but the spacing is lower */
#define ALLOW_SORT

#define TEST_BEGIN 20000
#define CONTACT_BEGIN 20002
#define CONTACT_DURATION 3
#define BANDWIDTH 90000

#ifdef ALLOW_SORT
#define CONTACT_SPACING 4
#else /* ALLOW_SORT */
#define CONTACT_SPACING 1
#endif /* ALLOW_SORT */

#define GS1 "upcn:gs.1"
#define GS2 "upcn:gs.2"
#define EP2 "e:p"

static struct bundle *bdl[BUNDLE_COUNT];
static struct bundle *bdl_rec[BUNDLE_COUNT];

void blt_init(void)
{
	int i;

	for (i = 0; i < BUNDLE_COUNT; i++) {
		bdl[i] = upcntest_create_bundle6(BUNDLE_PAYLOAD_MIN
			+ (rand() % (BUNDLE_PAYLOAD_MAX - BUNDLE_PAYLOAD_MIN)),
			EP2, 0, rand() % 2);
		bdl_rec[i] = NULL;
	}
}

void blt_cleanup(void)
{
	int i;

	for (i = 0; i < BUNDLE_COUNT; i++) {
		bundle_free(bdl[i]);
		if (bdl_rec[i] != NULL)
			bundle_free(bdl_rec[i]);
	}
}

#ifdef ALLOW_SORT
static int bprio(struct bundle *b)
{
	return (b->proc_flags & BUNDLE_FLAG_EXPEDITED_PRIORITY) != 0;
}
#endif /* ALLOW_SORT */

static int checkbdl(void)
{
	int i, c, eq, ok = 1;
#ifdef ALLOW_SORT
	int p, prio = 2;
#endif /* ALLOW_SORT */

	for (i = 0; i < BUNDLE_COUNT; i++) {
		if (bdl_rec[i] == NULL) {
			printf("Bundles >= %d not received\n", i);
			ok = 0;
			break;
		}
#ifdef ALLOW_SORT
		p = bprio(bdl_rec[i]);
		if (p < prio)
			prio = p;
		else if (p > prio)
			printf("Prio increased on bundle %d\n", i);
#endif /* ALLOW_SORT */
		eq = 0;
		c = 0;
		while (c < BUNDLE_COUNT) {
			if (bundlecmp(bdl_rec[i], bdl[c])) {
				eq = 1;
				break;
			}
			c++;
		}
		ok &= eq;
		if (eq)
			printf("Bundle %d equals %d\n", i, c);
		else
			printf("No equivalent for bundle %d\n", i);
	}
	return ok;
}

void blt_run(void)
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
	for (i = 0; i < BUNDLE_COUNT; i++)
		upcntest_send_bundle(bdl[i]);
	/*upcntest_send_query();*/
	wait_until(c2);
	for (i = 0; i < BUNDLE_COUNT; i++) {
		printf("Waiting for bundle %d... ", i + 1);
		bdl_rec[i] = upcntest_receive_next_bundle(500);
		printf("%s\n", bdl_rec[i] != NULL ? "OK" : "FAIL");
	}
	wait_until(c2 + CONTACT_DURATION);
	upcntest_send_storetrace();
	upcntest_send_query();
	ASSERT(checkbdl());
	wait_until(c2 + CONTACT_DURATION + 1);
	printf("Removing GS...\n");
	upcntest_send_rmgs(GS1);
	upcntest_send_rmgs(GS2);
}
