#ifndef BUNDLE_V7_PARSER_H_INCLUDED
#define BUNDLE_V7_PARSER_H_INCLUDED

#include <stddef.h>
#include <inttypes.h>

#include "upcn/result.h"
#include "upcn/bundle.h"


/**
 * Returns the number of bytes that will be required for the CBORepresentation
 * of the passed unsigned integer.
 */
size_t bundle7_cbor_uint_sizeof(uint64_t num);


/**
 * Converts the unified µPCN flags into BPv7-bis protocol-compliant bundle
 * processing flags.
 *
 * @return BPv7-bis bundle processing flags
 */
uint16_t bundle7_convert_to_protocol_proc_flags(const struct bundle *bundle);


/**
 * Converts the unified µPCN flags into BPv7-bis protocol-compliant block
 * processing flags.
 *
 * @return BPv7-bis bundle block processing flags
 */
uint16_t bundle7_convert_to_protocol_block_flags(
	const struct bundle_block *block);


/**
 * Returns the byte-length of the CBORepresentation of an extension block.
 */
size_t bundle7_block_get_size(struct bundle_block *block);

size_t bundle7_get_serialized_size(struct bundle *bundle);
size_t bundle7_get_serialized_size_without_payload(struct bundle *bundle);

/**
 * Recalculates the length of the primary block stored in the
 * "primary_block_length" field. You should call this function if you change
 * something in the primary block.
 */
void bundle7_recalculate_primary_block_length(struct bundle *bundle);


/**
 * Returns a pointer to the bundle payload data.
 *
 * The payload block of a BPbis bundle contains a CBOR byte string. This
 * function decodes the CBOR byte string header to get the actual payload and
 * its length.
 *
 * Note:
 *   We assume that the header is valid CBOR. No boundary or sanity
 *   checks are performed. The validity of the CBOR header must be
 *   ensured by the bundle7 parser or the bundle creator itself.
 */
uint8_t *bundle7_get_payload_data(const struct bundle *bundle, size_t *length);


/**
 * Returns the minimal number of serialized bytes of the first fragment of the
 * given bundle.
 *
 * The minimal fragment contains:
 *
 *   - all extension blocks
 *   - minimal header for the payload block
 */
size_t bundle7_get_first_fragment_min_size(struct bundle *bundle);

/**
 * Returns the minimal number of serialized bytes of the last fragment of the
 * given bundle.
 *
 * The minimal fragment contains:
 *
 *   - all extension blocks containing the "replicated in every fragment" flag
 *   - minimal header for the payload block
 */
size_t bundle7_get_last_fragment_min_size(struct bundle *bundle);

#endif // BUNDLE_V7_PARSER_H_INCLUDED
