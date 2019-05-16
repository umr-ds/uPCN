#ifndef BUNDLE6_EID_H_INCLUDED
#define BUNDLE6_EID_H_INCLUDED

#include <stddef.h>  // size_t
#include <stdbool.h> // bool
#include "upcn/bundle.h"  // struct bundle


// ------------------------------------
// RFC 5050 Endpoint Identifiers (EIDs)
// ------------------------------------

char *bundle6_read_eid(struct bundle *bundle, struct eid_reference eidref);

bool bundle6_eid_equals(const struct bundle *bundle,
	const struct eid_reference ref,	const char *scheme, const char *ssp);

bool bundle6_eid_equals_string(const struct bundle *bundle,
	const struct eid_reference ref, const char *eid, size_t eid_len);


#endif /* BUNDLE6_EID_H_INCLUDED */
