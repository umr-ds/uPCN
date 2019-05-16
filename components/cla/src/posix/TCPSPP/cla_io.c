#include <cla_io.h>
#include <cla_defines.h>
#include <hal_task.h>
#include <cla_contact_rx_task.h>
#include <cla_management.h>
#include <cla_io.h>

#include "hal_time.h"

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

static struct cla_packet snd_packet;
static struct sockaddr_in server;
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static Semaphore_t comm_semaphore;


static void disconnect_handler(int signal);

static struct cla_config *conf;
static int bind_sock;

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
	LOG("tcpspp: waiting for com rx semaphore");
	hal_semaphore_take_blocking(config->cla_com_rx_semaphore);
	LOG("tcpspp: locked com rx semaphore");
}

void cla_unlock_com_rx_semaphore(struct cla_config *config)
{
	hal_semaphore_release(config->cla_com_rx_semaphore);
	LOG("tcpspp: unlocked com rx semaphore");
}

void cla_begin_packet(struct cla_config *config,
		      const size_t length, const enum comm_type type)
{
	LOGF("tcpssp: %s(..., %zu, %d)", __func__, length, type);

	hal_semaphore_take_blocking(comm_semaphore);

	switch (type) {
	case COMM_TYPE_MESSAGE:
	{
		LOGF("tcpspp: %s(..., %zu, %d): redirecting to stdout",
		     __func__, length, type);
		if (length > sizeof(snd_packet.sndbuffer) - 1) {
			LOG("tcpspp: message too large, dropping.");
			snd_packet.state = TM_DROP;
		} else {
			snd_packet.state = TM_PRINT;
			snd_packet.packet_position_ptr =
					&snd_packet.sndbuffer[0];
			snd_packet.length = length + 1;
		}
		break;
	}
	case COMM_TYPE_BUNDLE:
	{
		size_t spp_length = spp_get_size(config->spp_ctx,
						 length);
		LOGF("tcpssp: %s(..., %zu, %d): spp_length = %zu",
		     __func__, length, type,
		     spp_length);

		if (spp_length > sizeof(snd_packet.sndbuffer)) {
			snd_packet.state = TM_ERROR;
			LOG("tcpspp: packet too large!");
			return;
		}

		struct spp_meta_t metadata;

		metadata.apid = CLA_TCPSPP_APID;
		metadata.is_request = 0;
		metadata.segment_number = 0;
		metadata.segment_status = SPP_SEGMENT_UNSEGMENTED;
		metadata.dtn_timestamp = hal_time_get_timestamp_s();
		metadata.dtn_counter = 0;

		snd_packet.packet_position_ptr = &snd_packet.sndbuffer[0];
		spp_serialize_header(config->spp_ctx, &metadata, length,
				     &snd_packet.packet_position_ptr);

		snd_packet.length = spp_length;
		snd_packet.state = TM_ACTIVE;
		break;
	}
	default:
		LOGF("tcpspp: %s(..., %zu, %d): unsupported type, dropping.",
		     __func__, length, type);
		snd_packet.state = TM_DROP;
	}
}

void cla_end_packet(struct cla_config *config)
{
	switch (snd_packet.state) {
	case TM_ACTIVE:
	{
		if (send(config->socket_identifier, snd_packet.sndbuffer,
			  snd_packet.length, 0) == -1) {
			LOG("An error occured during sending. Data discarded.");
			snd_packet.state = TM_ERROR;
			config->connection_established = false;
		}
	}
	case TM_DROP:
		break;
	case TM_PRINT:
		// ensure that NUL-termination occurs
		// the length has been checked in cla_begin_packet
		*snd_packet.packet_position_ptr++ = '\0';
		LOGF("hal message: %s", snd_packet.sndbuffer);
		break;
	default:
		LOG("An error occured before sending. Restart.");
		break;
	}

	snd_packet.state = TM_INACTIVE;
	hal_semaphore_release(comm_semaphore);
}

void cla_send_packet_data(const void *config,
			  const void *data, const size_t length)
{
	if (snd_packet.state == TM_ERROR) {
		LOG("Transmission in error state. Discarding data. Restart "\
		    "transmission.");
		return;
	} else if (snd_packet.state == TM_INACTIVE) {
		LOG("No transmission started. Start the packet transmission "\
		    "first! Discarding data.");
		return;
	} else if (snd_packet.state == TM_DROP) {
		// drop silently.
		return;
	}

	/* copy the data into the buffer */
	memcpy(snd_packet.packet_position_ptr, data, length);

	/* move the pointer to the next free byte in the buffer */
	snd_packet.packet_position_ptr += length;

}

void cla_write_raw(struct cla_config *config, void *data, size_t length)
{
	LOGF("tcpspp: %s not supported. message: %*s",
	     __func__, data, length);
	// cla_send_packet(config, data, length, COMM_TYPE_MESSAGE);
}

void cla_write_string(struct cla_config *config, const char *string)
{
	LOGF("tcpspp: %s not supported. message: %s",
	     __func__, string);
	// cla_send_packet(config, string, strlen(string), COMM_TYPE_MESSAGE);
}

int16_t cla_read_into(struct cla_config *config, uint8_t *buffer, size_t length)
{
	int ret;

	cla_lock_com_rx_semaphore(config);

	while (length) {
		ret = recv(config->socket_identifier, buffer, length, 0);
		if (ret < 0) {
			LOGI("Error reading from socket.", errno);
			/*
			 * NOTE: Shouldn't we handle errors other
			 * than EAGAIN better?
			 */
			cla_unlock_com_rx_semaphore(config);
			return RETURN_FAILURE;
		} else if (!ret) {
			// the peer has disconnected gracefully
			LOG("The peer has disconnected gracefully!");
			cla_unlock_com_rx_semaphore(config);
			disconnect_handler(SIGPIPE);
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
	cla_lock_com_rx_semaphore(config);

#ifdef THROUGHPUT_TEST
	if (timestamp_initialread == 0)
		timestamp_initialread = hal_time_get_timestamp_us();
#endif
	const ssize_t ret = recv(config->socket_identifier, buffer, length, 0);

	if (ret < 0) {
		LOGI("Error reading from socket!", errno);
		/*
		 * NOTE: Shouldn't we handle errors other
		 * than EAGAIN better?
		 */
		cla_unlock_com_rx_semaphore(config);
		return RETURN_FAILURE;
	} else if (ret == 0) {
		// the peer has disconnected gracefully
		LOG("The peer has disconnected gracefully!");
		cla_unlock_com_rx_semaphore(config);
		disconnect_handler(SIGPIPE);
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
	if (config->connection_established) {
		cla_begin_packet(config, length, type);
		cla_send_packet_data(config, data, length);
		cla_end_packet(config);
	} else {
		LOG("Discarded data because no connection present!");
	}
}

int16_t cla_output_data_waiting(struct cla_config *config)
{
	/* not relevant in POSIX, realised by closing the */
	/* socket properly (in hal_platform/hal_io_exit) */
	return RETURN_FAILURE;
}

void cla_io_listen(struct cla_config *config, uint16_t port)
{
	int enable = 1;

	/* Create a socket */
	bind_sock = socket(IO_SOCKET_DOMAIN, SOCK_STREAM, 0);
	if (bind_sock == -1) {
		LOG("Creating a socket failed!");
		exit(EXIT_FAILURE);
	}
	LOG("Socket created successfully");

	/* Enable the immediate reuse of a previously closed socket. */
	if (setsockopt(bind_sock, SOL_SOCKET, SO_REUSEADDR, &enable,
		       sizeof(int)) < 0) {
		LOG("setsockopt(SO_REUSEADDR) failed");
		exit(EXIT_FAILURE);
	}

	/* Disable the nagle algorithm to prevent delays in responses. */
	if (setsockopt(bind_sock, IPPROTO_TCP, TCP_NODELAY, &enable,
		       sizeof(int)) < 0) {
		LOG("setsockopt(TCP_NODELAY) failed");
		exit(EXIT_FAILURE);
	}

	/* Bind socket to a port. */
	server.sin_family = IO_SOCKET_DOMAIN;
	server.sin_addr.s_addr = INADDR_ANY;
	server.sin_port = htons(port);

	int error_code;

	if (bind(bind_sock, (struct sockaddr *) &server, sizeof(server)) < 0) {
		error_code = errno;
		LOGF("Binding the socket to port %d failed!", port);

		if (error_code == EADDRINUSE)
			LOG("Address already in use! Aborting!");
		exit(EXIT_FAILURE);
	}
	LOGF("Binding to port %d was successful", port);

	/* Listen for incoming connections */
	if (listen(bind_sock, 1) < 0) {
		LOG("Listening to socket failed!");
		exit(EXIT_FAILURE);
	}

	/* lock cla_semaphore (necessary for sleeping till
	 * connection terminates)
	 */
	pthread_mutex_lock(&lock);
	conf = config;

	while (true) {
		/* first, accept all incoming connections */
		config->socket_identifier = accept(bind_sock, NULL, NULL);
		config->connection_established = true;
		LOG("Connection accepted!");

		/* Disable the nagle algorithm to prevent delays in responses */
		if (setsockopt(config->socket_identifier,
			       IPPROTO_TCP,
			       TCP_NODELAY,
			       &enable,
			       sizeof(int)) < 0) {
			LOG("setsockopt(TCP_NODELAY) failed.");
		}

		/* unlock the connection mutex to allow communication */
		LOG("releasing semaphores!");
		hal_semaphore_release(config->cla_com_rx_semaphore);
		hal_semaphore_release(config->cla_com_tx_semaphore);


		/* sleep until the connection is closed */
		pthread_mutex_lock(&lock);

		LOG("Looking for new connection now!");

#ifdef THROUGHPUT_TEST
		hal_debug_printf("\n\nThrouput Test Results\n\n");
		for (int i = 0; i < 47; i++)
			printf("%u\t%lu\n",
			       bundle_size[i],
			       (timestamp_mp2[i]-timestamp_mp1[i])
			       +(timestamp_mp4[i]-timestamp_mp3[i]));
#endif
	}
}

int16_t cla_io_init(void)
{
	struct sigaction sa;

	sa.sa_handler = &disconnect_handler;

	/* Restart the system call, if at all possible */
	sa.sa_flags = SA_RESTART;

	/* Block every signal during the handler */
	sigfillset(&sa.sa_mask);

	/* Intercept SIGPIPE with this handler */
	if (sigaction(SIGPIPE, &sa, NULL) == -1) {
		LOG("Error: cannot handle SIGPIPE.");
		exit(EXIT_FAILURE);
	}

	comm_semaphore = hal_semaphore_init_binary();
	if (comm_semaphore == NULL)
		return EXIT_FAILURE;
	hal_semaphore_release(comm_semaphore);

	/* initialize the header */
	snd_packet.sndbuffer[0] = 0x0A;
	snd_packet.sndbuffer[1] = 0x42;

	return RETURN_SUCCESS;
}

static void disconnect_handler(int signal)
{
	/* unlock the cla_semaphore so we can look for a new connection */
	pthread_mutex_unlock(&lock);

	/* lock the connection mutex so no more communication is possible */
	hal_semaphore_take_blocking(conf->cla_com_rx_semaphore);
	hal_semaphore_take_blocking(conf->cla_com_tx_semaphore);

	conf->connection_established = false;
}

void cla_io_exit(void)
{
	close(bind_sock);
}

