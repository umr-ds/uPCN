#ifndef BUNDLE6_BUNDLE6_H_INCLUDED
#define BUNDLE6_BUNDLE6_H_INCLUDED

#include "upcn/bundle.h"

/**
 * Returns a pointer to the bundle payload data.
 */
uint8_t *bundle6_get_payload_data(const struct bundle *bundle, size_t *length);

// ----------------------
// Bundle and block sizes
// ----------------------

void bundle6_recalculate_header_length(struct bundle *bundle);
size_t bundle6_block_get_size(struct bundle_block *block);
size_t bundle6_get_serialized_size(struct bundle *bundle);
size_t bundle6_get_serialized_size_without_payload(struct bundle *bundle);


// -------------------
// Fragmentation sizes
// -------------------

size_t bundle6_get_first_fragment_min_size(struct bundle *bundle);
size_t bundle6_get_mid_fragment_min_size(struct bundle *bundle);
size_t bundle6_get_last_fragment_min_size(struct bundle *bundle);


// -------------------
// Bundle manipulation
// -------------------

enum upcn_result bundle6_set_current_custodian(struct bundle *bundle,
	const char *schema, const char *ssp);


#endif /* BUNDLE6_BUNDLE6_H_INCLUDED */
