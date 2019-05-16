#ifndef BEACON_H_INCLUDED
#define BEACON_H_INCLUDED

#include <stdint.h>

#include "upcn/upcn.h"

#define TLV_HAS_LENGTH(tag) (tag >= TLV_TYPE_STRING)
#define TLV_IS_PRIMITIVE(tag) (tag < 64)

enum tlv_type {
	/* Primitive types */
	TLV_TYPE_BOOLEAN = 0,
	TLV_TYPE_UINT64,
	TLV_TYPE_SINT64,
	TLV_TYPE_FIXED16,
	TLV_TYPE_FIXED32,
	TLV_TYPE_FIXED64,
	TLV_TYPE_FLOAT,
	TLV_TYPE_DOUBLE,
	TLV_TYPE_STRING,
	TLV_TYPE_BYTES,
	/* Constructed types */
	TLV_TYPE_CLA_TCP_V4 = 64,
	TLV_TYPE_CLA_UDP_V4,
	TLV_TYPE_CLA_TCP_V6,
	TLV_TYPE_CLA_UDP_V6,
	TLV_TYPE_CLA_TCP_HN,
	TLV_TYPE_CLA_UDP_HN,
	/* NBF */
	TLV_TYPE_NBF_HASHES = 126,
	TLV_TYPE_NBF_BITS,
	/* Private (some proposals) */
	TLV_TYPE_PRIVATE_CLA_AX25 = 128,      /* similar to other CLA types */
	TLV_TYPE_PRIVATE_HAS_INTERNET_ACCESS, /* contains: BOOLEAN */
	TLV_TYPE_PRIVATE_NEIGHBOR_EIDS,       /* contains: SDNV (count) + BIN */
	/* TRANSMISSION AND RECEIVING BITRATE, TX = MSBs, RX = LSBs */
	TLV_TYPE_PRIVATE_TX_RX_BITRATE,       /* contains: FIXED32 */
	TLV_TYPE_PRIVATE_RRND_FLAGS,          /* contains: FIXED16 */
	TLV_TYPE_PRIVATE_RRND_AVAILABILITY,   /* contains: SDNV */
	TLV_TYPE_PRIVATE_COOKIES = 160,       /* contains: [ FIXED32 ++ BIN ] */
	TLV_TYPE_PRIVATE_PKI_SIGNATURE = 192, /* contains: BYTES */
	TLV_TYPE_PRIVATE_PKI_CERTIFICATE      /* contains: BYTES */
};

struct tlv_definition {
	enum tlv_type tag;
	uint16_t length;
	union tlv_value {
		/* Primitive types */
		uint8_t u8;
		int8_t s8;
		uint16_t u16;
		int16_t s16;
		uint32_t u32;
		int32_t s32;
		uint64_t u64;
		int64_t s64;
		float f;
		double d;
		char *s;
		uint8_t *b;
		/* Constructed types have a children struct as value */
		struct tlv_children {
			uint16_t count;
			uint16_t _unused;
			struct tlv_definition *list;
		} children;
	} value;
};

enum beacon_flags {
	BEACON_FLAG_NONE         = 0x00,
	BEACON_FLAG_HAS_EID      = 0x01,
	BEACON_FLAG_HAS_SERVICES = 0x02,
	BEACON_FLAG_HAS_NBF      = 0x04,
	BEACON_FLAG_HAS_PERIOD   = 0x08
};

/* TODO: Check draft-irtf-dtnrg-ipnd-03 */
/* A basic representation of an IPND (draft-irtf-dtnrg-ipnd-02) beacon */
struct beacon {
	uint8_t version;
	enum beacon_flags flags;
	uint16_t sequence_number;
	char *eid;
	/* The (optional) service block. */
	uint16_t service_count;
	struct tlv_definition *services;
	/* The (optional) beacon period. Will be -1 if not defined. */
	int32_t period;
};

struct beacon *beacon_init(void);
void beacon_free(struct beacon *beacon);
void tlv_free(struct tlv_definition *tlv);

#define tlv_populate_pt(tlv_list, child, tag, value) \
	tlv_populate(tlv_list, child, tag, 0, value)
enum upcn_result tlv_populate(struct tlv_definition *const tlv_list,
	const uint8_t child, const enum tlv_type tag, const uint16_t length,
	const union tlv_value value);
struct tlv_definition *tlv_populate_ct(struct tlv_definition *const tlv_list,
	const uint8_t child, const enum tlv_type tag, const uint8_t children);

struct tlv_definition *beacon_get_service(
	const struct beacon *const beacon, const enum tlv_type tag);
struct tlv_definition *tlv_get_child(
	const struct tlv_definition *const tlv, const enum tlv_type tag);

#endif /* BEACON_H_INCLUDED */
