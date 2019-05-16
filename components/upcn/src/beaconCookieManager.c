#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "upcn/upcn.h"
#include "upcn/beacon.h"
#include "upcn/beaconCookieManager.h"
#include "upcn/bloomFilter.h"
#include "upcn/eidManager.h"

#ifdef ND_USE_COOKIES
#include <hal_crypto.h>
#endif

#define MAX_HASHED_LENGTH 256

#ifdef ND_USE_COOKIES
#include <hal_crypto.h>

static uint8_t private_key[BEACON_PRIVATE_KEY_LENGTH];

struct cookie_hash {
	uint8_t hash[BEACON_COOKIE_HASH_LENGTH];
};

static struct cookie_info {
	uint32_t time[BEACON_MAX_COOKIE_ENTRIES];
	struct cookie_hash data[BEACON_MAX_COOKIE_ENTRIES];
	struct bloom_filter *bf;
	uint8_t count;
} ci1, ci2;

static struct cookie_info *cur_ci = &ci1;
static uint8_t clear_next = 1;

static inline void clear_ci(struct cookie_info *const ci)
{
	ci->count = 0;
	bloom_filter_clear(ci->bf);
}

/* Clear alternatingly */
static void cookie_clear(void)
{
	if (!clear_next) {
		clear_ci(&ci1);
		cur_ci = &ci1;
	} else {
		clear_ci(&ci2);
		cur_ci = &ci2;
	}
	clear_next = !clear_next;
}

static void generate_hash(
	const char *const eid, const unsigned long time,
	struct cookie_hash *const h)
{
	static uint8_t buf[MAX_HASHED_LENGTH];
	size_t elen = MIN((int)strlen(eid), MAX_HASHED_LENGTH - 4);

	memcpy(buf, &time, 4);
	memcpy(buf + 4, eid, elen);
	hal_hash_hmac(private_key, BEACON_PRIVATE_KEY_LENGTH,
		  buf, elen + 4, h->hash);
}

enum upcn_result beacon_cookie_manager_init(const uint8_t *const secret)
{
	ci1.bf = bloom_filter_init(
		BEACON_BLOOM_TABLE_SZ, BEACON_BLOOM_SALT_CNT);
	if (ci1.bf == NULL)
		return UPCN_FAIL;
	ci2.bf = bloom_filter_init(
		BEACON_BLOOM_TABLE_SZ, BEACON_BLOOM_SALT_CNT);
	if (ci2.bf == NULL) {
		bloom_filter_free(ci1.bf);
		return UPCN_FAIL;
	}
	clear_ci(&ci1);
	clear_ci(&ci2);
	memcpy(private_key, secret, BEACON_PRIVATE_KEY_LENGTH);
	return UPCN_OK;
}

void beacon_cookie_manager_add_cookie(
	const char *const eid, const unsigned long time)
{
	if (cur_ci->count == BEACON_MAX_COOKIE_ENTRIES)
		cookie_clear();
	if (!bloom_filter_contains(ci1.bf, eid)
		&& !bloom_filter_contains(ci2.bf, eid)
	) {
		bloom_filter_insert(cur_ci->bf, eid);
		generate_hash(eid, time, &cur_ci->data[cur_ci->count]);
		cur_ci->time[cur_ci->count] = time;
		cur_ci->count++;
	}
}

int beacon_cookie_manager_is_valid(
	const char *const eid, const uint8_t *const cookie_bytes,
	const unsigned long cur_time)
{
	uint8_t i, different = 0;
	uint32_t cookie_time;
	const uint8_t *cookie_hash;
	struct cookie_hash test;

	memcpy(&cookie_time, cookie_bytes, 4);
	if ((cookie_time + BEACON_COOKIE_DISCOVERY_TIME) < cur_time) {
		cookie_hash = &cookie_bytes[4];
		generate_hash(eid, cookie_time, &test);
		/* Constant time comparison */
		different = 0;
		for (i = 0; i < BEACON_COOKIE_HASH_LENGTH; i++)
			different |= cookie_hash[i] ^ test.hash[i];
		return !different;
	}
	return 0;
}

enum upcn_result beacon_cookie_manager_write_to_tlv(
	struct tlv_definition *const target)
{
	uint8_t i, cookie_count, *bcbuf, *bcptr;
	size_t cbuf_size;

	cookie_count = ci1.count + ci2.count;
	bcbuf = NULL;
	cbuf_size = cookie_count * (4 + BEACON_COOKIE_HASH_LENGTH);
	if (cookie_count != 0) {
		bcbuf = malloc(cbuf_size);
		if (bcbuf == NULL)
			return UPCN_FAIL;
		bcptr = bcbuf;
		for (i = 0; i < ci1.count; i++) {
			memcpy(bcptr, &ci1.time[i], 4);
			bcptr += 4;
			memcpy(bcptr, &ci1.data[i].hash,
				BEACON_COOKIE_HASH_LENGTH);
			bcptr += BEACON_COOKIE_HASH_LENGTH;
		}
		for (i = 0; i < ci2.count; i++) {
			memcpy(bcptr, &ci2.time[i], 4);
			bcptr += 4;
			memcpy(bcptr, &ci2.data[i].hash,
				BEACON_COOKIE_HASH_LENGTH);
			bcptr += BEACON_COOKIE_HASH_LENGTH;
		}
	}
	/* Add cookie service */
	target->length = cbuf_size;
	target->value.b = bcbuf;
	return UPCN_OK;
}

#endif /* ND_USE_COOKIES */
