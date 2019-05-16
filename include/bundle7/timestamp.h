#ifndef BUNDLE7_TIMESTAMP_H_INCLUDED
#define BUNDLE7_TIMESTAMP_H_INCLUDED

#include <stdint.h>
#include "cbor.h"

CborError bundle7_timestamp_parse(CborValue * it,
	uint64_t *creation_timestamp,
	uint16_t *sequence_number);

#endif // BUNDLE7_TIMESTAMP_H_INCLUDED
