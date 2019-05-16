#include <stdio.h>
#include <unity_fixture.h>

#include "bundle7/crc.h"

TEST_GROUP(crc);

TEST_SETUP(crc)
{
}

TEST_TEAR_DOWN(crc)
{
}

TEST(crc, crc16)
{
	char *stream = "Some really long message";
	struct bundle7_crc_stream crc;

	TEST_ASSERT_EQUAL_UINT16(0x0000, bundle7_crc16("", 0));
	TEST_ASSERT_EQUAL_UINT16(0x1238, bundle7_crc16("asdf", 4));
	TEST_ASSERT_EQUAL_UINT16(0xa15c, bundle7_crc16("message", 7));
	TEST_ASSERT_EQUAL_UINT16(0xffce, bundle7_crc16(stream, 24));

	// Test CRC streamed calculation
	bundle7_crc_init(&crc, BUNDLE_V7_CRC16);

	// read data stream
	for (int i = 0; stream[i] != 0; i++)
		bundle7_crc_read(&crc, stream[i]);

	// finish CRC calculation
	bundle7_crc_done(&crc);

	TEST_ASSERT_EQUAL_UINT32(0xffce, crc.checksum);
}

TEST(crc, crc32)
{
	char *stream = "Some really long message";
	struct bundle7_crc_stream crc;

	TEST_ASSERT_EQUAL_UINT32(0x00000000, bundle7_crc32("", 0));
	TEST_ASSERT_EQUAL_UINT32(0x5129f3bd, bundle7_crc32("asdf", 4));
	TEST_ASSERT_EQUAL_UINT32(0xb6bd307f, bundle7_crc32("message", 7));
	TEST_ASSERT_EQUAL_UINT32(0xc9b2c668, bundle7_crc32(stream, 24));

	// Test CRC streamed calculation
	bundle7_crc_init(&crc, BUNDLE_V7_CRC32);

	// read data stream
	for (int i = 0; stream[i] != 0; i++)
		bundle7_crc_read(&crc, stream[i]);

	// finish CRC calculation
	bundle7_crc_done(&crc);

	TEST_ASSERT_EQUAL_UINT32(0xc9b2c668, crc.checksum);
}

TEST_GROUP_RUNNER(crc)
{
	RUN_TEST_CASE(crc, crc16);
	RUN_TEST_CASE(crc, crc32);
}
