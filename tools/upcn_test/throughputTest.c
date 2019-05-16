/* System includes */
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

/* uPCN includes */
#include <upcn/upcn.h>
#include <upcn/bundle.h>

/* Test includes */
#include <upcnTest.h>
#include <testlib.h>

#define CONFIG_BEGIN 9995
#define TEST_BEGIN 10000

#define CONTACT_NUMBER 2
#define MAXIMUM_CONTACT_TIME 256

#define BANDWIDTH 2300000 /* 2300 kBps */

void tt_init(void)
{
}

void tt_cleanup(void)
{
}

static void sleep_size(size_t size)
{
	double x = ((double)size/BANDWIDTH)*1000.0*1000.0;

	printf(" ->sleeping for %f...\n", x);
	usleep(x);
}


static void send(char *dest, size_t size)
{
	size_t s = 0;
	uint8_t *bin;
	struct bundle *b;

	printf("Sending Bundle with size %u\n", (unsigned int)size);
	b = upcntest_create_bundle7(size, dest, 1, 0);

	bin = upcntest_bundle_to_bin(b, &s, 0);
	upcntest_begin_send(INPUT_TYPE_BUNDLE_VERSION);
	upcntest_send_binary(bin, s);
	bundle_free(b);

	sleep_size(size);
}

static int handle_contact(int c)
{
	static int next_begin = TEST_BEGIN;

	/* wait for contact start */
	wait_until(next_begin);

	/* calculate the next contact start */
	next_begin += 15;

	if (c == 0) {
		send("dtn:test2.ep", 10);

		/* send bundles */
		for (int i = 1; i < 10; i++)
			send("dtn:test2.ep", i*10);

		for (int i = 1; i < 10; i++)
			send("dtn:test2.ep", i*100);

		for (int i = 1; i < 10; i++)
			send("dtn:test2.ep", i*1000);

		for (int i = 1; i < 10; i++)
			send("dtn:test2.ep", i*10000);

		for (int i = 1; i <= 10; i++)
			send("dtn:test2.ep", i*100000);
	}

	/* receive bundles */
	printf("Waiting for bundles ...\n");

	return EXIT_SUCCESS;
}

static void generate_gs(void)
{
	char *gs_name = malloc(100);

	for (int i = 1; i <= CONTACT_NUMBER; i++) {
		sprintf(gs_name, "dtn:gs.%d", i);
		upcntest_send_mkgs(gs_name, UPCNTEST_DEFAULT_GS_CLA, 1);
	}

	free(gs_name);
}

static void generate_ep(void)
{
	char *gs_name = malloc(100);
	char *ep_name = malloc(100);

	for (int i = 1; i <= CONTACT_NUMBER; i++) {
		sprintf(gs_name, "dtn:gs.%d", i);
		sprintf(ep_name, "dtn:test%d.ep", i);
		upcntest_send_mkep(gs_name, ep_name);
	}

	free(gs_name);
	free(ep_name);
}

static void generate_contacts(void)
{
	int start = TEST_BEGIN;
	char *gs_name = malloc(100);

	/* generate contact 1 and 2 */
	for (int i = 1; i <= 2; i++) {
		sprintf(gs_name, "dtn:gs.%d", i);
		upcntest_send_mkct(gs_name, start,
			start + 5, BANDWIDTH);

		start += 10;
	}

	free(gs_name);
}

void tt_run(void)
{
	int c, fail = 0;

	upcntest_send_query();
	printf("Resetting system...\n");
	upcntest_send_resetstats();
	upcntest_send_settime(CONFIG_BEGIN);
	resettime(CONFIG_BEGIN);
	/* Configure GS and contacts */
	printf("Configuring system...\n");
	generate_gs();
	generate_ep();
	printf("Configuring contacts...\n");
	generate_contacts();
	upcntest_send_resetstats();
	for (c = 0; c < CONTACT_NUMBER; c++) {
		printf("handling contact %d\n", c);
		if (handle_contact(c) == EXIT_FAILURE)
			fail = 1;

	}
	upcntest_send_query();
	/* Delete bundles from list, ... */
	tt_cleanup();

	if (fail)
		printf("One or more contacts failed to process\n");
	ASSERT(!fail);
}

