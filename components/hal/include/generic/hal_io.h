/*
 * hal_debug.h
 *
 * Description: contains the definitions of the hardware abstraction
 * layer interface for input/output functionality
 *
 */

#ifndef HAL_IO_H_INCLUDED
#define HAL_IO_H_INCLUDED

#include <stddef.h>
#include <stdint.h>

/**
 * @brief The comm_type enum Specifies the packet types that are sent or
 *			     received via the IO interface
 */
enum comm_type {
	COMM_TYPE_MESSAGE = 0x01,
	COMM_TYPE_BUNDLE = 0x02,
	COMM_TYPE_BEACON = 0x03,
	COMM_TYPE_ECHO = 0x04,
	COMM_TYPE_GS_INFO = 0x05,
	COMM_TYPE_PERF_DATA = 0x06,
	COMM_TYPE_RRND_STATUS = 0x07,
	COMM_TYPE_GENERIC_RESULT = 0x08,
	COMM_TYPE_CONTACT_STATE = 0x09,
	COMM_TYPE_UNDEFINED = 0xFF
};

/**
 * @brief hal_io_init Initialization of underlying OS/HW for I/O
 * @return EXIT_SUCCESS or EXIT_FAILURE (macros are resolved to convention of
 *         the underlying OS infrastructure
 */
int hal_io_init(void);

/**
 * @brief hal_debug_init Listen on the I/O socket
 * @param io_socket_port the port which will be used to open the socket;
 *                       0 is default value, in this case the default port
 *                       is used
 * @return EXIT_SUCCESS or EXIT_FAILURE (macros are resolved to convention of
 *         the underlying OS infrastructure
 */
void hal_io_listen(void *param);

/**
 * @brief hal_debug_write_raw Write raw data of a specific length to the
 *			      output interface
 * @param data The raw data
 * @param length The length of the raw data (in Bytes)
 */
void hal_io_write_raw(void *data, size_t length);

/**
 * @brief hal_io_write Write a string to the output interface
 * @param string
 */
void hal_io_write_string(const char *string);

/**
 * @brief hal_io_read_into Read several bytes from the interface into a buffer
 * @param buffer The target buffer to be read into
 * @param length The count of bytes to be read
 * @return whether the read was successful
 */
int hal_io_read_into(uint8_t *buffer, size_t length);

/**
 * @brief Read at most "length" bytes from the interface into a buffer
 *
 * The user must assert that the current buffer is large enough to contain
 * "length" bytes.
 *
 * @param buffer The target buffer to be read into
 * @param length Size of the buffer in bytes
 * @param bytes_read number of bytes read into the buffer
 * @return whether the read was successful
 */
int hal_io_read_chunk(uint8_t *buffer, size_t length, size_t *bytes_read);

/**
 * @brief hal_io_read_raw Read one byte from the input interface
 * @return the received input value
 */
uint8_t hal_io_read_raw(void);

/**
 * @brief hal_io_lock_com_semaphore Lock the communication semaphore
 *
 * So no other component of uPCN can use the communication interface
 */
void hal_io_lock_com_semaphore(void);

/**
 * @brief hal_io_try_lock_com_semaphore Try to lock the communication semaphore
 *					within a specific time range
 * @param ms_timeout The timeout in milliseconds
 * @return Wether the lock attempt was successful
 */
uint8_t hal_io_try_lock_com_semaphore(uint16_t ms_timeout);

/**
 * @brief hal_io_lock_com_semaphore Unlock the communication semaphore
 *
 * So other components of uPCN can use the communication interface again.
 */
void hal_io_unlock_com_semaphore(void);

/**
 * @brief hal_io_send_packet Send a complete packet via the output interface
 * @param data The payload data
 * @param length The length of the payload data (in bytes)
 * @param type The packet type
 */
void hal_io_send_packet(const void *data,
			const size_t length,
			const enum comm_type type);

/**
 * @brief hal_io_begin_packet Begin the transmission of a new packet via the
 *			      output interface
 * @param length The length of the payload data (necessary for the header)
 * @param type The packet type
 */
void hal_io_begin_packet(const size_t length, const enum comm_type type);

/**
 * @brief hal_io_send_packet_data Send the actual payload after a new
 *				  packet transmission has been started by
 *				  hal_io_begin_packet()
 * @param data The payload data
 * @param length The length of the payload data of this function call (bytes)
 */
void hal_io_send_packet_data(const void *data, const size_t length);

/**
 * @brief hal_io_end_packet End the transmission of a packet
 */
void hal_io_end_packet(void);

/**
 * @brief hal_io_output_data_waiting Provides information about the output
 *				     buffer state
 * @return Whether there are items remaining in the output queue
 */
uint8_t hal_io_output_data_waiting(void);

/**
 * @brief hal_io_exit Flushes the output buffer of all connections and ends
 *		      all IO activity
 * @return Whether the closing of the connections was successful
 */
int hal_io_exit(void);

#endif /* HAL_IO_H_INCLUDED */
