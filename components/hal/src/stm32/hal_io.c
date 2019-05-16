/*
 * hal_io.c
 *
 * Description: contains the stm32-implementation of the hardware
 * abstraction layer interface for input/output functionality
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hal_io.h"
#include "upcn/config.h"
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
#include <hal_defines.h>
#include <FreeRTOS.h>
#include <cla_io.h>
#include <cla.h>


void hal_io_write_raw(void *data, size_t length)
{
	hal_io_send_packet(data, length, COMM_TYPE_MESSAGE);
}

void hal_io_write_string(const char *string)
{
	hal_io_send_packet(string, strlen(string), COMM_TYPE_MESSAGE);
}

void hal_io_lock_com_semaphore(void)
{
	cla_lock_com_tx_semaphore(cla_get_debug_cla_handler());
}

void hal_io_unlock_com_semaphore(void)
{
	cla_unlock_com_tx_semaphore(cla_get_debug_cla_handler());
}

void hal_io_send_packet(const void *data,
			const size_t length,
			const enum comm_type type)
{
	cla_send_packet(cla_get_debug_cla_handler(),
			data,
			length,
			type);
}

void hal_io_begin_packet(const size_t length, const enum comm_type type)
{
	cla_begin_packet(cla_get_debug_cla_handler(),
			 length,
			 type);
}

void hal_io_send_packet_data(const void *data, const size_t length)
{
	cla_send_packet_data(cla_get_debug_cla_handler(),
			     data,
			     length);
}

void hal_io_end_packet(void)
{
	cla_end_packet(cla_get_debug_cla_handler());
}

uint8_t hal_io_output_data_waiting(void)
{
	return cla_output_data_waiting(cla_get_debug_cla_handler());
}

uint8_t hal_io_try_lock_com_semaphore(uint16_t ms_timeout)
{
	return cla_try_lock_com_tx_semaphore(cla_get_debug_cla_handler(),
					     ms_timeout);
}

int hal_io_exit(void)
{
	return RETURN_FAILURE;
}


#endif /* INCLUDE_BOARD_LIB */
