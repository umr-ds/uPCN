/*
 * This bloom filter implementation is a C-port of the one
 * found in IBR-DTN (https://www.ibr.cs.tu-bs.de/projects/ibr-dtn/)
 * written by Johannes Morgenroth, which is based on
 * Open Bloom Filter (http://www.partow.net) written by Arash Partow.
 */

#ifndef BLOOMFILTER_H_INCLUDED
#define BLOOMFILTER_H_INCLUDED

#include <stddef.h>
#include <stdint.h>

#define BLOOM_FILTER_HASH_BASE_ID 0x80

struct bloom_filter {
	uint8_t *bit_table;
	size_t table_size;
	uint32_t item_count;
	size_t salt_count;
};

struct bloom_filter *bloom_filter_init(
	size_t table_size, const size_t salt_count);
void bloom_filter_free(struct bloom_filter *f);
void bloom_filter_clear(struct bloom_filter *const f);
uint8_t bloom_filter_insert(
	struct bloom_filter *const f, const char *const value);
uint8_t bloom_filter_contains(
	const struct bloom_filter *const f, const char *const value);

struct bloom_filter *bloom_filter_import(
	const uint8_t *const bits,
	const size_t table_size, const size_t salt_count);
uint8_t *bloom_filter_export(
	const struct bloom_filter *const f, size_t *const res_len);
size_t bloom_filter_export_to(
	const struct bloom_filter *const f,
	uint8_t *const buffer, const size_t buffer_size);

size_t bloom_filter_get_table_size(const struct bloom_filter *const f);
size_t bloom_filter_get_byte_count(const struct bloom_filter *const f);
uint32_t bloom_filter_get_item_count(const struct bloom_filter *const f);
void bloom_filter_set_item_count(
	struct bloom_filter *const f, const uint32_t item_count);

double bloom_filter_get_allocation(const struct bloom_filter *const f);

#endif /* BLOOMFILTER_H_INCLUDED */
