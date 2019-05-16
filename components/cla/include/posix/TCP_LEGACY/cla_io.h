#ifndef CLA_IO_H_INCLUDED
#define CLA_IO_H_INCLUDED

#include <cla_defines.h>
#include <hal_io.h>

/**
 * @brief cla_lock_com_tx_semaphore Lock the communication semaphore.
 *
 * Thus, no further other component of uPCN can use the communication interface.
 */
void cla_lock_com_tx_semaphore(struct cla_config *config);

/**
 * @brief cla_lock_com_tx_semaphore Unlock the communication semaphore.
 *
 * Thus, further components of uPCN can use the communication interface again.
 */
void cla_unlock_com_tx_semaphore(struct cla_config *config);

/**
 * @brief cla_lock_com_rx_semaphore Lock the communication semaphore.
 *
 * Thus, no further other component of uPCN can use the communication interface.
 */
void cla_lock_com_rx_semaphore(struct cla_config *config);

/**
 * @brief cla_lock_com_tx_semaphore Unlock the communication semaphore.
 *
 * Thus, further components of uPCN can use the communication interface again.
 */
void cla_unlock_com_rx_semaphore(struct cla_config *config);

/**
 * @brief cla_begin_packet Begin the transmission of a new packet via the
 *			      output interface.
 * @param length The length of the payload data (necessary for the header).
 * @param type The packet type.
 */
void cla_begin_packet(struct cla_config *config,
		      const size_t length, const enum comm_type type);

/**
 * @brief cla_end_packet End the transmission of a packet.
 */
void cla_end_packet(struct cla_config *config);

/**
 * @brief cla_send_packet_data Send the actual payload after a new
 *				  packet transmission has been started by
 *				  cla_begin_packet().
 * @param data The payload data.
 * @param length The length of the payload of this function call (in bytes).
 */
void cla_send_packet_data(const void *config,
			  const void *data, const size_t length);

/**
 * @brief cla_write_raw Write raw data of a specific length to the
 *			      output interface.
 * @param data The raw data.
 * @param length The length of the raw data (in bytes).
 */
void cla_write_raw(struct cla_config *config, void *data, size_t length);

/**
 * @brief cla_write_string Write a string to the output interface.
 * @param string
 */
void cla_write_string(struct cla_config *config, const char *string);

/**
 * @brief cla_read_into Read several bytes from the interface into a buffer.
 * @param buffer The target buffer to be read to.
 * @param length The count of bytes to be read.
 * @return Specifies if the read was successful.
 */
int16_t cla_read_into(struct cla_config *config,
		      uint8_t *buffer, size_t length);

/**
 * @brief Read at most "length" bytes from the interface into a buffer.
 *
 * The user must assert that the current buffer is large enough to contain
 * "length" bytes.
 *
 * @param buffer The target buffer to be read to.
 * @param length Size of the buffer in bytes.
 * @param bytes_read Number of bytes read into the buffer.
 * @return Specifies if the read was successful.
 */
int16_t cla_read_chunk(struct cla_config *config,
		   uint8_t *buffer,
		   size_t length,
		   size_t *bytes_read);

/**
 * @brief cla_read_raw Read one byte from the input interface.
 * @return The received input value.
 */
int16_t cla_read_raw(struct cla_config *config);

/**
 * @brief cla_try_lock_com_tx_semaphore Try to lock the communication semaphore
 *					within a specific time range.
 * @param ms_timeout The timeout in milliseconds.
 * @return Specifies if the lock attempt was successful.
 */
int16_t cla_try_lock_com_tx_semaphore(struct cla_config *config,
				      uint16_t ms_timeout);

/**
 * @brief cla_try_lock_com_rx_semaphore Try to lock the communication semaphore
 *					within a specific time range.
 * @param ms_timeout The timeout in milliseconds.
 * @return Specifies if the lock attempt was successful.
 */
int16_t cla_try_lock_com_rx_semaphore(struct cla_config *config,
				      uint16_t ms_timeout);

/**
 * @brief cla_send_packet Send a complete packet via the output interface.
 * @param data The payload data.
 * @param length The length of the payload data (in bytes).
 * @param type The packet type.
 */
void cla_send_packet(struct cla_config *config,
		     const void *data,
		     const size_t length,
		     const enum comm_type type);

/**
 * @brief cla_output_data_waiting Provides information about the output
 *				     buffer state.
 * @return Specifies if there are items remaining in the output queue.
 */
int16_t cla_output_data_waiting(struct cla_config *config);

/**
 * @brief cla_send_tcpl_header_packet Send the required header packet to
 *					the communication peer.
 * @param config The configuration of the current contact.
 * @return 0 on success, ERROR-Code otherwise.
 */
int16_t cla_send_tcpl_header_packet(struct cla_config *config);

int16_t cla_io_init(void);

void cla_io_listen(struct cla_config *config, uint16_t port);

void cla_io_exit(void);

#endif /*CLA_IO_H_INCLUDED*/
