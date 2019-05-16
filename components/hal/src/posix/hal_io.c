/*
 * hal_io.c
 *
 * Description: contains the POSIX implementation of the hardware
 * abstraction layer interface for input/output functionality
 *
 */

#include "hal_io.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "upcn/config.h"
#include <pthread.h>

#include <semaphore.h>
#include "hal_queue.h"
#include <hal_config.h>
#include <hal_defines.h>
#include <hal_debug.h>
#include <hal_time.h>
#include <hal_semaphore.h>
#include <signal.h>
#include <errno.h>

#include <cla.h>
#include <cla_io.h>


#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <unistd.h>

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
	/* not relevant in POSIX, realised by closing the */
	/* socket properly (in hal_platform/hal_io_exit) */
	return RETURN_FAILURE;
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
