#ifndef SERIALIZE_BEACON_H_INCLUDED
#define SERIALIZE_BEACON_H_INCLUDED

#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <serialize_helper.h>

#include <upcn/upcn.h>
#include <upcn/rrnd.h>
#include <upcn/beacon.h>
#include <upcn/beaconSerializer.h>
#include <upcn/eidList.h>
#include <upcn/eidManager.h>

/* TODO: Use in upcn_test! */
static struct serialized_data serialize_new_beacon(
	const unsigned short seq, const char *const eid, const uint16_t period,
	const uint16_t tx_bitrate, const uint16_t rx_bitrate,
	const void *const cookie, const size_t cookie_length,
	const char *const *const eids, const size_t eid_count,
	const enum rrnd_beacon_flags rrnd_flags,
	const unsigned long availability_duration)
{
	struct beacon *b = beacon_init();
	struct tlv_definition *t;
	uint16_t cur_svc, templen;
	unsigned char *serializebuffer;
	size_t eid_alloc_len;

	b->version = 0x04;
	b->flags = BEACON_FLAG_HAS_EID
		| BEACON_FLAG_HAS_SERVICES | BEACON_FLAG_HAS_PERIOD;
	eid_alloc_len = strlen(eid) + 1;
	b->eid = malloc(eid_alloc_len);
	strncpy(b->eid, eid, eid_alloc_len);
	b->eid = eidmanager_alloc_ref(b->eid, true);
	b->sequence_number = seq;
	b->period = period;
	b->service_count = 2; /* BR + Flags at minimum */
	if (cookie != NULL)
		b->service_count++;
	if (eids != NULL)
		b->service_count++;
	if (HAS_FLAG(rrnd_flags, RRND_FLAG_INTERMITTENT_AVAILABILITY))
		b->service_count++;
	b->services = calloc(b->service_count, sizeof(struct tlv_definition));
	cur_svc = 0;
	t = tlv_populate_ct(b->services,
		cur_svc, TLV_TYPE_PRIVATE_TX_RX_BITRATE, 1);
	if (t != NULL) {
		t[0].tag = TLV_TYPE_FIXED32;
		t[0].value.u32 = (tx_bitrate << 16) | rx_bitrate;
	}
	cur_svc++;
	t = tlv_populate_ct(b->services,
		cur_svc, TLV_TYPE_PRIVATE_RRND_FLAGS, 1);
	if (t != NULL) {
		t[0].tag = TLV_TYPE_FIXED16;
		t[0].value.u16 = (uint16_t)rrnd_flags;
	}
	if (cookie != NULL) {
		cur_svc++;
		t = tlv_populate_ct(b->services,
			cur_svc, TLV_TYPE_PRIVATE_COOKIES, 1);
		if (t != NULL) {
			t[0].tag = TLV_TYPE_BYTES;
			t[0].value.b = malloc(cookie_length);
			if (t[0].value.b != NULL) {
				t[0].length = (uint16_t)cookie_length;
				memcpy(t[0].value.b, cookie, cookie_length);
			}
		}
	}
	if (eids != NULL) {
		cur_svc++;
		t = tlv_populate_ct(b->services,
			cur_svc, TLV_TYPE_PRIVATE_NEIGHBOR_EIDS, 1);
		if (t != NULL) {
			t[0].tag = TLV_TYPE_BYTES;
			t[0].value.b = eidlist_encode(
				eids, eid_count, (int *)&templen);
			if (t[0].value.b != NULL)
				t[0].length = templen;
		}
	}
	if (HAS_FLAG(rrnd_flags, RRND_FLAG_INTERMITTENT_AVAILABILITY)) {
		cur_svc++;
		t = tlv_populate_ct(b->services,
			cur_svc, TLV_TYPE_PRIVATE_RRND_AVAILABILITY, 1);
		if (t != NULL) {
			t[0].tag = TLV_TYPE_UINT64;
			t[0].value.u64 = (uint64_t)availability_duration;
		}
	}
	serializebuffer = beacon_serialize(b, &templen);
	beacon_free(b);
	return (struct serialized_data){ serializebuffer, (size_t)templen };
}

#endif /* SERIALIZE_BEACON_H_INCLUDED */
