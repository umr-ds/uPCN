#include <unity.h>
#include <unity_fixture.h>

#include "upcn/upcn.h"
#include "upcn/bloomFilter.h"

TEST_GROUP(bloomFilter);

#define TABLE_SIZE 64
#define SALT_COUNT 8

#define ITER 10000
#define TEST_STRING "BFTEST123"

static struct bloom_filter *bf;

struct testval {
	uint32_t rand;
	uint8_t null;
};

TEST_SETUP(bloomFilter)
{
	bf = bloom_filter_init(TABLE_SIZE, SALT_COUNT);
}

TEST_TEAR_DOWN(bloomFilter)
{
	bloom_filter_free(bf);
}

TEST(bloomFilter, bloomFilter_test)
{
	struct testval t = { .rand = 0, .null = 0 };
	int i;

	TEST_ASSERT_FALSE(bloom_filter_contains(bf, TEST_STRING));
	for (i = 0; i < ITER; i++, t.rand = hal_random_get())
		TEST_ASSERT_FALSE(bloom_filter_contains(bf, (char *)&t));
	TEST_ASSERT_FALSE(bloom_filter_insert(bf, TEST_STRING));
	TEST_ASSERT_TRUE(bloom_filter_contains(bf, TEST_STRING));
	bloom_filter_clear(bf);
	TEST_ASSERT_FALSE(bloom_filter_contains(bf, TEST_STRING));
	TEST_ASSERT_FALSE(bloom_filter_insert(bf, TEST_STRING));
	TEST_ASSERT_TRUE(bloom_filter_contains(bf, TEST_STRING));
	TEST_ASSERT_TRUE(bloom_filter_insert(bf, TEST_STRING));
	for (i = 0; i < ITER; i++, t.rand = hal_random_get())
		TEST_ASSERT_FALSE(bloom_filter_contains(bf, (char *)&t));
}

TEST(bloomFilter, bloomFilter_export)
{
	uint8_t *buf;
	size_t len;
	struct bloom_filter *bf2;

	TEST_ASSERT_FALSE(bloom_filter_insert(bf, TEST_STRING));
	buf = bloom_filter_export(bf, &len);
	TEST_ASSERT_NOT_NULL(buf);
	TEST_ASSERT_EQUAL(TABLE_SIZE / 8, len);
	bf2 = bloom_filter_import(buf, TABLE_SIZE, SALT_COUNT);
	free(buf);
	TEST_ASSERT_NOT_NULL(bf2);
	TEST_ASSERT_EQUAL_UINT8_ARRAY(bf->bit_table, bf2->bit_table, len);
	bloom_filter_free(bf2);
}

TEST_GROUP_RUNNER(bloomFilter)
{
	RUN_TEST_CASE(bloomFilter, bloomFilter_test);
	RUN_TEST_CASE(bloomFilter, bloomFilter_export);
}
