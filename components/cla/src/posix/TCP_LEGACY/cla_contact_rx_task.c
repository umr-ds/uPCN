#include <stddef.h>
/* FIXME
 *    Without this line newlib's stdlib.h included fom libcbor raises an error
 */
#include <stdlib.h>
#include <string.h>

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
#include <signal.h>
#include <errno.h>
#include "upcn/init.h"

#include <cla_io.h>
#include <cla_management.h>
#include <hal_config.h>

#ifdef THROUGHPUT_TEST
extern uint64_t timestamp_initialread;
uint64_t timestamp_mp1[47];
uint32_t bundle_size[47];
#endif

static QueueIdentifier_t router_signaling_queue;
static QueueIdentifier_t bundle_signaling_queue;

struct rx_task_data {
	struct parser *cur_parser;

	struct input_parser *input_parser;
	struct router_parser *router_parser;
	struct bundle6_parser *bundle6_parser;
	struct bundle7_parser *bundle7_parser;
	struct beacon_parser *beacon_parser;
	struct rrnd_parser *rrnd_parser;

	/**
	 * Size of the input buffer in bytes
	 */
	#define BUFFER_SIZE BUNDLE_QUOTA
	uint8_t *input_buffer;
	bool timeout_occured;
};

static void reset_parser(struct rx_task_data *rx_data)
{
#ifdef THROUGHPUT_TEST
	timestamp_initialread = 0;
#endif
	input_parser_reset(rx_data->input_parser);
	rx_data->cur_parser = rx_data->input_parser->basedata;
	ASSERT(router_parser_reset(rx_data->router_parser) == UPCN_OK);
	ASSERT(bundle6_parser_reset(rx_data->bundle6_parser) == UPCN_OK);
	ASSERT(bundle7_parser_reset(rx_data->bundle7_parser) == UPCN_OK);
	ASSERT(beacon_parser_reset(rx_data->beacon_parser) == UPCN_OK);
	ASSERT(rrnd_parser_reset(rx_data->rrnd_parser) == UPCN_OK);
}

static size_t select_bundle_parser_version(struct rx_task_data *rx_data,
					   const uint8_t *buffer,
					   size_t length)
{
	/* Empty buffers cannot be parsed */
	if (length == 0) {
		reset_parser(rx_data);
		return 0;
	}

	switch (buffer[0]) {
	/* Bundle Protocol v6 (RFC 5050) */
	case 6:
		rx_data->input_parser->type = INPUT_TYPE_BUNDLE_V6;
		rx_data->cur_parser = rx_data->bundle6_parser->basedata;
		return bundle6_parser_read(rx_data->bundle6_parser,
				buffer, length);
	/* CBOR indefinite array -> Bundle Protocol v7 */
	case 0x9f:
		rx_data->input_parser->type = INPUT_TYPE_BUNDLE_V7;
		rx_data->cur_parser = rx_data->bundle7_parser->basedata;
		return bundle7_parser_read(rx_data->bundle7_parser,
				buffer, length);
	/* Unknown Bundle Protocol version, keep buffer */
	default:
		reset_parser(rx_data);
		return 0;
	}
}

static size_t forward_to_specific_parser(struct rx_task_data *rx_data,
					 const uint8_t *buffer,
					 size_t length)
{
	/* Current parser is input_parser */
	if (rx_data->input_parser->stage != INPUT_EXPECT_DATA)
		return input_parser_read(rx_data->input_parser,
					 buffer,
					 length);

	switch (rx_data->input_parser->type) {
	case INPUT_TYPE_ROUTER_COMMAND_DATA:
		rx_data->cur_parser = rx_data->router_parser->basedata;
		return router_parser_read(rx_data->router_parser,
					  buffer,
					  length);

	case INPUT_TYPE_BUNDLE_VERSION:
		return select_bundle_parser_version(rx_data,
						    buffer,
						    length);
	case INPUT_TYPE_BUNDLE_V6:
		rx_data->cur_parser = rx_data->bundle6_parser->basedata;
		return bundle6_parser_read(rx_data->bundle6_parser,
					   buffer,
					   length);
	case INPUT_TYPE_BUNDLE_V7:
		rx_data->cur_parser = rx_data->bundle7_parser->basedata;
		return bundle7_parser_read(rx_data->bundle7_parser,
					   buffer,
					   length);
	case INPUT_TYPE_BEACON_DATA:
		rx_data->cur_parser = rx_data->beacon_parser->basedata;
		return beacon_parser_read(rx_data->beacon_parser,
					  buffer,
					  length);
	case INPUT_TYPE_RRND_DATA:
		rx_data->cur_parser = rx_data->rrnd_parser->basedata;
		return rrnd_parser_read(rx_data->rrnd_parser,
					buffer,
					length);
	/* Unknown parser type, keep buffer and reset all parsers */
	default:
		reset_parser(rx_data);
		return 0;
	}
	return RETURN_SUCCESS;
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

static void router_command_send(struct router_command *cmd)
{
	struct router_signal signal = {
		.type = ROUTER_SIGNAL_PROCESS_COMMAND,
		.data = cmd
	};

	ASSERT(cmd != NULL);
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
 * @param buffer already buffered input bytes
 * @param filled number of bytes in the input buffer
 * @return number of bytes remaining in the input buffer after the operation
 */
static size_t bulk_read(struct cla_config *config,
			struct rx_task_data *rx_data,
			size_t filled)
{
	size_t remaining;
	int en;

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
	if (rx_data->cur_parser->next_bytes <= filled) {
		/* Fill bulk read buffer from input buffer */
		memcpy(rx_data->cur_parser->next_buffer, rx_data->input_buffer,
			rx_data->cur_parser->next_bytes);
		remaining = filled - rx_data->cur_parser->next_bytes;
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
	 *                               |
	 *                    pointer for HAL read operation
	 */
	else {
		/* Copy the whole input buffer to bulk read buffer */
		if (filled) {
			memcpy(rx_data->cur_parser->next_buffer,
			       rx_data->input_buffer,
				filled);
		}

		/* Read the remaining bytes directly from the HAL */
		int result = cla_read_into(config,
				rx_data->cur_parser->next_buffer + filled,
				rx_data->cur_parser->next_bytes - filled);

		if (result == EAGAIN || result == EWOULDBLOCK)
			rx_data->timeout_occured = true;

		/* We could not read from input
		 *
		 *   - reset all parsers
		 *   - clear input buffer
		 */
		if (result != RETURN_SUCCESS) {
			en = errno;
			if (en == EAGAIN || en == EWOULDBLOCK)
				rx_data->timeout_occured = true;
			else {
				hal_semaphore_take_blocking(
						config->cla_com_rx_semaphore);
			}
			reset_parser(rx_data);
			return 0;
		}
		remaining = 0;
	}

	/* Disable bulk read mode */
	rx_data->cur_parser->flags &= ~PARSER_FLAG_BULK_READ;

	/*
	 * Feed parser with an empty buffer, indicating that the bulk read
	 * operation was performed
	 */
	forward_to_specific_parser(rx_data, NULL, 0);

	if (rx_data->cur_parser->status != PARSER_STATUS_GOOD)
		reset_parser(rx_data);

	return remaining;
}


/**
 * Reads a chunk of bytes into the input buffer and forwards the input buffer
 * to the specific parser.
 *
 * @param filled number of bytes in the input buffer
 * @return number of bytes remaining in the input buffer after the operation
 */
static size_t chunk_read(struct cla_config *config,
			 struct rx_task_data *rx_data,
			 size_t filled)
{
	size_t read = 0;
	uint8_t *stream = rx_data->input_buffer;
	int en;

	int result = cla_read_chunk(config,
			rx_data->input_buffer + filled,
			BUFFER_SIZE - filled,
			&read);
	/* We could not read from input
	 *
	 *   - reset all parsers
	 *   - keep input buffer
	 */
	if (result != RETURN_SUCCESS) {
		en = errno;
		if (en == EAGAIN || en == EWOULDBLOCK)
			rx_data->timeout_occured = true;
		else
			hal_semaphore_take_blocking(
					config->cla_com_rx_semaphore);
		reset_parser(rx_data);
		return filled;
	}
	filled += read;

	while (stream < rx_data->input_buffer + filled) {
		size_t parsed = forward_to_specific_parser(rx_data,
							   stream,
				filled - (stream - rx_data->input_buffer));

		/* Advance stream pointer */
		stream += parsed;

		if (rx_data->cur_parser->status != PARSER_STATUS_GOOD)
			reset_parser(rx_data);
		else if (HAS_FLAG(rx_data->cur_parser->flags,
				  PARSER_FLAG_BULK_READ))
			break;
		/*
		 * No bytes were parsed -- meaning that there is not enough data
		 * in the buffer. Stop parsing and wait for the buffer to be
		 * filled with sufficient data in next read iteration.
		 */
		else if (parsed == 0)
			break;

	}

	return filled - (stream - rx_data->input_buffer);
}


void cla_contact_rx_task(void * const param)
{
	size_t remaining;
	size_t filled = 0; /* Number of bytes in buffer */
	struct cla_config *config;
	struct cla_contact_rx_task_parameters *p =
		(struct cla_contact_rx_task_parameters *)param;

	bundle_signaling_queue = p->bundle_signaling_queue;
	router_signaling_queue = p->router_signaling_queue;

	LOG("contact_rx_task: Started up successfully");

	config = p->pair->config;

	struct rx_task_data *rx_data = malloc(sizeof(struct rx_task_data));

	rx_data->timeout_occured = false;
	rx_data->input_parser = malloc(sizeof(struct input_parser));
	rx_data->router_parser = malloc(sizeof(struct router_parser));
	rx_data->bundle6_parser = malloc(sizeof(struct bundle6_parser));
	rx_data->bundle7_parser = malloc(sizeof(struct bundle7_parser));
	rx_data->beacon_parser = malloc(sizeof(struct beacon_parser));
	rx_data->rrnd_parser = malloc(sizeof(struct rrnd_parser));
	rx_data->input_buffer = malloc(sizeof(uint8_t)*BUFFER_SIZE);

	ASSERT(rx_data->cur_parser = input_parser_init(rx_data->input_parser));

	ASSERT(router_parser_init(rx_data->router_parser,
				  &router_command_send));
	ASSERT(bundle6_parser_init(rx_data->bundle6_parser,
				   &bundle_send));
	ASSERT(bundle7_parser_init(rx_data->bundle7_parser,
				   &bundle_send));
	ASSERT(beacon_parser_init(rx_data->beacon_parser,
				  &beacon_send));
	ASSERT(rrnd_parser_init(rx_data->rrnd_parser, &rrnd_execute_command));

	for (;;) {
		if (HAS_FLAG(rx_data->cur_parser->flags, PARSER_FLAG_BULK_READ))
			remaining = bulk_read(config, rx_data, filled);
		else
			remaining = chunk_read(config, rx_data, filled);

		/* The whole input buffer was consumed, reset it */
		if (remaining == 0)
			filled = 0;
		/*
		 * Remove parsed bytes from input buffer by shifting
		 * the parsed buffer bytes to the left.
		 *
		 * filled = 7
		 *
		 *   parsed = 3  remaining = 4
		 * ---------------------------------------
		 * | / | / | / | x | x | x | x |   |   |   ...
		 * ---------------------------------------
		 *               ^
		 *               |
		 *               |
		 *     input_buffer + parsed
		 *
		 * memmove:
		 *     Copying takes place as if an intermediate buffer
		 *     were used, allowing the destination and source to
		 *     overlap.
		 */
		else if (remaining > 0) {
			size_t parsed = filled - remaining;

			memmove(rx_data->input_buffer,
				rx_data->input_buffer + parsed,
				remaining);
			filled = remaining;
		}
		/*
		 * No bytes were parsed but the input buffer is full. We assume
		 * that there was an attempt to send a too large value not
		 * fitting into the input buffer.
		 *
		 * We discard the current buffer content and reset all parsers.
		 */
		else if (filled == BUFFER_SIZE) {
			LOG("contact_rx_task: Warning! Input buffer full");
			filled = 0;
			reset_parser(rx_data);
		}
	}

	// we should never reach this point!

	free(rx_data->input_parser);
	free(rx_data->router_parser);
	free(rx_data->bundle6_parser);
	free(rx_data->bundle7_parser);
	free(rx_data->beacon_parser);
	free(p);
	free(rx_data);

}

Task_t cla_launch_contact_rx_task(cla_handler *pair)
{
	struct cla_contact_rx_task_parameters *cla_contact_rx_task_params
		= malloc(sizeof(struct cla_contact_rx_task_parameters));

	ASSERT(cla_contact_rx_task_params != NULL);
	cla_contact_rx_task_params->router_signaling_queue
		= get_global_router_signaling_queue();
	cla_contact_rx_task_params->bundle_signaling_queue
		= get_global_bundle_signaling_queue();
	cla_contact_rx_task_params->pair = pair;

	return hal_task_create(cla_contact_rx_task,
			"rx_t",
			CONTACT_RX_TASK_PRIORITY,
			cla_contact_rx_task_params,
			CONTACT_RX_TASK_STACK_SIZE,
			(void *)CONTACT_RX_TASK_TAG);
}
