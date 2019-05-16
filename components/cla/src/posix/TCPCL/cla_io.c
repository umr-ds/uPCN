#include <cla_io.h>
#include <cla_defines.h>
#include <hal_task.h>
#include <cla_contact_rx_task.h>
#include <cla_management.h>
#include <cla_io.h>

#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include "upcn/sdnv.h"
#include <signal.h>
#include <errno.h>
#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <cla_contact_rx_task.h>
#include "upcn/routingTable.h"

#ifdef THROUGHPUT_TEST

extern uint64_t timestamp_mp1[47];
extern uint64_t timestamp_mp2[47];
extern uint64_t timestamp_mp3[47];
extern uint64_t timestamp_mp4[47];
extern uint32_t bundle_size[47];
uint64_t timestamp_initialread;
#endif

/**
 * @brief The tcpcl_message_types enum contains all possible message types
 *		according to RFC 7242 (Table 2).
 */
enum tcpcl_message_types {
	TCPCL_TYPE_DATA_SEGMENT = 0x01,
	TCPCL_TYPE_ACK_SEGMENT = 0x02,
	TCPCL_TYPE_REFUSE_BUNDLE = 0x03,
	TCPCL_TYPE_KEEPALIVE = 0x04,
	TCPCL_TYPE_SHUTDOWN = 0x05,
	TCPCL_TYPE_LENGTH = 0x06
};

static uint8_t sdnv_buffer[BUNDLE_QUOTA];

int16_t cla_send_tcpl_header_packet(struct cla_config *config)
{
	static char my_scheme[] = UPCN_SCHEME;
	static char my_ssp[] = UPCN_SSP;

	// Generate EID and get size.

	int eid_len = sizeof(UPCN_SCHEME) + sizeof(UPCN_SSP) + 1;
	char *eid = malloc(eid_len + 1);

	if (eid == NULL)
		return RETURN_FAILURE;

	memcpy(eid, my_scheme, sizeof(my_scheme));
	eid[sizeof(my_scheme)-1] = ':';
	memcpy(eid + sizeof(my_scheme), my_ssp, sizeof(my_ssp));
	eid[eid_len] = '\0';

	// TODO: Check if EID is too long.

	// Calculate SDNV size of EID.
	int sdnv_len = sdnv_write_u32(sdnv_buffer, eid_len);

	// Allocate memory for header packet.
	int packet_len = 8 + sdnv_len + eid_len;
	char *hpacket = malloc(packet_len);

	if (hpacket == NULL) {
		free(eid);
		return RETURN_FAILURE;
	}

	// Write magic into packet (string "dtn!").
	hpacket[0] = 0x64;
	hpacket[1] = 0x74;
	hpacket[2] = 0x6e;
	hpacket[3] = 0x21;

	// Put version number into packet (RFC7242 --> 3).
	hpacket[4] = 3;

	// Set flags (currently we don't support any additional tcpcl features)
	// -> TODO later
	hpacket[5] = 0x00;

	// Set keep_alive interval (currently not supported -> 0).
	hpacket[6] = 0x00;
	hpacket[7] = 0x00;

	// Add sdnv value.
	memcpy(&hpacket[8], sdnv_buffer, sdnv_len);

	// Add eid value.
	memcpy(&hpacket[8+sdnv_len], eid, eid_len);

	// Send packet.
	cla_lock_com_tx_semaphore(config);
	if (send(config->socket_identifier, hpacket,
		  packet_len, 0) == -1) {
		free(eid);
		free(hpacket);
		LOG("An error occured during sending! Data discarded!");
		return RETURN_FAILURE;
	}
	cla_unlock_com_tx_semaphore(config);

	// Free allocated resources.
	free(hpacket);
	free(eid);

	return RETURN_SUCCESS;
}

void cla_lock_com_tx_semaphore(struct cla_config *config)
{
	hal_semaphore_take_blocking(config->cla_com_tx_semaphore);
}

void cla_unlock_com_tx_semaphore(struct cla_config *config)
{
	hal_semaphore_release(config->cla_com_tx_semaphore);
}

void cla_lock_com_rx_semaphore(struct cla_config *config)
{
	hal_semaphore_take_blocking(config->cla_com_rx_semaphore);
}

void cla_unlock_com_rx_semaphore(struct cla_config *config)
{
	hal_semaphore_release(config->cla_com_rx_semaphore);
}

void cla_begin_packet(struct cla_config *config,
		      const size_t length, const enum comm_type type)
{
	static uint8_t sdnv_buffer[MAX_SDNV_SIZE];

	if (config->state == CLA_STATE_IGNORE)
		return;

	hal_semaphore_take_blocking(config->cla_com_tx_semaphore);

	config->packet = malloc(sizeof(struct cla_packet));

	struct cla_packet *snd_packet = config->packet;

	// Set packet type to DATA_SEGMENT and set both start and end flags.
	snd_packet->sndbuffer[0] = (TCPCL_TYPE_DATA_SEGMENT<<4 | 0x3);


	// Calculate and set SDNV size of packet length.
	int sdnv_len = sdnv_write_u32(sdnv_buffer, length);

	memcpy(&snd_packet->sndbuffer[1], sdnv_buffer, sdnv_len);

	if (type != COMM_TYPE_BUNDLE) {
		/* Initialize the inner header. */
		snd_packet->sndbuffer[sdnv_len+1] = 0x0A;
		snd_packet->sndbuffer[sdnv_len+2] = 0x42;
		snd_packet->sndbuffer[sdnv_len+3] = (uint8_t)type;
		snd_packet->sndbuffer[sdnv_len+4] = (length & 0xFF00) >> 8;
		snd_packet->sndbuffer[sdnv_len+5] = length & 0x00FF;
		snd_packet->packet_position_ptr =
				&snd_packet->sndbuffer[6+sdnv_len];
		snd_packet->length = length+6+sdnv_len+1;
	} else {
		snd_packet->packet_position_ptr =
				&snd_packet->sndbuffer[1+sdnv_len];
		snd_packet->length = length+1+sdnv_len;
	}

	snd_packet->state = TM_ACTIVE;
}

void cla_end_packet(struct cla_config *config)
{
	struct cla_packet *snd_packet = config->packet;

	if (config->state == CLA_STATE_IGNORE)
		return;

	if (snd_packet->state == TM_INACTIVE ||
			snd_packet->state == TM_ERROR) {
		LOG("An error occured before sending. Restart.");
		return;
	}

	if (send(config->socket_identifier, snd_packet->sndbuffer,
		  snd_packet->length, 0) == -1) {
		LOG("An error occured during sending. Data discarded.");
		snd_packet->state = TM_ERROR;
		config->connection_established = false;
	}

	snd_packet->state = TM_INACTIVE;
	hal_semaphore_release(config->cla_com_tx_semaphore);
}

void cla_send_packet_data(const void *config,
			  const void *data, const size_t length)
{

	struct cla_config *conf = (struct cla_config *) config;
	struct cla_packet *snd_packet = conf->packet;

	if (conf->state == CLA_STATE_IGNORE)
		return;

	if (snd_packet->state == TM_ERROR) {
		LOG("Transmission in error state. Discarding data. Restart "\
		    "transmission.");
		return;
	} else if (snd_packet->state == TM_INACTIVE) {
		LOG("No transmission started. Start the packet transmission "\
		    "first. Discarding data.");
		return;
	}

	/* copy the data into the buffer */
	memcpy(snd_packet->packet_position_ptr, data, length);

	/* move the pointer to the next free byte in the buffer */
	snd_packet->packet_position_ptr += length;
}

void cla_write_raw(struct cla_config *config, void *data, size_t length)
{
	cla_send_packet(config, data, length, COMM_TYPE_MESSAGE);
}

void cla_write_string(struct cla_config *config, const char *string)
{
	cla_send_packet(config, string, strlen(string), COMM_TYPE_MESSAGE);
}

int16_t cla_read_into(struct cla_config *config, uint8_t *buffer, size_t length)
{
	int ret;

	if (config->state == CLA_STATE_IGNORE)
		return RETURN_FAILURE;

	if (cla_ensure_connection(config))
		return RETURN_FAILURE;

	cla_lock_com_rx_semaphore(config);

	while (length) {
		ret = recv(config->socket_identifier, buffer, length, 0);
		if (ret < 0) {
			LOGI("Error reading from socket!", errno);
			/*
			 * NOTE: Shouldn't we handle errors other
			 * than EAGAIN better?
			 */
			cla_unlock_com_rx_semaphore(config);
			return RETURN_FAILURE;
		} else if (!ret) {
			// the peer has disconnected gracefully
			LOG("The peer has disconnected gracefully!");
			cla_disconnect(config);
			cla_unlock_com_rx_semaphore(config);
			return RETURN_FAILURE;
		}
		length -= ret;
		buffer += ret;
	}

	cla_unlock_com_rx_semaphore(config);

	return RETURN_SUCCESS;
}

int16_t cla_read_chunk(struct cla_config *config,
		   uint8_t *buffer,
		   size_t length,
		   size_t *bytes_read)
{
	if (config->state == CLA_STATE_IGNORE)
		return -1;

	cla_ensure_connection(config);

	cla_lock_com_rx_semaphore(config);

#ifdef THROUGHPUT_TEST
	if (timestamp_initialread == 0)
		timestamp_initialread = hal_time_get_timestamp_us();
#endif
	ssize_t ret = recv(config->socket_identifier, buffer, length, 0);

	if (ret < 0) {
		LOGI("Error reading from socket.", errno);
		/*
		 * TODO: Shouldn't we handle errors other
		 * than EAGAIN better?
		 */
		cla_unlock_com_rx_semaphore(config);
		return RETURN_FAILURE;
	} else if (ret == 0) {
		// The peer has disconnected gracefully.
		LOG("The peer has disconnected gracefully.");
		cla_disconnect(config);
		cla_unlock_com_rx_semaphore(config);
		return RETURN_FAILURE;
	}
	*bytes_read = ret;
	cla_unlock_com_rx_semaphore(config);
	return RETURN_SUCCESS;
}

int16_t cla_read_raw(struct cla_config *config)
{
	uint8_t buf;

	if (cla_read_into(config, &buf, 1) == RETURN_SUCCESS)
		return buf;

	return -1;
}

int16_t cla_try_lock_com_tx_semaphore(struct cla_config *config,
				      uint16_t ms_timeout)
{
	return hal_semaphore_try_take(config->cla_com_tx_semaphore, ms_timeout);
	return 0;
}

int16_t cla_try_lock_com_rx_semaphore(struct cla_config *config,
				      uint16_t ms_timeout)
{
	return hal_semaphore_try_take(config->cla_com_rx_semaphore, ms_timeout);
}

void cla_send_packet(struct cla_config *config,
		     const void *data,
		     const size_t length,
		     const enum comm_type type)
{
	if (config->state == CLA_STATE_IGNORE)
		return;

	if (config->connection_established) {
		cla_begin_packet(config, length, type);
		cla_send_packet_data(config, data, length);
		cla_end_packet(config);
	} else {
		LOG("Discarded data because no connection present.");
	}
}

int16_t cla_output_data_waiting(struct cla_config *config)
{
	/* Not relevant in POSIX, realised by closing the */
	/* socket properly (in hal_platform/hal_io_exit). */
	return RETURN_FAILURE;
}
