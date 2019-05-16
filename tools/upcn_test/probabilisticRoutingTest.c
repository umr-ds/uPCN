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

#define BUNDLE_PAYLOAD_SIZE 500

#define TEST_BEGIN 50000
#define CONTACT_BEGIN 50003
#define CONTACT_DURATION 2
#define CONTACT_SPACING 7
#define CONTACT_PERIOD 20
#define BANDWIDTH 1200 /* 9600 Baud */
#define ORBIT_COUNT 2
#define BUNDLE_COUNT 4

#define EP "upcn:prob.ep"

static struct bundle *obdl[BUNDLE_COUNT];
static struct bundle *rbdl[3 * ORBIT_COUNT][BUNDLE_COUNT];

void prt_init(void)
{
	int i;

	for (i = 0; i < BUNDLE_COUNT; i++)
		obdl[i] = upcntest_create_bundle6(
				BUNDLE_PAYLOAD_SIZE, EP, 0, 0);
}

void prt_cleanup(void)
{
	int i;

	for (i = 0; i < BUNDLE_COUNT; i++)
		bundle_free(obdl[i]);
	/* TODO: Free rbdl */
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

static void sendbdl(struct bundle *b)
{
	size_t s = 0;
	uint8_t *bin;

	bin = upcntest_bundle_to_bin(b, &s, 0);
	upcntest_begin_send(INPUT_TYPE_BUNDLE_VERSION);
	upcntest_send_binary(bin, s);
	upcntest_finish_send();
}

static int handle_contact(int c, int bdls)
{
	int i, n, success = 1;

	wait_until(contacttime(c));
	printf("Handling contact %d...\n", c);
	for (i = 0; i < bdls; i++) {
		printf("Waiting for bundle... ");
		rbdl[c][i] = upcntest_receive_next_bundle(100);
		if (rbdl[c][i] == NULL) {
			printf("FAIL\n");
			success = 0;
			continue;
		}
		for (n = 0; n < BUNDLE_COUNT; n++) {
			if (bundlecmp(obdl[n], rbdl[c][i])) {
				printf("= sent bundle #%d ", n);
				break;
			}
		}
		if (n == BUNDLE_COUNT)
			printf("[ no match ] ");
		printf("OK\n");
	}
	return success;
}

void prt_run(void)
{
	int c, success = 1;

	upcntest_send_query();
	upcntest_send_resetstats();
	upcntest_send_settime(TEST_BEGIN);
	resettime(TEST_BEGIN);
	/* Configure GS and contacts */
	printf("Configuring system...\n");
	upcntest_send_mkgs("upcn:gs.1", "AAAAA", 1.0);
	upcntest_send_mkgs("upcn:gs.2", "BBBBB", 0.6);
	upcntest_send_mkgs("upcn:gs.3", "CCCCC", 0.7);
	upcntest_send_mkep("upcn:gs.2", EP);
	upcntest_send_mkep("upcn:gs.3", EP);
	for (c = 0; c < ORBIT_COUNT; c++)
		add_orbit(c);
	upcntest_send_resetstats();
	wait_until(contacttime(0));
	for (c = 0; c < BUNDLE_COUNT; c++)
		sendbdl(obdl[c]);
	success &= handle_contact(1, 4);
	success &= handle_contact(2, 4);
	success &= handle_contact(3, 0);
	success &= handle_contact(4, 4);
	success &= handle_contact(5, 0);
	upcntest_send_storetrace();
	upcntest_send_query();
	ASSERT(success);
}
