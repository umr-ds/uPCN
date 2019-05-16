#include <stdint.h>
#include <stdlib.h>
#include <stddef.h>

#include <unity.h>
#include <unity_fixture.h>

#include "upcn/upcn.h"
#include "upcn/beacon.h"
#include "upcn/beaconParser.h"
#include "upcn/beaconSerializer.h"

TEST_GROUP(beacon);

TEST_SETUP(beacon)
{
}

TEST_TEAR_DOWN(beacon)
{
}

TEST(beacon, short_beacon_parse_serialize)
{
	const char test_beacon[] = {
		/* Version 4, EID + SVC + Period, SN 0x1044, EID "AB", 1 SVC */
		0x04, 0x0B, 0x10, 0x44, 0x02, 'A', 'B', 0x01,
		/* TLV */
		96, 14, 3, 0, 5, 97, 6, 1, 22, 9, 2, 'A', 'B', 3, 0, 2,
		/* Period = 1 min */
		60
	};
	struct beacon_parser bp;
	struct parser *p = beacon_parser_init(&bp, NULL);
	struct beacon *b;
	struct tlv_definition *s1, *s2;
	uint16_t length;
	uint8_t *serialized;

	TEST_ASSERT_NOT_NULL(p);
	beacon_parser_read(&bp, (uint8_t *) test_beacon, sizeof(test_beacon));
	TEST_ASSERT_EQUAL_INT(PARSER_STATUS_DONE, p->status);
	b = bp.beacon;
	/* Basic values */
	TEST_ASSERT_EQUAL_INT(0x04, b->version);
	TEST_ASSERT_EQUAL_INT(0x0B, b->flags);
	TEST_ASSERT_EQUAL_UINT16(0x1044, b->sequence_number);
	TEST_ASSERT_EQUAL_STRING("AB", b->eid);
	TEST_ASSERT_EQUAL_UINT16(1, b->service_count);
	TEST_ASSERT_EQUAL_INT(60, b->period);
	/* TLV structure */
	s1 = &b->services[0];
	TEST_ASSERT_EQUAL_INT(96, s1->tag);
	TEST_ASSERT_EQUAL_UINT16(14, s1->length);
	TEST_ASSERT_EQUAL_UINT16(3, s1->value.children.count);
	TEST_ASSERT_EQUAL_UINT16(0, s1->value.children._unused);
	/* simple children */
	TEST_ASSERT_EQUAL_INT(3, s1->value.children.list[0].tag);
	TEST_ASSERT_EQUAL_UINT16(0, s1->value.children.list[0].length);
	TEST_ASSERT_EQUAL_UINT16(5, s1->value.children.list[0].value.u16);
	TEST_ASSERT_EQUAL_INT(3, s1->value.children.list[2].tag);
	TEST_ASSERT_EQUAL_UINT16(0, s1->value.children.list[2].length);
	TEST_ASSERT_EQUAL_UINT16(2, s1->value.children.list[2].value.u16);
	/* complex child */
	s2 = &s1->value.children.list[1];
	TEST_ASSERT_EQUAL_INT(97, s2->tag);
	TEST_ASSERT_EQUAL_UINT16(6, s2->length);
	TEST_ASSERT_EQUAL_UINT16(2, s2->value.children.count);
	TEST_ASSERT_EQUAL_UINT16(0, s2->value.children._unused);
	TEST_ASSERT_EQUAL_INT(1, s2->value.children.list[0].tag);
	TEST_ASSERT_EQUAL_UINT16(0, s2->value.children.list[0].length);
	TEST_ASSERT_EQUAL_UINT64(22, s2->value.children.list[0].value.u64);
	TEST_ASSERT_EQUAL_INT(9, s2->value.children.list[1].tag);
	TEST_ASSERT_EQUAL_UINT16(2, s2->value.children.list[1].length);
	TEST_ASSERT_EQUAL_UINT8('A', s2->value.children.list[1].value.s[0]);
	TEST_ASSERT_EQUAL_UINT8('B', s2->value.children.list[1].value.s[1]);
	/* serialize */
	length = 0;
	serialized = beacon_serialize(b, &length);
	TEST_ASSERT_NOT_NULL(serialized);
	TEST_ASSERT_EQUAL_INT(sizeof(test_beacon), length);
	TEST_ASSERT_EQUAL_UINT8_ARRAY(test_beacon, serialized, length);
	free(serialized);
	beacon_free(b);
	free(p);
}

TEST_GROUP_RUNNER(beacon)
{
	RUN_TEST_CASE(beacon, short_beacon_parse_serialize);
}
