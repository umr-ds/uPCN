#ifndef BUNDLE6_SERIALIZER_H_INCLUDED
#define BUNDLE6_SERIALIZER_H_INCLUDED

#include "upcn/upcn.h"
#include "upcn/bundle.h"

#include <stddef.h>

enum upcn_result bundle6_serialize(
	struct bundle *bundle,
	void (*write)(const void *cla_obj, const void *, const size_t),
	const void *cla_obj);

#endif /* BUNDLE6_SERIALIZER_H_INCLUDED */
