#ifndef BEACONPARSER_H_INCLUDED
#define BEACONPARSER_H_INCLUDED

#include <stdint.h>

#include "upcn/sdnv.h"
#include "upcn/beacon.h"
#include "upcn/parser.h"

enum beacon_parser_stage {
	BCP_EXPECT_VERSION,
	BCP_EXPECT_FLAGS,
	BCP_EXPECT_SEQUENCE_NUMBER_MSB,
	BCP_EXPECT_SEQUENCE_NUMBER_LSB,
	BCP_EXPECT_EID_LENGTH,
	BCP_EXPECT_EID,
	BCP_EXPECT_SERVICENUM,
	BCP_EXPECT_TLV_TAG,
	BCP_EXPECT_TLV_LENGTH,
	BCP_EXPECT_TLV_VALUE,
	BCP_EXPECT_PERIOD
};

struct tlv_stack_frame {
	struct tlv_definition *tlv;
	uint16_t bytes_value;
	uint8_t bytes_header;
};

struct beacon_parser {
	struct parser *basedata;
	void (*send_callback)(struct beacon *);
	enum beacon_parser_stage stage;
	uint16_t index;
	uint16_t remaining;
	struct sdnv_state sdnv_state;
	union {
		uint16_t *sdnv_u16;
		uint32_t *sdnv_u32;
		uint64_t *sdnv_u64;
		char *string;
		uint8_t *bytes;
	} cur;
	int16_t tlv_index;
	struct tlv_stack_frame tlv_stack[TLV_MAX_DEPTH];
	struct tlv_stack_frame *tlv_cur;
	struct beacon *beacon;
};

struct parser *beacon_parser_init(
	struct beacon_parser *parser, void (*send_callback)(struct beacon *));
enum upcn_result beacon_parser_reset(struct beacon_parser *parser);
size_t beacon_parser_read(struct beacon_parser *parser,
	const uint8_t *buffer, size_t length);

#endif /* BEACONPARSER_H_INCLUDED */
