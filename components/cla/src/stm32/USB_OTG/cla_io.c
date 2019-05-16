#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "upcn/config.h"
#include "cla_io.h"
#include "upcn/buildFlags.h"

#ifdef THROUGHPUT_TEST

extern uint64_t timestamp_mp1[47];
extern uint64_t timestamp_mp2[47];
extern uint64_t timestamp_mp3[47];
extern uint64_t timestamp_mp4[47];
extern uint32_t bundle_size[47];
uint64_t timestamp_initialread;
#endif

#ifdef INCLUDE_BOARD_LIB

#include <usbd_cdc_vcp.h>
#include "hal_semaphore.h"
#include "hal_queue.h"
#include <hal_config.h>
#include <hal_time.h>
#include <cla_defines.h>
#include <cla.h>
#include <FreeRTOS.h>

static QueueIdentifier_t rx_queue, tx_queue;

static uint8_t tlvbuf[5];
static Semaphore_t comm_semaphore;

static struct cla_config *conf;

static inline void VCP_Transmit_Bytes(const uint8_t *_buf, const int _len)
{
	int i;

#ifndef COMM_BLOCK_ON_DISCONNECT
	if (!USB_VCP_Connected()) {
		hal_queue_reset(tx_queue);
		return;
	}
#endif /* COMM_BLOCK_ON_DISCONNECT */
	for (i = 0; i < _len; i++) {
#if !defined(COMM_TX_TIMEOUT) || COMM_TX_TIMEOUT < 0
		hal_queue_push_to_back(tx_queue, &_buf[i]);
#else /* !defined(COMM_TX_TIMEOUT) || COMM_TX_TIMEOUT < 0 */
		if (hal_queue_try_push_to_back(tx_queue, &_buf[i],
				COMM_TX_TIMEOUT) != pdPASS)
			return;
#endif /* !defined(COMM_TX_TIMEOUT) || COMM_TX_TIMEOUT < 0 */
	}
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
	hal_semaphore_take_blocking(comm_semaphore);
	tlvbuf[2] = (uint8_t)type;
	tlvbuf[3] = (length & 0xFF00) >> 8;
	tlvbuf[4] = length & 0x00FF;
	VCP_Transmit_Bytes(tlvbuf, 5);
}

void cla_end_packet(struct cla_config *config)
{
	hal_semaphore_release(comm_semaphore);
}

void cla_send_packet_data(const void *config,
			  const void *data, const size_t length)
{
	VCP_Transmit_Bytes(data, length);
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
	uint8_t c;

	while (length) {
		while (hal_queue_receive(rx_queue, &c, -1) != pdPASS)
			;
		*(buffer++) = c;
		length--;
	}

#ifdef THROUGHPUT_TEST
	if (timestamp_initialread == 0)
		timestamp_initialread = hal_time_get_timestamp_us();
#endif
	return RETURN_SUCCESS;
}

int16_t cla_read_chunk(struct cla_config *config,
		   uint8_t *buffer,
		   size_t length,
		   size_t *bytes_read)
{
#ifdef THROUGHPUT_TEST
	if (timestamp_initialread == 0)
		timestamp_initialread = hal_time_get_timestamp_us();
#endif
	// Special case: empty buffer
	if (length == 0) {
		*bytes_read = 0;
		return RETURN_SUCCESS;
	}

	// Write-pointer to the current buffer position
	uint8_t *stream = buffer;

	// Block indefinitly for the first byte
	while (hal_queue_receive(rx_queue, stream, -1) != pdPASS)
		;
	stream++;

	// 10ms timeout for every next byte
	while (stream < buffer + length
			&& hal_queue_receive(rx_queue, stream, 10) == pdPASS)
		stream++;

	*bytes_read = stream - buffer;

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
	cla_begin_packet(config, length, type);
	cla_send_packet_data(config, data, length);
	cla_end_packet(config);
}

int16_t cla_output_data_waiting(struct cla_config *config)
{
	return (tx_queue != NULL && uxQueueMessagesWaiting(tx_queue));

}

int16_t cla_io_init(void)
{
	tlvbuf[0] = 0x0A;
	tlvbuf[1] = 0x42;

	rx_queue = hal_queue_create(COMM_RX_QUEUE_LENGTH, sizeof(uint8_t));
	tx_queue = hal_queue_create(COMM_TX_QUEUE_LENGTH, sizeof(uint8_t));
	USB_VCP_Init(rx_queue, tx_queue);

	comm_semaphore = hal_semaphore_init_binary();
	if (comm_semaphore == NULL)
		return EXIT_FAILURE;
	hal_semaphore_release(comm_semaphore);

	conf = cla_get_debug_cla_handler();

	return EXIT_SUCCESS;
}

#endif /* INCLUDE_BOARD_LIB */
