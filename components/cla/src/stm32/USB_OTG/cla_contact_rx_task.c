/* FIXME
 *    Without this line newlib's stdlib.h included fom libcbor raises an error
 */
#include <stdlib.h>
#include <string.h>  /* memmove() */

#include "bundle6/parser.h"
#include "bundle7/parser.h"
#include "upcn/inputParser.h"
#include "upcn/routerParser.h"
#include "upcn/beaconParser.h"
#include "upcn/rrndParser.h"
#include "upcn/rrndCommand.h"
#include "upcn/bundleStorageManager.h"
#include "upcn/bundleProcessor.h"

#include <cla_contact_rx_task.h>

#include <hal_io.h>
#include <hal_config.h>
#include <cla_io.h>
#include "upcn/init.h"

#ifdef THROUGHPUT_TEST
extern uint64_t timestamp_initialread;
uint64_t timestamp_mp1[47];
uint32_t bundle_size[47];
#endif

static struct parser *cur_parser;

static struct input_parser input_parser;
static struct router_parser router_parser;
static struct bundle6_parser bundle6_parser;
static struct bundle7_parser bundle7_parser;
static struct beacon_parser beacon_parser;
static struct rrnd_parser rrnd_parser;

static QueueIdentifier_t router_signaling_queue;
static QueueIdentifier_t bundle_signaling_queue;

/**
 * Size of the input buffer in bytes
 */
#define BUFFER_SIZE 64

static struct {
	uint8_t start[BUFFER_SIZE];
	uint8_t *end;
} input_buffer;


static void reset_parser(void)
{
#ifdef THROUGHPUT_TEST
	timestamp_initialread = 0;
#endif
	input_parser_reset(&input_parser);
	cur_parser = input_parser.basedata;
	ASSERT(router_parser_reset(&router_parser) == UPCN_OK);
	ASSERT(bundle6_parser_reset(&bundle6_parser) == UPCN_OK);
	ASSERT(bundle7_parser_reset(&bundle7_parser) == UPCN_OK);
	ASSERT(beacon_parser_reset(&beacon_parser) == UPCN_OK);
	ASSERT(rrnd_parser_reset(&rrnd_parser) == UPCN_OK);
}

static size_t select_bundle_parser_version(const uint8_t *buffer, size_t length)
{
	/* Empty buffers cannot be parsed */
	if (length == 0) {
		reset_parser();
		return 0;
	}

	switch (buffer[0]) {
	/* Bundle Protocol v6 (RFC 5050) */
	case 6:
		input_parser.type = INPUT_TYPE_BUNDLE_V6;
		cur_parser = bundle6_parser.basedata;
		return bundle6_parser_read(&bundle6_parser,
				buffer, length);
	/* CBOR indefinite array -> Bundle Protocol v7 */
	case 0x9f:
		input_parser.type = INPUT_TYPE_BUNDLE_V7;
		cur_parser = bundle7_parser.basedata;
		return bundle7_parser_read(&bundle7_parser,
				buffer, length);
	/* Unknown Bundle Protocol version, keep buffer */
	default:
		reset_parser();
		return 0;
	}
}

static size_t forward_to_specific_parser(const uint8_t *buffer, size_t length)
{
	/* Current parser is input_parser */
	if (input_parser.stage != INPUT_EXPECT_DATA)
		return input_parser_read(&input_parser, buffer, length);

	switch (input_parser.type) {
	case INPUT_TYPE_ROUTER_COMMAND_DATA:
		cur_parser = router_parser.basedata;
		return router_parser_read(&router_parser, buffer, length);
	case INPUT_TYPE_BUNDLE_VERSION:
		return select_bundle_parser_version(buffer, length);
	case INPUT_TYPE_BUNDLE_V6:
		cur_parser = bundle6_parser.basedata;
		return bundle6_parser_read(&bundle6_parser, buffer, length);
	case INPUT_TYPE_BUNDLE_V7:
		cur_parser = bundle7_parser.basedata;
		return bundle7_parser_read(&bundle7_parser, buffer, length);
	case INPUT_TYPE_BEACON_DATA:
		cur_parser = beacon_parser.basedata;
		return beacon_parser_read(&beacon_parser, buffer, length);
	case INPUT_TYPE_RRND_DATA:
		cur_parser = rrnd_parser.basedata;
		return rrnd_parser_read(&rrnd_parser, buffer, length);
	/* Unknown parser type, keep buffer and reset all parsers */
	default:
		reset_parser();
		return 0;
	}
}

static void router_command_send(struct router_command *cmd)
{
	struct router_signal signal = {
		.type = ROUTER_SIGNAL_PROCESS_COMMAND,
		.data = cmd
	};

	ASSERT(cmd != NULL);
	hal_queue_push_to_back(router_signaling_queue, &signal);
}

static void bundle_send(struct bundle *bundle)
{
#ifdef THROUGHPUT_TEST
	timestamp_mp1[bundle->sequence_number] = timestamp_initialread;
	bundle_size[bundle->sequence_number] = bundle->payload_block->length;
	timestamp_initialread = 0;
#endif
	bundleid_t new_id;

	ASSERT(bundle != NULL);

	new_id = bundle_storage_add(bundle);
	if (new_id != BUNDLE_INVALID_ID)
		bundle_processor_inform(
				bundle_signaling_queue,
				new_id,
				BP_SIGNAL_BUNDLE_INCOMING,
				BUNDLE_SR_REASON_NO_INFO);
	else
		bundle_free(bundle);
}

static void beacon_send(struct beacon *beacon)
{
	struct router_signal signal = {
		.type = ROUTER_SIGNAL_PROCESS_BEACON,
		.data = beacon
	};

	ASSERT(beacon != NULL);
	hal_queue_push_to_back(router_signaling_queue, &signal);
}



/**
 * If a "bulk read" operation is requested, this gets handled by the input
 * processor directly. A preallocated byte buffer and the requested length have
 * to be passed in the parser base data structure. After successful completion
 * of the bulk read operation the parser gets called again with an empty input
 * buffer (NULL pointer) and the bulk read flag cleared to trigger further
 * processing.
 *
 * Bytes in the current input buffer are considered and copied appropriatly --
 * meaning that non-parsed input bytes are copied into the bulk read buffer and
 * the remaining bytes are read directly from the input stream.
 *
 * @return pointer to the position up to the input buffer is consumed after the
 *         operation
 */
static uint8_t *bulk_read(void)
{
	uint8_t *parsed = input_buffer.start + cur_parser->next_bytes;

	/*
	 * Bulk read operation requested that is smaller than the input buffer
	 *
	 * -------------------------------------
	 * |   |   |   |   |   |   |   |   |   | input buffer
	 * -------------------------------------
	 *
	 * |_______________________|___________|
	 *
	 *   bulk read operation     remaining
	 *
	 *
	 */
	if (parsed <= input_buffer.end) {
		/* Fill bulk read buffer from input buffer */
		memcpy(cur_parser->next_buffer, input_buffer.start,
			cur_parser->next_bytes);
	}
	/*
	 *
	 * -----------------------------
	 * |   |   |   |   |   |   |   |  input buffer
	 * -----------------------------
	 *
	 * |___________________________________________|
	 *
	 *              bulk read operation
	 *
	 * ---------------------------------------------
	 * |   |   |   |   |   |   |   |   |   |   |   |  bulk buffer
	 * ---------------------------------------------
	 *                               ^
	 *                               |
	 *                    pointer for HAL read operation
	 */
	else {
		size_t filled = input_buffer.end - input_buffer.start;

		/* Copy the whole input buffer to bulk read buffer */
		if (filled)
			memcpy(cur_parser->next_buffer,
				input_buffer.start,
				filled);

		/* Read the remaining bytes directly from the HAL */
		int result = cla_read_into(
				NULL,
				cur_parser->next_buffer + filled,
				cur_parser->next_bytes - filled);

		/* We could not read from input
		 *
		 *   - reset all parsers
		 *   - clear input buffer
		 */
		if (result != RETURN_SUCCESS) {
			reset_parser();
			return input_buffer.end;
		}
		parsed = input_buffer.end;
	}

	/* Disable bulk read mode */
	cur_parser->flags &= ~PARSER_FLAG_BULK_READ;

	/*
	 * Feed parser with an empty buffer, indicating that the bulk read
	 * operation was performed
	 */
	forward_to_specific_parser(NULL, 0);

	if (cur_parser->status != PARSER_STATUS_GOOD)
		reset_parser();

	return parsed;
}


/**
 * Reads a chunk of bytes into the input buffer and forwards the input buffer
 * to the specific parser.
 *
 * @return pointer to the position up to the input buffer is consumed after the
 *         operation
 */
static uint8_t *chunk_read(void)
{
	size_t read = 0;
	uint8_t *stream = input_buffer.start;

	int result = cla_read_chunk(
			NULL,
			input_buffer.end,
			(input_buffer.start + BUFFER_SIZE) - input_buffer.end,
			&read);

	/* We could not read from input
	 *
	 *   - reset all parsers
	 *   - keep input buffer
	 */
	if (result != RETURN_SUCCESS) {
		reset_parser();
		return stream;
	}
	input_buffer.end += read;

	while (stream < input_buffer.end) {
		size_t parsed = forward_to_specific_parser(stream,
					input_buffer.end - stream);

		/* Advance stream pointer */
		stream += parsed;

		if (cur_parser->status != PARSER_STATUS_GOOD)
			reset_parser();
		else if (HAS_FLAG(cur_parser->flags, PARSER_FLAG_BULK_READ))
			break;
		/*
		 * No bytes were parsed -- meaning that there is not enough data
		 * in the buffer. Stop parsing and wait for the buffer to be
		 * filled with sufficient data in next read iteration.
		 */
		else if (parsed == 0)
			break;
	}

	return stream;
}


void cla_contact_rx_task(void *param)
{
	uint8_t *parsed;

	input_buffer.end = input_buffer.start;

	bundle_signaling_queue = get_global_bundle_signaling_queue();
	router_signaling_queue = get_global_router_signaling_queue();

	ASSERT(cur_parser = input_parser_init(&input_parser));
	ASSERT(router_parser_init(&router_parser, &router_command_send));
	ASSERT(bundle6_parser_init(&bundle6_parser, &bundle_send));
	ASSERT(bundle7_parser_init(&bundle7_parser, &bundle_send));
	ASSERT(beacon_parser_init(&beacon_parser, &beacon_send));
	ASSERT(rrnd_parser_init(&rrnd_parser, &rrnd_execute_command));

	for (;;) {
		if (HAS_FLAG(cur_parser->flags, PARSER_FLAG_BULK_READ))
			parsed = bulk_read();
		else
			parsed = chunk_read();

		/* The whole input buffer was consumed, reset it */
		if (parsed == input_buffer.end)
			input_buffer.end = input_buffer.start;
		/*
		 * Remove parsed bytes from input buffer by shifting
		 * the parsed buffer bytes to the left.
		 *
		 *               remaining = 4
		 * ---------------------------------------
		 * | / | / | / | x | x | x | x |   |   |   ...
		 * ---------------------------------------
		 *               ^               ^
		 *               |               |
		 *               |               |
		 *             parsed           end
		 *
		 * memmove:
		 *     Copying takes place as if an intermediate buffer
		 *     were used, allowing the destination and source to
		 *     overlap.
		 *
		 * ---------------------------------------
		 * | x | x | x | x |   |   |   |   |   |   ...
		 * ---------------------------------------
		 *                   ^
		 *                   |
		 *                   |
		 *                  end
		 */
		else if (parsed != input_buffer.start) {
			memmove(input_buffer.start,
				parsed,
				input_buffer.end - parsed);

			/*
			 * Move end pointer backwards for that amount of bytes
			 * that were parsed
			 */
			input_buffer.end -= parsed - input_buffer.start;
		}
		/*
		 * No bytes were parsed but the input buffer is full. We assume
		 * that there was an attempt to send a too large value not
		 * fitting into the input buffer.
		 *
		 * We discard the current buffer content and reset all parsers.
		 */
		else if (input_buffer.end == input_buffer.start + BUFFER_SIZE) {
			LOG("InputProcessor: Warning! Input buffer full");
			input_buffer.end = input_buffer.start;
			reset_parser();
		}
	}
}

Task_t cla_launch_contact_rx_task(void)
{
	return hal_task_create(cla_contact_rx_task,
			"rx_t",
			CONTACT_RX_TASK_PRIORITY,
			NULL,
			CONTACT_RX_TASK_STACK_SIZE,
			(void *)CONTACT_RX_TASK_TAG);
}
