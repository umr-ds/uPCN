#ifndef BEACONPROCESSOR_H_INCLUDED
#define BEACONPROCESSOR_H_INCLUDED

#include <stdint.h>

#include "upcn/upcn.h"
#include "upcn/beacon.h"
#include "upcn/groundStation.h"
#include "upcn/rrnd.h"

enum processed_beacon_origin {
	BEACON_ORIGIN_UNKNOWN,
	BEACON_ORIGIN_IN_DISCOVERY,
	BEACON_ORIGIN_KNOWN_GS
};

struct processed_beacon_info {
	enum processed_beacon_origin origin;
	void *handle;
};

enum upcn_result beacon_processor_init(const uint8_t *const secret);
struct processed_beacon_info beacon_processor_process(
	struct beacon *beacon, Semaphore_t gs_semaphore,
	QueueIdentifier_t router_signaling_queue);

#endif /* BEACONPROCESSOR_H_INCLUDED */
