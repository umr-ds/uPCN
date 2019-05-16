/*
 * This bloom filter implementation is a C-port of the one
 * found in IBR-DTN (https://www.ibr.cs.tu-bs.de/projects/ibr-dtn/)
 * written by Johannes Morgenroth, which is based on
 * Open Bloom Filter (http://www.partow.net) written by Arash Partow.
 */

#include <stddef.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

#include "upcn/upcn.h"
#include "upcn/bloomFilter.h"

#define bloom_type uint32_t

#define PREDEF_SALT_COUNT 64

static const bloom_type PREDEF_SALT[PREDEF_SALT_COUNT] = {
	0xAAAAAAAA, 0x55555555, 0x33333333, 0xCCCCCCCC,
	0x66666666, 0x99999999, 0xB5B5B5B5, 0x4B4B4B4B,
	0xAA55AA55, 0x55335533, 0x33CC33CC, 0xCC66CC66,
	0x66996699, 0x99B599B5, 0xB54BB54B, 0x4BAA4BAA,
	0xAA33AA33, 0x55CC55CC, 0x33663366, 0xCC99CC99,
	0x66B566B5, 0x994B994B, 0xB5AAB5AA, 0xAAAAAA33,
	0x555555CC, 0x33333366, 0xCCCCCC99, 0x666666B5,
	0x9999994B, 0xB5B5B5AA, 0xFFFFFFFF, 0xFFFF0000,
	0xB823D5EB, 0xC1191CDF, 0xF623AEB3, 0xDB58499F,
	0xC8D42E70, 0xB173F616, 0xA91A5967, 0xDA427D63,
	0xB1E8A2EA, 0xF6C0D155, 0x4909FEA3, 0xA68CC6A7,
	0xC395E782, 0xA26057EB, 0x0CD5DA28, 0x467C5492,
	0xF15E6982, 0x61C6FAD3, 0x9615E352, 0x6E9E355A,
	0x689B563E, 0x0C9831A8, 0x6753C18B, 0xA622689B,
	0x8CA63C47, 0x42CC2884, 0x8E89919B, 0x6EDBD7D3,
	0x15B6796C, 0x1D6FDFE4, 0x63FF9092, 0xE7401432
};

static const uint8_t BIT_MASK[8] = {
	0x01,  /* 00000001 */
	0x02,  /* 00000010 */
	0x04,  /* 00000100 */
	0x08,  /* 00001000 */
	0x10,  /* 00010000 */
	0x20,  /* 00100000 */
	0x40,  /* 01000000 */
	0x80   /* 10000000 */
};

/* Implementation of AP hash function for C-strings */
/* See: http://www.partow.net/programming/hashfunctions/#APHashFunction */
static bloom_type bf_hash_ap(const char *const val, bloom_type hash)
{
	const uint8_t *it = (uint8_t *)val;

	while (*it != '\0') {
		hash ^= (hash <<  7) ^ (*it++) * (hash >> 3);
		if (*it == '\0')
			break;
		hash ^= (~((hash << 11) + ((*it++) ^ (hash >> 5))));
	}
	return hash;
}

static inline size_t get_byte_count(const size_t table_size)
{
	if (table_size % 8)
		return table_size / 8 + 1;
	return table_size / 8;
}

struct bloom_filter *bloom_filter_init(
	size_t table_size, const size_t salt_count)
{
	struct bloom_filter *ret = malloc(sizeof(struct bloom_filter));

	if (ret == NULL)
		return NULL;
	ret->bit_table = calloc(get_byte_count(table_size), sizeof(uint8_t));
	if (ret->bit_table == NULL) {
		free(ret);
		return NULL;
	}
	ret->table_size = table_size;
	ret->item_count = 0;
	if (salt_count == 0)
		ret->salt_count = 1;
	else if (salt_count > PREDEF_SALT_COUNT)
		ret->salt_count = PREDEF_SALT_COUNT;
	else
		ret->salt_count = salt_count;
	return ret;
}

void bloom_filter_free(struct bloom_filter *f)
{
	ASSERT(f != NULL);
	free(f->bit_table);
	free(f);
}

void bloom_filter_clear(struct bloom_filter *const f)
{
	ASSERT(f != NULL);
	memset(f->bit_table, 0, f->table_size / 8);
	f->item_count = 0;
}

uint8_t bloom_filter_insert(
	struct bloom_filter *const f, const char *const value)
{
	size_t salt, bit_index;
	bloom_type hash;
	uint8_t collision = 1;

	ASSERT(f != NULL);
	for (salt = 0; salt < f->salt_count; salt++) {
		hash = bf_hash_ap(value, PREDEF_SALT[salt]);
		bit_index = hash % f->table_size;
		if (!(f->bit_table[bit_index / 8] & BIT_MASK[bit_index % 8])) {
			collision = 0;
			f->bit_table[bit_index / 8] |= BIT_MASK[bit_index % 8];
		}
	}
	if (f->item_count < UINT32_MAX)
		f->item_count++;
	return collision;
}

uint8_t bloom_filter_contains(
	const struct bloom_filter *const f, const char *value)
{
	size_t salt, bit_index;
	bloom_type hash;

	ASSERT(f != NULL);
	for (salt = 0; salt < f->salt_count; salt++) {
		hash = bf_hash_ap(value, PREDEF_SALT[salt]);
		bit_index = hash % f->table_size;
		if (!(f->bit_table[bit_index / 8] & BIT_MASK[bit_index % 8]))
			return 0;
	}
	return 1;
}

struct bloom_filter *bloom_filter_import(
	const uint8_t *const bits,
	const size_t table_size, const size_t salt_count)
{
	struct bloom_filter *f = bloom_filter_init(table_size, salt_count);

	ASSERT(bits != NULL);
	if (f == NULL)
		return NULL;
	memcpy(f->bit_table, bits, get_byte_count(table_size));
	f->item_count = 1; /* XXX How to import? */
	return f;
}

uint8_t *bloom_filter_export(
	const struct bloom_filter *const f, size_t *const res_len)
{
	ASSERT(f != NULL);

	size_t length = get_byte_count(f->table_size);
	uint8_t *ret = malloc(length);

	if (ret == NULL)
		return NULL;
	memcpy(ret, f->bit_table, length);
	if (res_len != NULL)
		*res_len = length;
	return ret;
}

size_t bloom_filter_export_to(
	const struct bloom_filter *const f,
	uint8_t *const buffer, const size_t buffer_size)
{
	ASSERT(f != NULL);

	size_t length = get_byte_count(f->table_size);

	if (length > buffer_size)
		return 0;
	memcpy(buffer, f->bit_table, length);
	return length;
}

size_t bloom_filter_get_table_size(const struct bloom_filter *const f)
{
	return f->table_size;
}

size_t bloom_filter_get_byte_count(const struct bloom_filter *const f)
{
	return get_byte_count(f->table_size);
}

uint32_t bloom_filter_get_item_count(const struct bloom_filter *const f)
{
	return f->item_count;
}

void bloom_filter_set_item_count(
	struct bloom_filter *const f, const uint32_t item_count)
{
	f->item_count = item_count;
}

double bloom_filter_get_allocation(const struct bloom_filter *const f)
{
	ASSERT(f != NULL);

	double m = (double)f->table_size;
	double n = (double)f->item_count;
	double k = (double)f->salt_count;

	return pow(1 - pow(1 - (1 / m), k * n), k);
}
