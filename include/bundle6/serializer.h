#ifndef BUNDLE6_SERIALIZER_H_INCLUDED
#define BUNDLE6_SERIALIZER_H_INCLUDED

#include "upcn/bundle.h"
#include "upcn/result.h"

#include <stddef.h>

enum upcn_result bundle6_serialize(
	struct bundle *bundle,
	void (*write)(void *cla_obj, const void *, const size_t),
	void *cla_obj);

#endif /* BUNDLE6_SERIALIZER_H_INCLUDED */
