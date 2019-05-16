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
#include "upcn/bundleStorageManager.h"
#include "upcn/bundleProcessor.h"
#include <cla_contact_rx_task.h>
#include <signal.h>
#include <errno.h>
#include "upcn/init.h"
#include "spp/spp_parser.h"

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

enum payload_type {
	PAYLOAD_UNKNOWN = 0,
	PAYLOAD_BUNDLE6 = 6,
	PAYLOAD_BUNDLE7 = 7
};

struct rx_task_data {
	enum payload_type type;

	struct parser *cur_parser;

	struct spp_parser *spp_parser;
	struct bundle6_parser *bundle6_parser;
	struct bundle7_parser *bundle7_parser;

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
	rx_data->type = PAYLOAD_UNKNOWN;
	rx_data->cur_parser = spp_parser_init(rx_data->spp_parser,
					      rx_data->spp_parser->ctx);
	ASSERT(bundle6_parser_reset(rx_data->bundle6_parser) == UPCN_OK);
	ASSERT(bundle7_parser_reset(rx_data->bundle7_parser) == UPCN_OK);
	LOG("tcpssp: reset parsers.");
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
		/* rx_data->spp_parser->type = INPUT_TYPE_BUNDLE_V6; */
		rx_data->type = PAYLOAD_BUNDLE6;
		rx_data->cur_parser = rx_data->bundle6_parser->basedata;
		return bundle6_parser_read(rx_data->bundle6_parser,
				buffer, length);
	/* CBOR indefinite array -> Bundle Protocol v7 */
	case 0x9f:
		/* rx_data->input_parser->type = INPUT_TYPE_BUNDLE_V7; */
		rx_data->type = PAYLOAD_BUNDLE7;
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
	LOGF("tcpspp: got bytes: %zu", length);

	/* Current parser is input_parser */
	if (rx_data->spp_parser->state != SPP_PARSER_STATE_DATA_SUBPARSER)
		return spp_parser_read(rx_data->spp_parser,
							   buffer,
							   length);

	switch (rx_data->type) {
	case PAYLOAD_UNKNOWN:
		return select_bundle_parser_version(rx_data,
						    buffer,
						    length);
	case PAYLOAD_BUNDLE6:
		rx_data->cur_parser = rx_data->bundle6_parser->basedata;
		return bundle6_parser_read(rx_data->bundle6_parser,
					   buffer,
					   length);
	case PAYLOAD_BUNDLE7:
		rx_data->cur_parser = rx_data->bundle7_parser->basedata;
		return bundle7_parser_read(rx_data->bundle7_parser,
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

	if (rx_data->cur_parser->status != PARSER_STATUS_GOOD) {
		/* make sure to reset parsers if something went wrong */
		reset_parser(rx_data);
	}

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

	LOGF("tcpspp: %s(..., %zu): initiating read(..., %zd)",
	     __func__,
	     filled, BUFFER_SIZE - filled);
	int result = cla_read_chunk(config,
			rx_data->input_buffer + filled,
			BUFFER_SIZE - filled,
			&read);
	LOGF("tcpspp: %s(..., %zu): read returned: %d",
	     __func__,
	     result);
	/* We could not read from input
	 *
	 *   - reset all parsers
	 *   - keep input buffer
	 */
	if (result != RETURN_SUCCESS) {
		en = errno;
		if (en == EAGAIN || en == EWOULDBLOCK) {
			rx_data->timeout_occured = true;
		} else {
			// XXX: I’m not sure this is a great idea. The
			// TCP_LEGACY code already had this _take_blocking in
			// there; I added a _release, otherwise it would
			// dead-lock after a new connection was established.
			// However, I’m not sure this is all that great. The
			// buffer should probably be cleared at least to enter
			// a proper clean state.
			// Leaving it as-is for now. (jwi, 2018-04-16)
			LOG("tcpspp: waiting for new connection.");
			hal_semaphore_take_blocking(
					config->cla_com_rx_semaphore);
			hal_semaphore_release(
					config->cla_com_rx_semaphore);
		}
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

		if (rx_data->cur_parser->status != PARSER_STATUS_GOOD) {
			reset_parser(rx_data);
		} else if (HAS_FLAG(rx_data->cur_parser->flags,
				    PARSER_FLAG_BULK_READ)) {
			break;
		} else if (parsed == 0) {
			/*
			 * No bytes were parsed -- meaning that there is not
			 * enough data in the buffer. Stop parsing and wait for
			 * the buffer to be filled with sufficient data in next
			 * read iteration.
			 */
			break;
		}
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
	rx_data->spp_parser = malloc(sizeof(struct spp_parser));
	rx_data->bundle6_parser = malloc(sizeof(struct bundle6_parser));
	rx_data->bundle7_parser = malloc(sizeof(struct bundle7_parser));
	rx_data->input_buffer = malloc(sizeof(uint8_t)*BUFFER_SIZE);

	ASSERT(rx_data->cur_parser = spp_parser_init(rx_data->spp_parser,
						     config->spp_ctx));

	ASSERT(bundle6_parser_init(rx_data->bundle6_parser,
				   &bundle_send));
	ASSERT(bundle7_parser_init(rx_data->bundle7_parser,
				   &bundle_send));

	LOG("tcpspp initialized");

	for (;;) {
		if (HAS_FLAG(rx_data->cur_parser->flags,
			     PARSER_FLAG_BULK_READ)) {
			LOG("tcpspp: bulk read");
			remaining = bulk_read(config, rx_data, filled);
		} else {
			LOG("tcpspp: chunk read");
			remaining = chunk_read(config, rx_data, filled);
		}

		if (remaining == 0) {
			/* The whole input buffer was consumed, reset it */
			filled = 0;
		} else if (remaining > 0) {
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
			size_t parsed = filled - remaining;

			memmove(rx_data->input_buffer,
				rx_data->input_buffer + parsed,
				remaining);
			filled = remaining;
		} else if (filled == BUFFER_SIZE) {
			/*
			 * No bytes were parsed but the input buffer is full.
			 * We assume that there was an attempt to send a too
			 * large value not fitting into the input buffer.
			 *
			 * We discard the current buffer content and reset all
			 * parsers.
			 */
			LOG("contact_rx_task: Warning! Input buffer full");
			filled = 0;
			reset_parser(rx_data);
		}
	}

	// we should never reach this point!

	free(rx_data->spp_parser);
	free(rx_data->bundle6_parser);
	free(rx_data->bundle7_parser);
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
