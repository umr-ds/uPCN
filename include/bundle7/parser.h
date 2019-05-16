#ifndef BUNDLE_V7_PARSER_H_INCLUDED
#define BUNDLE_V7_PARSER_H_INCLUDED

#include <stddef.h>
#include "cbor.h"
#include "upcn/bundle.h"  // struct bundle
#include "upcn/parser.h"  // struct parser
#include "upcn/result.h"  // enum upcn_result
#include "bundle7/crc.h"  // struct bundle7_crc_stream
#include "bundle7/eid.h"  // struct bundle7_eid_parser

// Used to define BUNDLE_QUOTA
//
// Replace this config with your own #define if you want to use
// the library outside the upcn project
#include <upcn/config.h>


enum bundle7_parser_flags {
	/**
	 * If the flag is set the parser will - after successfully parsing and
	 * CBOR element - feed the CRC calculation with the raw CBOR bytes of
	 * the parsed CBOR element.
	 */
	BUNDLE_V7_PARSER_CRC_FEED = 0x01,
};


/**
 * BPv7-bis CBOR parser using the iterator-bases CBOR-decoding from TinyCBOR
 */
struct bundle7_parser {
	struct parser *basedata;
	uint8_t flags;

	// CRC
	struct bundle7_crc_stream crc16;
	struct bundle7_crc_stream crc32;
	uint32_t checksum;

	/**
	 * Counts the number of CBOR bytes parsed for this bundle. This field
	 * is used to enforce the bundle quota (max. size of bundles)
	 */
	size_t bundle_size;
	struct bundle *bundle;

	/**
	 * Parsing callbacks
	 *
	 * Each callback can advance this function to pointer to transition
	 * into the next parsing stage.
	 */
	CborError(*parse)(struct bundle7_parser *, CborValue *);
	CborError(*next)(struct bundle7_parser *, CborValue *);

	/**
	 * Callback after a bundle gets successfully parsed. The only passed
	 * argument is the bundle itself.
	 */
	void (*send_callback)(struct bundle *);

	struct bundle_block_list **current_block_entry;
};


struct parser *bundle7_parser_init(
	struct bundle7_parser *state,
	void (*send_callback)(struct bundle *)
);


size_t bundle7_parser_read(struct bundle7_parser *parser,
	const uint8_t *buffer, size_t length);

enum upcn_result bundle7_parser_reset(struct bundle7_parser *parser);


#endif /* BUNDLE_V7_PARSER_H_INCLUDED */
