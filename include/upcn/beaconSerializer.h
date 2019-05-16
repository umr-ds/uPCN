#ifndef BEACONSERIALIZER_H_INCLUDED
#define BEACONSERIALIZER_H_INCLUDED

#include <stdint.h>

#include "upcn/beacon.h"

uint8_t *beacon_serialize(struct beacon *beacon, uint16_t *length);

#endif /* BEACONSERIALIZER_H_INCLUDED */
