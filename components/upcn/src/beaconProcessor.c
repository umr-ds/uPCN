#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "upcn/upcn.h"
#include "upcn/beacon.h"
#include "upcn/beaconProcessor.h"
#include "upcn/beaconCookieManager.h"
#include "upcn/beaconGenerator.h"
#include "upcn/groundStation.h"
#include "upcn/routingTable.h"
#include "upcn/routerTask.h"
#include "upcn/bloomFilter.h"
#include "upcn/eidList.h"
#include "upcn/eidManager.h"
#include "upcn/satpos.h"

#ifdef ND_USE_COOKIES
static uint8_t contains_valid_cookie(
	struct beacon *b)
{
	struct tlv_definition *cookie_tlv, *cookie_bytes_tlv;
	uint8_t *cookie_bytes;
	uint16_t cookie_count, c, i;
	uint64_t cur_time;

	cookie_tlv = beacon_get_service(b, TLV_TYPE_PRIVATE_COOKIES);
	if (cookie_tlv == NULL)
		return 0;
	cookie_bytes_tlv = tlv_get_child(cookie_tlv, TLV_TYPE_BYTES);
	if (cookie_bytes_tlv == NULL || cookie_bytes_tlv->length % (4 +
			BEACON_COOKIE_HASH_LENGTH) != 0)
		return 0;
	cookie_bytes = cookie_bytes_tlv->value.b;
	cookie_count = cookie_bytes_tlv->length
		/ (4 + BEACON_COOKIE_HASH_LENGTH);
	cur_time = hal_time_get_timestamp_s();
	for (c = 0; c < cookie_count; c++) {
		i = c * (4 + BEACON_COOKIE_HASH_LENGTH);
		if (beacon_cookie_manager_is_valid(
				b->eid, &cookie_bytes[i],
				(unsigned long)cur_time))
			return 1;
	}
	return 0;
}
#endif /* ND_USE_COOKIES */

static struct endpoint_list *get_eid_list_from_tlv(const struct beacon *const b)
{
	struct tlv_definition *tlv;
	char **eid_list;
	uint8_t i, j;
	struct endpoint_list *ret = NULL, *cur, *tmp;

	tlv = beacon_get_service(b, TLV_TYPE_PRIVATE_NEIGHBOR_EIDS);
	if (tlv == NULL)
		return NULL;
	tlv = tlv_get_child(tlv, TLV_TYPE_BYTES);
	if (tlv == NULL)
		return NULL;
	if (tlv->length < 2 || tlv->value.b[0] == 0)
		return NULL;
	eid_list = eidlist_decode(tlv->value.b, tlv->length);
	if (eid_list == NULL)
		return NULL;
	for (i = 0; i < tlv->value.b[0]; i++) {
		tmp = malloc(sizeof(struct endpoint_list));
		if (tmp == NULL) {
			for (j = 0; j < tlv->value.b[0]; j++)
				eidmanager_free_ref(eid_list[j]);
			free(eid_list);
			while ((ret = endpoint_list_free(ret)) != NULL)
				;
			return NULL;
		}
		tmp->eid = eid_list[i];
		tmp->p = DISCOVERY_NODE_LIST_INITIAL_P;
		tmp->next = NULL;
		if (i == 0)
			ret = tmp;
		else
			cur->next = tmp;
		cur = tmp;
	}
	free(eid_list);
	return ret;
}

static struct bloom_filter *get_nbf_from_tlv(const struct beacon *const b)
{
	const struct tlv_definition *tlv;
	struct bloom_filter *ret;
	uint8_t salts = DISCOVERY_NBF_DEFAULT_HASHES;

	tlv = beacon_get_service(b, TLV_TYPE_NBF_HASHES);
	if (tlv != NULL) {
		tlv = tlv_get_child(tlv, TLV_TYPE_BYTES);
		if (tlv != NULL) {
			if (tlv->length > 0 && (tlv->value.b[0]
					== BLOOM_FILTER_HASH_BASE_ID))
				salts = tlv->length;
		}
	}
	tlv = beacon_get_service(b, TLV_TYPE_NBF_BITS);
	if (tlv == NULL)
		return NULL;
	tlv = tlv_get_child(tlv, TLV_TYPE_BYTES);
	if (tlv == NULL)
		return NULL;
	if (tlv->length == 0 || tlv->length > 255)
		return NULL;
	ret = bloom_filter_import(tlv->value.b, tlv->length * 8, salts);
	return ret;
}

enum upcn_result beacon_processor_init(const uint8_t *const secret)
{
#ifdef ND_USE_COOKIES
	if (beacon_cookie_manager_init(secret) != UPCN_OK)
		return UPCN_FAIL;
#endif /* ND_USE_COOKIES */
#ifndef ND_DISABLE_BEACONS
	if (beacon_generator_init() != UPCN_OK)
		return UPCN_FAIL;
#endif /* ND_DISABLE_BEACONS */
	return UPCN_OK;
}

static int is_valid(const struct beacon *const b)
{
	return (
		b->version == 0x04 &&
		b->eid != NULL &&
		b->period > 0 &&
		b->services != NULL &&
		beacon_get_service(b, TLV_TYPE_PRIVATE_TX_RX_BITRATE) != NULL
	);
}

static struct rrnd_beacon extract_rrnd_info(const struct beacon *const b)
{
	const struct tlv_definition *tlv;
	struct rrnd_beacon ret;

	ret.sequence_number = b->sequence_number;
	ret.period = b->period * 100;
	tlv = beacon_get_service(b, TLV_TYPE_PRIVATE_TX_RX_BITRATE);
	tlv = tlv_get_child(tlv, TLV_TYPE_FIXED32);
	if (tlv != NULL) {
		ret.tx_bitrate = ((tlv->value.u32 >> 16) & 0xFFFF) * 8;
		ret.rx_bitrate = (tlv->value.u32 & 0xFFFF) * 8;
	} else {
		ret.tx_bitrate = 0;
		ret.rx_bitrate = 0;
	}
	ret.flags = RRND_FLAG_NONE;
	tlv = beacon_get_service(b, TLV_TYPE_PRIVATE_RRND_FLAGS);
	tlv = tlv_get_child(tlv, TLV_TYPE_FIXED16);
	if (tlv != NULL)
		ret.flags = (enum rrnd_beacon_flags)tlv->value.u16;
	tlv = beacon_get_service(b, TLV_TYPE_PRIVATE_HAS_INTERNET_ACCESS);
	tlv = tlv_get_child(tlv, TLV_TYPE_BOOLEAN);
	if (tlv != NULL && tlv->value.u8 == 1)
		ret.flags |= RRND_FLAG_INTERNET_ACCESS;
	tlv = beacon_get_service(b, TLV_TYPE_PRIVATE_RRND_AVAILABILITY);
	tlv = tlv_get_child(tlv, TLV_TYPE_UINT64);
	if (tlv != NULL)
		ret.availability_duration = (unsigned long)tlv->value.u64;
	else
		ret.availability_duration = 0;
	return ret;
}

struct processed_beacon_info beacon_processor_process(
	struct beacon *beacon, Semaphore_t gs_semaphore,
	QueueIdentifier_t router_signaling_queue)
{
	struct ground_station *gs;
	struct processed_beacon_info ret = { /* TODO */
		.origin = BEACON_ORIGIN_UNKNOWN,
		.handle = NULL
	};
	struct rrnd_beacon rrnd_data;
	enum rrnd_status rc;
	struct router_command *cmd;
	struct router_signal sig = {
		.type = ROUTER_SIGNAL_PROCESS_COMMAND,
		.data = NULL
	};

#ifdef VERBOSE
	if (beacon->eid != NULL) {
		hal_io_write_string("BeaconProcessor: Beacon from EID ");
		hal_io_write_string(beacon->eid);
		hal_io_write_string("\n");
	} else {
		LOGI("Invalid (no-EID) beacon received", beacon);
	}
#endif
	/* TODO: Neighbor list + NBFs */
	(void)get_eid_list_from_tlv;
	(void)get_nbf_from_tlv;
	if (!is_valid(beacon)) {
		LOGI("Invalid beacon received", beacon);
		beacon_free(beacon);
		return ret;
	}

	// NOTE: The beacon processor uses the GS list!
	hal_semaphore_take_blocking(gs_semaphore);
	gs = routing_table_lookup_ground_station(beacon->eid);
	if (gs != NULL) {
		LOGI("Beacon from known GS received", beacon);
		if (gs->rrnd_info != NULL) {
			rrnd_data = extract_rrnd_info(beacon);
			rc = rrnd_process(gs->rrnd_info, rrnd_data,
				hal_time_get_timestamp_ms(), 0, satpos_get);
			hal_io_send_packet(&rc, RRND_STATUS_SIZEOF,
				COMM_TYPE_RRND_STATUS);
			/*if (HAS_FLAG(rc, RRND_STATUS_UPDATED))*/
			/* TODO: Report? */
		}
		hal_semaphore_release(gs_semaphore);
#ifdef ND_USE_COOKIES
	} else if (contains_valid_cookie(beacon)) {
		LOGI("Beacon with valid cookie received", beacon);
#else /* ND_USE_COOKIES */
	} else {
		LOGI("New beacon received", beacon);
#endif /* ND_USE_COOKIES */
		hal_semaphore_release(gs_semaphore);
		gs = ground_station_create(beacon->eid);
		gs->rrnd_info = ground_station_rrnd_info_create(gs);
		rrnd_data = extract_rrnd_info(beacon);
		rc = rrnd_process(gs->rrnd_info, rrnd_data,
			hal_time_get_timestamp_ms(), 0, satpos_get);
		if (!HAS_FLAG(rc, RRND_STATUS_FAILED)) {
			/* GS discovered */
			gs->flags |= GS_FLAG_DISCOVERED;
			if (HAS_FLAG(rrnd_data.flags,
					RRND_FLAG_INTERNET_ACCESS))
				gs->flags |= GS_FLAG_INTERNET_ACCESS;
			cmd = malloc(sizeof(struct router_command));
			if (cmd == NULL) {
				free_ground_station(gs);
			} else {
				cmd->type = ROUTER_COMMAND_ADD;
				cmd->data = gs;
				sig.data = cmd;
				hal_queue_push_to_back(router_signaling_queue,
						&sig);
			}
		} else {
			free_ground_station(gs);
		}
		hal_io_send_packet(&rc, RRND_STATUS_SIZEOF,
			COMM_TYPE_RRND_STATUS);
#ifdef ND_USE_COOKIES
	} else {
		hal_semaphore_release(gs_semaphore);
		LOGI("New beacon received", beacon);
		beacon_cookie_manager_add_cookie(
			beacon->eid, (uint32_t)hal_time_get_timestamp_s());
#endif /* ND_USE_COOKIES */
	}
	beacon_free(beacon);
	return ret;
}
