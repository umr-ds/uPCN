#include <stdio.h>
#include <stddef.h>

#include "upcn/beacon.h"
#include "upcn/beaconParser.h"
#include "upcn/bundle.h"
#include "bundle6/parser.h"
#include "upcn/eidManager.h"

#include <serialize_beacon.h>
#include <serialize_bundle.h>

/* How many beacons to serialize */
#define SAMPLE_COUNT 10000

/* Typical, the EDP block only consists of some SDNVs, can stay zero here */
#define EDP_BLOCKLEN 15

static const char source_eid[] = "ipn://test_gs_01";
static const char multihop_1[] = "dtn://test_eid_1";

static struct serialized_data immediate_ipnd[SAMPLE_COUNT];
static struct serialized_data immediate_edp[SAMPLE_COUNT];

static size_t serialize_ipnd(unsigned short seqnum)
{
	const char *const eids[2] = { multihop_1 };

	immediate_ipnd[seqnum] = serialize_new_beacon(
		seqnum, source_eid, 10, 9600, 9600, NULL, 0, eids, 1,
		RRND_FLAG_INTERNET_ACCESS, 0);
	return immediate_ipnd[seqnum].size;
}

static size_t serialize_edp(unsigned short seqnum)
{
	immediate_edp[seqnum] = serialize_new_bundle(
		EDP_BLOCKLEN, source_eid, multihop_1, 0, 0, 5000);
	return immediate_edp[seqnum].size;
}

static void beac_send(struct beacon *beac)
{
	beacon_free(beac);
}

static int try_parse_ipnd(unsigned short seqnum)
{
	struct beacon_parser p;

	beacon_parser_init(&p, &beac_send);
	beacon_parser_read(&p,
		immediate_ipnd[seqnum].dataptr,
		immediate_ipnd[seqnum].size);
	free(p.basedata);
	if (p.beacon != NULL) {
		beacon_free(p.beacon);
		return 0;
	}
	return 1;
}

static void bundle_send(struct bundle *bundle)
{
	bundle_free(bundle);
}

static int try_parse_edp(unsigned short seqnum)
{
	struct bundle6_parser p;

	bundle6_parser_init(&p, &bundle_send);
	bundle6_parser_read(&p,
		immediate_edp[seqnum].dataptr,
		immediate_edp[seqnum].size);
	free(p.basedata);
	if (p.bundle != NULL) {
		bundle_free(p.bundle);
		return 0;
	}
	return 1;
}

int main(void)
{
	int i, memsize, success = 1;

	/* XXX This is needed for serializing beacons */
	eidmanager_init();
	for (i = 0, memsize = 0; i < SAMPLE_COUNT; i++)
		memsize += serialize_ipnd(i);
	printf("Serialized %d IPND beacons, mem = %d\n", SAMPLE_COUNT, memsize);
	for (i = 0, memsize = 0; i < SAMPLE_COUNT; i++)
		memsize += serialize_edp(i);
	printf("Serialized %d bundles, mem = %d\n", SAMPLE_COUNT, memsize);
	for (i = 0; i < SAMPLE_COUNT; i++)
		success &= try_parse_ipnd(i);
	printf("Parsed %d IPND beacons, success = %d\n", SAMPLE_COUNT, success);
	for (i = 0; i < SAMPLE_COUNT; i++)
		success &= try_parse_edp(i);
	printf("Parsed %d bundles, success = %d\n", SAMPLE_COUNT, success);
	for (i = 0; i < SAMPLE_COUNT; i++) {
		free(immediate_ipnd[i].dataptr);
		free(immediate_edp[i].dataptr);
	}
	return 0;
}
