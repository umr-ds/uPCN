/* System includes */
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

/* uPCN includes */
#include <upcn/upcn.h>
#include <upcn/beacon.h>

/* Test includes */
#include <upcnTest.h>
#include <testlib.h>

#define T_BEACON_PERIOD 1
#define T_BR 1200
#define T_BEAC_CNT 100

#define GS "upcn:testgs"
#define EP "upcn:testep"

static struct beacon *beacs[T_BEAC_CNT];
static int randc;

void idt_init(void)
{
	static char *ep1 = EP;
	static char gs[100];
	int i;

	randc = rand();
	for (i = 0; i < T_BEAC_CNT; i++) {
		sprintf(gs, "%s.%d.%d", GS, randc, i);
		beacs[i] = upcntest_create_beacon(gs, T_BEACON_PERIOD, T_BR,
			NULL, 0, (const char **)&ep1, 1);
	}
}

void idt_cleanup(void)
{
	int i;

	for (i = 0; i < T_BEAC_CNT; i++)
		beacon_free(beacs[i]);
}

static void beacsend(struct beacon *b)
{
	size_t s = 0;
	uint8_t *bin = upcntest_beacon_to_bin(b, &s, 0);

	printf(".");
	upcntest_begin_send(INPUT_TYPE_BEACON_DATA);
	upcntest_send_binary(bin, s);
	upcntest_finish_send();
}

void idt_run(void)
{
	int i;

	upcntest_send_query();
	resettime(100000);
	upcntest_send_resetstats();
	for (i = 0; i < T_BEAC_CNT; i++)
		beacsend(beacs[i]);
	printf("\n");
	wait_for(500);
	upcntest_send_storetrace();
	upcntest_send_query();
	upcntest_send_router_query();
}
