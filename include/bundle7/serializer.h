#ifndef BUNDLE_V7_SERIALIZER_H_INCLUDED
#define BUNDLE_V7_SERIALIZER_H_INCLUDED

#include <stdbool.h>
#include "upcn/bundle.h"
#include "upcn/result.h"
#include "bundle7/crc.h"
#include "bundle7/bundle7.h"


enum bundle7_serializer_status {
	BUNDLE_V7_SERIALIZER_STATUS_RUNNING,
	BUNDLE_V7_SERIALIZER_STATUS_FINISHED,
	BUNDLE_V7_SERIALIZER_STATUS_ERROR
};


struct bundle7_serializer {
	enum bundle7_serializer_status status;

	/*
	 * Output buffer
	 *
	 * CBOR encoded data is written into this buffer.
	 */
	size_t filled;
	size_t written;
	uint8_t *buffer;

	/*
	 * If raw bytes should be written directory to the output without
	 * copying them into the output buffer, this pointer and size are
	 * used to notify the serializer to use the raw bytes directly.
	 */
	uint8_t *raw;
	size_t raw_bytes;

	/*
	 * CRC stream calculator
	 *
	 * "skip_crc" is a flag telling the serializer to jump over the
	 * currently written bytes and not using them in the CRC calculus
	 */
	struct bundle7_crc_stream crc;
	bool skip_crc;

	struct bundle_block_list *block_element;

	// Callbacks
	void (*callback)(struct bundle7_serializer *, struct bundle *);
	void (*next_callback)(struct bundle7_serializer *, struct bundle *);
};



/**
 * Creates CBOR-encoded byte stream of a Bundle v7
 */
enum upcn_result bundle7_serialize(struct bundle *bundle,
	void (*write)(const void *cla_obj, const void *, const size_t),
	const void *cla_obj);


#endif /* BUNDLE_V7_SERIALIZER_H_INCLUDED */
