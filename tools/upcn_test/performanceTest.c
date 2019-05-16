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

#define CONTACT_NUMBER 25
#define MAXIMUM_CONTACT_TIME 256

#define BANDWIDTH 230000 /* 230 kBps */

static int contact_durations_average[5] = {
	200,
	80,
	40,
	10,
	2
};

static int wait_time[26] = {
	10000,
	10400,
	10560,
	10640,
	10660,
	10666,
	10678,
	10728,
	10848,
	11128,
	11350,
	11351,
	11352,
	11353,
	11354,
	11355,
	11356,
	11357,
	11358,
	11359,
	11360,
	11616,
	11872,
	12128,
	12384,
	12640
};

void pt_init(void)
{
}

void pt_cleanup(void)
{
}


static void sleep_size(size_t size)
{
	double x = ((double)size/230000.0)*1000.0*1000.0;

	printf(" ->sleeping for %f...\n", x);
	usleep(x);
}


static void send(char *dest, size_t size)
{
	size_t s = 0;
	uint8_t *bin;
	struct bundle *b;

	printf("Sending Bundle with size %u\n", (unsigned int)size);

	b = upcntest_create_bundle6(size, dest, 1, 0);
	bin = upcntest_bundle_to_bin(b, &s, 0);
	upcntest_begin_send(INPUT_TYPE_BUNDLE_VERSION);
	upcntest_send_binary(bin, s);
	upcntest_finish_send();
	bundle_free(b);

	sleep_size(size);
}

static int handle_contact(int c)
{
	char *gs_name;

	printf("--\nHandling Contact %d\n", c);
	/* wait for contact start */
	printf("Waiting until %d ms...\n", wait_time[c]);
	wait_until(wait_time[c]);

	if (c < 10) {
		/* average scenario */

		/* send bundles */
		switch (c) {
		case 0:
			for (int i = 0; i < 32; i++)
				send("upcn:test10.ep", 1000000);
			send("upcn:test10.ep", 200000);
			break;

		case 1:
			for (int i = 0; i < 12; i++)
				send("upcn:test9.ep", 1000000);
			send("upcn:test9.ep", 800000);
			break;

		case 2:
			for (int i = 0; i < 51; i++)
				send("upcn:test8.ep", 100000);
			for (int i = 0; i < 13; i++)
				send("upcn:test4.ep", 100000);
			break;


		case 3:
			for (int i = 0; i < 53; i++)
				send("upcn:test5.ep", 5000);
			break;

		case 4:
			for (int i = 0; i < 100; i++)
				send("upcn:test6.ep", 500);
			break;

		case 5:
			for (int i = 0; i < 53; i++)
				send("upcn:test7.ep", 5000);
			break;

		case 6:
			for (int i = 0; i < 13; i++)
				send("upcn:test8.ep", 100000);
			break;

		default:
			break;

		}

	} else if (c < 19) {
		gs_name = malloc(100);
		sprintf(gs_name, "upcn:gs.%d", c+2);

		/* Short contact scenario */
		for (int i = 0; i <= 20; i++)
			send(gs_name, 10000);

		free(gs_name);

	} else if (c >= 20 && c < 25) {
		/* Long contact scenario */

		/* send bundles */
		switch (c) {
		case 20:
			for (int i = 0; i < 53; i++)
				send("upcn:test22.ep", 1000000);
			break;

		case 23:
			for (int i = 0; i < 50500; i++)
				send("upcn:test25.ep", 1000);
			break;

		default:
			break;

		}
	}

	/* receive bundles */

	return EXIT_SUCCESS;
}

static void generate_gs(void)
{
	char *gs_name = malloc(100);

	for (int i = 1; i <= CONTACT_NUMBER; i++) {
		sprintf(gs_name, "upcn:gs.%d", i);
		upcntest_send_mkgs(gs_name, UPCNTEST_DEFAULT_GS_CLA, 1);
	}

	free(gs_name);
}

static void generate_ep(void)
{
	char *gs_name = malloc(100);
	char *ep_name = malloc(100);

	for (int i = 1; i <= CONTACT_NUMBER; i++) {
		sprintf(gs_name, "upcn:gs.%d", i);
		sprintf(ep_name, "upcn:test%d.ep", i);
		upcntest_send_mkep(gs_name, ep_name);
	}

	free(gs_name);
	free(ep_name);
}

static void generate_contacts(void)
{
	int start = TEST_BEGIN;
	int end = 1328 + TEST_BEGIN;
	char *gs_name = malloc(100);

	/* generate contacts for average situation */
	for (int i = 1; i <= 5; i++) {
		sprintf(gs_name, "upcn:gs.%d", i);
		upcntest_send_mkct(gs_name, start,
			start + contact_durations_average[i-1], BANDWIDTH);

		sprintf(gs_name, "upcn:gs.%d", 11-i);
		upcntest_send_mkct(gs_name,
				   end - contact_durations_average[i-1],
			end, BANDWIDTH);

		start += 2*contact_durations_average[i-1];
		end -= 2*contact_durations_average[i-1];
	}

	free(gs_name);

	/* generate contacts for maximum situation */
	start = TEST_BEGIN + 1350;

	/* generate contacts for short contact test */
	for (int i = 11; i <= 20; i++) {
		sprintf(gs_name, "upcn:gs.%d", i);
		upcntest_send_mkct(gs_name, start,
			start + 1, BANDWIDTH);

		start += 1;
	}

	/* generate contacts for max contact test */
	for (int i = 21; i <= 25; i++) {
		sprintf(gs_name, "upcn:gs.%d", i);
		upcntest_send_mkct(gs_name, start,
			start + MAXIMUM_CONTACT_TIME, BANDWIDTH);

		start += MAXIMUM_CONTACT_TIME;
	}

}

void pt_run(void)
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
		if (handle_contact(c) == EXIT_FAILURE)
			fail = 1;

	}
	/* Delete bundles from list, ... */
	pt_cleanup();

	if (fail)
		printf("One or more contacts failed to process\n");
	ASSERT(!fail);
}

