#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>

#include "upcn/upcn.h"
#include "upcn/beacon.h"
#include "upcn/eidManager.h"
#include "upcn/beaconSerializer.h"
#include "upcn/beaconCookieManager.h"
#include "upcn/routingTable.h"
#include "upcn/bloomFilter.h"

#ifndef ND_DISABLE_BEACONS

static uint64_t next_time;

static struct beacon *own_beacon;

#define BEACON_TX_RX_BITRATE_SERVICE 0
#define BEACON_NBF_HASHES_SERVICE 1
#define BEACON_NBF_BITS_SERVICE 2

#ifdef ND_USE_COOKIES
#define BEACON_COOKIE_SERVICE 3
#define BEACON_SERVICE_COUNT 4
#else /* ND_USE_COOKIES */
#define BEACON_SERVICE_COUNT 3
#endif /* ND_USE_COOKIES */


static struct bloom_filter *own_filter;
static uint8_t own_filter_hashes[BEACON_OWN_BLOOM_SALT_CNT];
static uint8_t own_filter_cache[BEACON_OWN_BLOOM_TABLE_SZ];
static uint8_t own_filter_counter;

static void update_own_bf_cache(void)
{
	size_t length;
	struct ground_station_list *gs;

	if (!own_filter_counter) {
		gs = routing_table_get_station_list();
		bloom_filter_clear(own_filter);
		while (gs != NULL) {
			bloom_filter_insert(own_filter, gs->station->eid);
			gs = gs->next;
		}
		length = bloom_filter_export_to(own_filter, own_filter_cache,
						BEACON_OWN_BLOOM_TABLE_SZ);
		ASSERT(length == BEACON_OWN_BLOOM_TABLE_SZ);
		own_filter_counter = BEACON_OWN_BLOOM_REFRESH_INTERVAL;
	} else {
		own_filter_counter--;
	}
}

enum upcn_result beacon_generator_init(void)
{
	static char my_scheme[] = UPCN_SCHEME;
	static char my_ssp[] = UPCN_SSP;

	uint16_t eid_len;
	uint8_t i;
	struct tlv_definition *cur_tlv;

	own_beacon = beacon_init();
	if (own_beacon == NULL)
		return UPCN_FAIL;

	own_filter = bloom_filter_init(BEACON_OWN_BLOOM_TABLE_SZ * 8,
				       BEACON_OWN_BLOOM_SALT_CNT);
	if (own_filter == NULL)
		goto fail_b;

	own_beacon->version = 0x04;
	own_beacon->flags = BEACON_FLAG_HAS_EID | BEACON_FLAG_HAS_PERIOD
		| BEACON_FLAG_HAS_SERVICES;
	own_beacon->sequence_number = 0;
	eid_len = sizeof(UPCN_SCHEME) + sizeof(UPCN_SSP) + 1;
	own_beacon->eid = malloc(eid_len + 1);

	if (own_beacon->eid == NULL)
		goto fail_own_filter;
	memcpy(own_beacon->eid, my_scheme, sizeof(my_scheme));
	own_beacon->eid[sizeof(my_scheme)] = ':';
	memcpy(own_beacon->eid + sizeof(my_scheme), my_ssp, sizeof(my_ssp));
	own_beacon->eid[eid_len] = '\0';
	own_beacon->eid = eidmanager_alloc_ref(own_beacon->eid, true);
	own_beacon->period = BEACON_PERIOD / 1000;
	own_beacon->service_count = BEACON_SERVICE_COUNT;
	own_beacon->services = malloc(
		sizeof(struct tlv_definition) * BEACON_SERVICE_COUNT);
	if (own_beacon->services == NULL)
		goto fail_eid;

#ifdef ND_USE_COOKIES
	/* Add cookie "service" */
	cur_tlv = tlv_populate_ct(own_beacon->services, BEACON_COOKIE_SERVICE,
		TLV_TYPE_PRIVATE_COOKIES, 1);
	if (cur_tlv == NULL)
		goto fail_services;
	cur_tlv[0].tag = TLV_TYPE_BYTES; /* The rest is zeroed automatically */
#endif /* ND_USE_COOKIES */

	/* Add transmit/receive bitrate "service" */
	cur_tlv = tlv_populate_ct(own_beacon->services,
		BEACON_TX_RX_BITRATE_SERVICE,
		TLV_TYPE_PRIVATE_TX_RX_BITRATE, 1);
	if (cur_tlv == NULL)
		goto fail_first_service;
	cur_tlv = &cur_tlv[0];
	cur_tlv->tag = TLV_TYPE_FIXED32;
	cur_tlv->value.u32 = (((uint32_t)DEFAULT_TX_BITRATE) << 16)
		| DEFAULT_RX_BITRATE;

	/* Add NBF hashes "service" */
	cur_tlv = tlv_populate_ct(own_beacon->services,
		BEACON_NBF_HASHES_SERVICE,
		TLV_TYPE_NBF_HASHES, 1);
	if (cur_tlv == NULL)
		goto fail_second_service;
	cur_tlv = &cur_tlv[0];
	cur_tlv->tag = TLV_TYPE_BYTES;
	cur_tlv->length = BEACON_OWN_BLOOM_SALT_CNT;
	cur_tlv->value.b = &own_filter_hashes[0];
	/* Fill the array */
	ASSERT(BEACON_OWN_BLOOM_SALT_CNT <= 64);
	for (i = 0; i < BEACON_OWN_BLOOM_SALT_CNT; i++)
		own_filter_hashes[i] = BLOOM_FILTER_HASH_BASE_ID + i;

	/* Add NBF bits "service" */
	cur_tlv = tlv_populate_ct(own_beacon->services,
		BEACON_NBF_BITS_SERVICE,
		TLV_TYPE_NBF_BITS, 1);
	if (cur_tlv == NULL)
		goto fail_third_service;
	cur_tlv = &cur_tlv[0];
	cur_tlv->tag = TLV_TYPE_BYTES;
	cur_tlv->length = BEACON_OWN_BLOOM_TABLE_SZ;
	cur_tlv->value.b = &own_filter_cache[0];

	/* TODO: Add CLA TLV */
	return UPCN_OK;

fail_third_service:
	free(own_beacon->services[BEACON_NBF_HASHES_SERVICE]
		.value.children.list);
fail_second_service:
	free(own_beacon->services[BEACON_TX_RX_BITRATE_SERVICE]
		.value.children.list);
fail_first_service:
#ifdef ND_USE_COOKIES
	free(own_beacon->services[BEACON_COOKIE_SERVICE]
		.value.children.list);
fail_services:
#endif /* ND_USE_COOKIES */
	free(own_beacon->services);
fail_eid:
	eidmanager_free_ref(own_beacon->eid);
fail_own_filter:
	bloom_filter_free(own_filter);
fail_b:
	beacon_free(own_beacon);
	return UPCN_FAIL;
}

uint64_t beacon_generator_check_send(uint64_t cur_time)
{
	uint8_t *bd;
	uint16_t bl;
#ifdef ND_USE_COOKIES
	struct tlv_definition *cur_tlv;
#endif /* ND_USE_COOKIES */
	if (cur_time >= next_time) {
#ifdef ND_USE_COOKIES
		/* Add cookie service */
		cur_tlv = &own_beacon->services[BEACON_COOKIE_SERVICE]
			.value.children.list[0];
		if (beacon_cookie_manager_write_to_tlv(cur_tlv) != UPCN_OK)
			return next_time;
#endif /* ND_USE_COOKIES */
		/* Update NBF if necessary */
		update_own_bf_cache();
		/* Serialize */
		bl = 0;
		bd = beacon_serialize(own_beacon, &bl);
#ifdef ND_USE_COOKIES
		free(cur_tlv->value.b);
		cur_tlv->length = 0;
		cur_tlv->value.b = NULL;
#endif /* ND_USE_COOKIES */
		if (bd == NULL)
			return next_time;
		LOGI("BeaconProcessor: Sending beacon",
			own_beacon->sequence_number);
		hal_io_lock_com_semaphore();
		if (bd != NULL) {
			hal_io_send_packet(bd, bl, COMM_TYPE_BEACON);
			free(bd);
		}
		hal_io_unlock_com_semaphore();
		/* We might have waited during sending the data */
		/* cur_time = hal_time_get_timestamp_ms(); */
		next_time += ((cur_time - next_time) / BEACON_PERIOD + 1)
			* BEACON_PERIOD;
		own_beacon->sequence_number++;
	}
	return next_time;
}

void beacon_generator_reset_next_time(void)
{
	next_time = 0;
}

#endif /* ND_DISABLE_BEACONS */
