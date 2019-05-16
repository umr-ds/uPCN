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

#define BUNDLE_SIZE 500
#define BUNDLE_DEST "dtn:bundle.receiver"

#define TEST_BEGIN 10000
#define CONTACT_BEGIN 10003
#define CONTACT_DURATION 2
#define CONTACT_SPACING 7
#define CONTACT_PERIOD 20
#define BANDWIDTH 1200 /* 9600 Baud */

#define TEST_ITERATIONS 1
#define ITER_CONTACTS 9

#define EP_1 "upcn:ahn.1.ep"
#define EP_2 "upcn:ahn.2.ep"
#define EP_3 "upcn:web.ep"

#define CONTACT_COUNT 9
#define CONTACT_MAX_BDL 7

static char *bdldests[3] = {
	EP_1,
	EP_2,
	EP_3
};

static int out_bdl_c;
static struct bundle *out_bdl[CONTACT_COUNT * CONTACT_MAX_BDL];

static int contact_bdl_c[CONTACT_COUNT];
static struct bundle *contact_bdl[CONTACT_COUNT][CONTACT_MAX_BDL];

void rt_init(void)
{
}

void rt_cleanup(void)
{
	int i, c;

	for (i = 0; i < out_bdl_c; i++) {
		if (out_bdl[i] != NULL) {
			bundle_free(out_bdl[i]);
			out_bdl[i] = NULL;
		}
	}
	out_bdl_c = 0;
	for (i = 0; i < CONTACT_COUNT; i++) {
		for (c = 0; c < contact_bdl_c[i]; c++) {
			if (contact_bdl[i][c] != NULL) {
				bundle_free(contact_bdl[i][c]);
				contact_bdl[i][c] = NULL;
			}
		}
		contact_bdl_c[i] = 0;
	}
}

static uint64_t contacttime(int contact)
{
	int orbit = contact / 3;
	uint64_t orbit_begin = CONTACT_BEGIN + CONTACT_PERIOD * orbit;

	return orbit_begin + (contact % 3) * CONTACT_SPACING;
}

static void add_orbit(int num)
{
	int c = num * 3;

	upcntest_send_mkct("upcn:gs.1", contacttime(c + 0),
		contacttime(c + 0) + CONTACT_DURATION, BANDWIDTH);
	upcntest_send_mkct("upcn:gs.2", contacttime(c + 1),
		contacttime(c + 1) + CONTACT_DURATION, BANDWIDTH);
	upcntest_send_mkct("upcn:gs.3", contacttime(c + 2),
		contacttime(c + 2) + CONTACT_DURATION, BANDWIDTH);
}

static void send_with_prio(int dest, int high)
{
	size_t s = 0;
	uint8_t *bin;
	struct bundle *b;

	b = upcntest_create_bundle6(BUNDLE_SIZE, bdldests[dest], 1, 0);
	b->proc_flags &= ~(BUNDLE_FLAG_NORMAL_PRIORITY
		| BUNDLE_FLAG_EXPEDITED_PRIORITY);
	b->proc_flags |= (high ? BUNDLE_FLAG_EXPEDITED_PRIORITY
		: BUNDLE_FLAG_NORMAL_PRIORITY);
	// Reduce payload length to fit bundle into approx. BUNDLE_SIZE
	bundle_recalculate_header_length(b);
	b->payload_block->length -= (
		bundle_get_serialized_size(b) - BUNDLE_SIZE
	);
	out_bdl[out_bdl_c++] = b;
	bin = upcntest_bundle_to_bin(b, &s, 0);
	upcntest_begin_send(INPUT_TYPE_BUNDLE_VERSION);
	upcntest_send_binary(bin, s);
	upcntest_finish_send();
}

/* TO 1 LO | TO 1 HI | ... | TO 3 HI | FROM LO | FROM HI */
static const int contact_mapping[CONTACT_COUNT][8] = {
	{0, 0, 0, 0, 2, 2, 0, 0},
	{0, 0, 0, 0, 2, 1, 0, 0},
	{1, 1, 0, 1, 0, 0, 2, 3},
	{0, 0, 0, 0, 0, 1, 1, 1},
	{0, 0, 0, 0, 0, 0, 0, 1},
	{1, 2, 1, 0, 0, 0, 3, 1},
	{0, 0, 0, 0, 0, 0, 1, 2},
	{0, 0, 0, 0, 0, 0, 1, 0},
	{0, 0, 0, 0, 0, 0, 0, 0}
};

static const int allowed_unmatched_bdl_count[ITER_CONTACTS] = {
	0,
	0,
	1,
	0,
	0,
	3,
	0,
	0,
	0,
};

static int bprio(struct bundle *b)
{
	return (b->proc_flags & BUNDLE_FLAG_EXPEDITED_PRIORITY) != 0;
}

static int handle_contact(int c, int iter)
{
	int i, j, n, p, b = 0, firstc = iter * ITER_CONTACTS, fail = 0;
	int allowed_failed_matches = allowed_unmatched_bdl_count[c];

	wait_until(contacttime(firstc + c));
	printf("Handling contact %d...\n", c);
	for (i = 5; i >= 0; i--)
		for (j = 0; j < contact_mapping[c][i]; j++)
			send_with_prio(i / 2, i % 2);
	for (i = 6; i < 8; i++)
		b += contact_mapping[c][i];
	contact_bdl_c[c] = b;
	if (b > CONTACT_MAX_BDL)
		b = CONTACT_MAX_BDL;
	p = 2;
	for (i = 0; i < b; i++) {
		printf("Waiting for bundle... ");
		contact_bdl[c][i] = upcntest_receive_next_bundle(500);
		if (contact_bdl[c][i] == NULL) {
			fail = 1;
			printf("FAIL\n");
			continue;
		}
		if (bprio(contact_bdl[c][i]) > p) {
			fail = 1;
			printf("<PRIOFAIL> ");
		}
		p = bprio(contact_bdl[c][i]);
		for (n = 0; n < out_bdl_c; n++) {
			if (bundlecmp(out_bdl[n], contact_bdl[c][i])) {
				printf("= sent bundle #%d ", n);
				break;
			}
		}
		if (n == out_bdl_c) {
			if (allowed_failed_matches-- <= 0) {
				printf("no match, not allowed - FAIL\n");
				fail = 1;
			} else {
				printf("no match, allowed - OK\n");
			}
		} else {
			printf("OK\n");
		}
	}
	return fail ? EXIT_FAILURE : EXIT_SUCCESS;
}

void rt_run(void)
{
	int i, c, fail = 0;

	upcntest_send_query();
	upcntest_send_resetstats();
	upcntest_send_settime(TEST_BEGIN);
	resettime(TEST_BEGIN);
	/* Configure GS and contacts */
	printf("Configuring system...\n");
	upcntest_send_mkgs("upcn:gs.1", "AAAAA", 1);
	upcntest_send_mkgs("upcn:gs.2", "BBBBB", 1);
	upcntest_send_mkgs("upcn:gs.3", "CCCCC", 1);
	upcntest_send_mkep("upcn:gs.1", EP_1);
	upcntest_send_mkep("upcn:gs.2", EP_2);
	upcntest_send_mkep("upcn:gs.3", EP_3);
	for (c = 0; c < TEST_ITERATIONS * 3; c++)
		add_orbit(c);
	upcntest_send_resetstats();
	for (c = 0; c < TEST_ITERATIONS; c++) {
		/* Test */
		for (i = 0; i < ITER_CONTACTS; i++)
			if (handle_contact(i, c) == EXIT_FAILURE)
				fail = 1;
		/* Delete bundles from list, ... */
		rt_cleanup();
	}
	/* Get logfile */
	upcntest_send_storetrace();
	upcntest_send_query();
	if (fail)
		printf("One or more contacts failed to process\n");
	ASSERT(!fail);
}
