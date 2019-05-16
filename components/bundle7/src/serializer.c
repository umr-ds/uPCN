#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <inttypes.h>
#include "cbor.h"
#include "compilersupport_p.h" // Private TinyCBOR header, used for endianess
#include "bundle7/serializer.h"
#include "bundle7/eid.h"
#include "bundle7/allocators.h"


/* CBOR output buffer size. This buffer will be allocated during the
 * serialization process and released afterwards
 */
#define BUFFER_SIZE 128


static inline size_t primary_block_get_item_count(struct bundle *bundle)
{
	size_t length = 8;

	if (bundle->crc_type != BUNDLE_CRC_TYPE_NONE)
		length++;

	if (bundle_is_fragmented(bundle))
		length++;

	return length;
}


static inline size_t block_get_item_count(const struct bundle_block *block)
{
	size_t length = 6;

	if (block->crc_type != BUNDLE_CRC_TYPE_NONE)
		length++;

	return length;
}


static CborError write_crc(
	struct CborEncoder *encoder,
	enum bundle_crc_type crc_type, struct bundle7_crc_stream *crc)
{
	// CRC-32
	if (crc_type == BUNDLE_CRC_TYPE_32) {
		// Feed the "zero" CRC checksum
		bundle7_crc_read(crc, 0x44);
		bundle7_crc_read(crc, 0x00);
		bundle7_crc_read(crc, 0x00);
		bundle7_crc_read(crc, 0x00);
		bundle7_crc_read(crc, 0x00);
		bundle7_crc_done(crc);

		// Swap to network byte order
		crc->checksum = cbor_htonl(crc->checksum);

		return cbor_encode_byte_string(encoder, crc->bytes, 4);
	}
	// CRC-16
	else {
		// Feed the "zero" CRC checksum
		bundle7_crc_read(crc, 0x42);
		bundle7_crc_read(crc, 0x00);
		bundle7_crc_read(crc, 0x00);
		bundle7_crc_done(crc);

		// Swap to network byte order
		crc->checksum = cbor_htons(crc->checksum);

		// We skip the first two (empty) bytes of the checksum here
		return cbor_encode_byte_string(encoder, crc->bytes, 2);
	}
}


/*
 * Number of bytes required for CBOR-encoded max. value:
 *
 * Primary Block
 * -------------
 *
 *
 * Extension Block (without payload):
 * ----------------------------------
 *
 * Array header:                   1 Byte
 * Block type code (255):          2 Byte
 * Block number (255):             2 Byte
 * Block processing control flags: 1 Byte
 * CRC (32 bit):                   5 Byte
 * Block data length:              5 Byte
 */


static inline void init_crc(struct bundle7_crc_stream *crc,
	enum bundle_crc_type type)
{
	if (type == BUNDLE_CRC_TYPE_32)
		bundle7_crc_init(crc, BUNDLE_V7_CRC32);
	else if (type == BUNDLE_CRC_TYPE_16)
		bundle7_crc_init(crc, BUNDLE_V7_CRC16);
}


static inline void feed_crc(struct bundle7_crc_stream *crc,
	enum bundle_crc_type type, uint8_t *buffer, size_t length)
{
	if (type != BUNDLE_CRC_TYPE_NONE)
		for (size_t i = 0; i < length; ++i)
			bundle7_crc_read(crc, buffer[i]);
}


enum upcn_result bundle7_serialize(struct bundle *bundle,
	void (*write)(const void *cla_obj, const void *, const size_t),
	const void *cla_obj)
{
	// Assert that the bundle has correct version
	if (bundle->protocol_version != 7)
		return UPCN_FAIL;

	uint8_t *buffer;
	CborEncoder encoder;
	struct bundle7_crc_stream crc;
	int written;

	buffer = bundle7_malloc(BUFFER_SIZE);
	if (buffer == NULL)
		return UPCN_FAIL;

	// Bundle start (CBOR indefinite array)
	buffer[0] = 0x9f;

	// -------------
	// Primary Block
	// -------------

	buffer[1] = 0x80 + primary_block_get_item_count(bundle);

	init_crc(&crc, bundle->crc_type);

	cbor_encoder_init(&encoder, buffer + 2, BUFFER_SIZE - 2, 0);
	cbor_encode_uint(&encoder, bundle->protocol_version);
	cbor_encode_uint(&encoder,
		bundle7_convert_to_protocol_proc_flags(bundle));
	cbor_encode_uint(&encoder, bundle->crc_type);

	write(cla_obj, buffer, cbor_encoder_get_buffer_size(&encoder, buffer));
	feed_crc(&crc, bundle->crc_type, buffer + 1,
		cbor_encoder_get_buffer_size(&encoder, buffer + 1));

	// Destination EID
	written = bundle7_eid_serialize(bundle->destination,
		buffer, BUFFER_SIZE);
	if (written <= 0)
		return UPCN_FAIL;
	write(cla_obj, buffer, written);
	feed_crc(&crc, bundle->crc_type, buffer, written);

	// Source EID
	written = bundle7_eid_serialize(bundle->source,
		buffer, BUFFER_SIZE);
	if (written <= 0)
		return UPCN_FAIL;
	write(cla_obj, buffer, written);
	feed_crc(&crc, bundle->crc_type, buffer, written);

	// Report-To EID
	written = bundle7_eid_serialize(bundle->report_to,
		buffer, BUFFER_SIZE);
	if (written <= 0)
		return UPCN_FAIL;
	write(cla_obj, buffer, written);
	feed_crc(&crc, bundle->crc_type, buffer, written);

	// Creation Timestamp
	buffer[0] = 0x82;
	cbor_encoder_init(&encoder, buffer + 1, BUFFER_SIZE - 1, 0);
	cbor_encode_uint(&encoder, bundle->creation_timestamp);
	cbor_encode_uint(&encoder, bundle->sequence_number);
	cbor_encode_uint(&encoder, bundle->lifetime);

	if (bundle_is_fragmented(bundle))
		cbor_encode_uint(&encoder, bundle->fragment_offset);

	// CRC checksum for primary block
	if (bundle->crc_type != BUNDLE_CRC_TYPE_NONE) {
		feed_crc(&crc, bundle->crc_type, buffer,
			cbor_encoder_get_buffer_size(&encoder, buffer));

		// Calculate and encode CRC checksum for primary block.
		write_crc(&encoder, bundle->crc_type, &crc);
	}

	write(cla_obj, buffer, cbor_encoder_get_buffer_size(&encoder, buffer));

	// ----------------
	// Extension Blocks
	// ----------------

	struct bundle_block_list *cur_block = bundle->blocks;

	while (cur_block != NULL) {
		const struct bundle_block *block = cur_block->data;

		init_crc(&crc, block->crc_type);

		// CBOR array header with embedded number of items
		buffer[0] = 0x80 + block_get_item_count(block);

		cbor_encoder_init(&encoder, buffer + 1, BUFFER_SIZE - 1, 0);
		cbor_encode_uint(&encoder, block->type);
		cbor_encode_uint(&encoder, block->number);
		cbor_encode_uint(&encoder,
			bundle7_convert_to_protocol_block_flags(
				block));
		cbor_encode_uint(&encoder, block->crc_type);
		cbor_encode_uint(&encoder, block->length);

		write(cla_obj, buffer, cbor_encoder_get_buffer_size(&encoder,
								    buffer));
		feed_crc(&crc, block->crc_type, buffer,
			 cbor_encoder_get_buffer_size(&encoder, buffer));

		write(cla_obj, block->data, block->length);
		feed_crc(&crc, block->crc_type,
			 block->data, block->length);

		if (block->crc_type != BUNDLE_CRC_TYPE_NONE) {
			// Reset CBOR encoder
			cbor_encoder_init(&encoder, buffer, BUFFER_SIZE, 0);

			// Calculate and CRC checksum for extension block
			write_crc(&encoder, block->crc_type, &crc);

			write(cla_obj, buffer,
			      cbor_encoder_get_buffer_size(&encoder, buffer));
		}

		cur_block = cur_block->next;
	}

	// CBOR "break"
	buffer[0] = 0xff;
	write(cla_obj, buffer, 1);

	bundle7_free(buffer);

	return UPCN_OK;
}
