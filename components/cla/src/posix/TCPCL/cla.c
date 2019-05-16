#include <cla.h>
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
#include <cla_contact_tx_task.h>
#include "upcn/routingTable.h"
#include "upcn/init.h"

static int socket_fd;
static int socket_conn_fd;
static struct sockaddr_in server;
static struct cla_config log_cla_config;
static uint16_t io_socket_port;

struct cla_config *cla_allocate_cla_config()
{
	return malloc(sizeof(struct cla_config));
}

const char *cla_get_name(void)
{
	return "TCPCL";
}

static int cla_hand_to_contact(struct cla_config *config, char *eid)
{
	struct ground_station_list *gs_list = routing_table_get_station_list();

	// Traverse gs_list and check for matching eid.
	// FIXME: Thread safety - we already _have_ a semaphore for that ...
	while (gs_list != NULL && gs_list->station != NULL) {

		if (gs_list->station != NULL &&
		    gs_list->station->eid != NULL &&
		    !strcmp(eid, gs_list->station->eid)) {
			LOG("Found matching EID in existing gs list.");
			ASSERT(gs_list->station->cla_config != NULL);
			struct cla_config *conf = gs_list->station->cla_config;

			if (conf->state == CLA_STATE_SCHEDULED) {
				LOG("Assigning connection to active contact.");
				/* GS has already active contact, assign
				 * connection.
				 */
				hal_semaphore_take_blocking(
							conf->cla_semaphore);
				conf->socket_identifier =
						config->socket_identifier;
				conf->connection_established = true;
				hal_semaphore_release(
						conf->cla_com_tx_semaphore);
				hal_semaphore_release(
						conf->cla_com_rx_semaphore);
				hal_semaphore_release(conf->cla_semaphore);
				return 0; /* Contact was matched successfully.*/
			}
		}
		gs_list = gs_list->next;
	}
	LOG("Couldn't find matching contact in existing gs list.");
	LOG("Will create opportunistic contact rx task.");
	cla_create_oppo_rx_task(config->socket_identifier);

	return 0;
}



int cla_global_setup(void)
{
	struct sdnv_state header_sdnv_state;
	uint32_t header_sdnv_value;
	struct cla_config *conf = cla_allocate_cla_config();

	LOG("GlobalInputManager: Started successfully.");

	cla_init_config_struct(conf);
	cla_init_config_struct(&log_cla_config);
	cla_unlock_com_rx_semaphore(&log_cla_config);
	cla_unlock_com_tx_semaphore(&log_cla_config);
	log_cla_config.state = CLA_STATE_IGNORE;

	/* Create a socket. */
	socket_fd = socket(IO_SOCKET_DOMAIN, SOCK_STREAM, 0);
	if (socket_fd == -1) {
		LOG("Creating a socket failed.");
		exit(EXIT_FAILURE);
	}
	LOG("Socket created successfully.");

	/* Enable the immediate reuse of a previously closed socket. */
	int enable = 1;

	if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &enable,
		       sizeof(int)) < 0) {
		LOG("setsockopt(SO_REUSEADDR) failed");
		exit(EXIT_FAILURE);
	}

	/* Bind socket to a port. */
	if (!io_socket_port)
		io_socket_port = CLA_TCPCL_PORT;
	server.sin_family = IO_SOCKET_DOMAIN;
	server.sin_addr.s_addr = INADDR_ANY;
	server.sin_port = htons(io_socket_port);

	int error_code;

	if (bind(socket_fd, (struct sockaddr *) &server, sizeof(server)) < 0) {
		error_code = errno;
		LOG("Binding the socket to a port failed.");

		if (error_code == EADDRINUSE)
			LOG("Address already in use. Aborting.");
		exit(EXIT_FAILURE);
	}
	LOGA("Binding to port was successful.", io_socket_port, LOG_NO_ITEM);

	/* Listen for incoming connections. */
	if (listen(socket_fd, IO_SOCKET_BACKLOG) < 0) {
		LOG("Listening to socket failed.");
		exit(EXIT_FAILURE);
	}

	while (true) {
		/* First, accept all incoming connections. */
		socket_conn_fd = accept(socket_fd, NULL, NULL);

		conf->socket_identifier = socket_conn_fd;
		conf->state = CLA_STATE_INACTIVE;
		conf->contact = NULL;

		cla_unlock_com_tx_semaphore(conf);

		cla_send_tcpl_header_packet(conf);

		cla_unlock_com_rx_semaphore(conf);

		if (cla_read_raw(conf) != 0x64 ||
			cla_read_raw(conf) != 0x74 ||
			cla_read_raw(conf) != 0x6e ||
			cla_read_raw(conf) != 0x21) {

			/* Remote did not send dtn! magic. */
			/* Close connection. */
			LOG("Remote did not send dtn! magic.");
			LOG("Close connection.");
			close(socket_conn_fd);
			/* Lock the connection mutex to stop communication. */
			cla_lock_com_rx_semaphore(conf);
		} else {
			/* Dump received header packet. */
			for (unsigned int t = 0; t < 4; t++) {
				if (cla_read_raw(conf) < 0) {
					LOG("Error reading header, " \
					    "closing connection.");
					close(socket_conn_fd);
					continue;
				}
			}

			/* Read size of receiving EID. */
			sdnv_reset(&header_sdnv_state);
			header_sdnv_value = 0;

			while (header_sdnv_state.status == SDNV_IN_PROGRESS) {
				int tmp = cla_read_raw(conf);

				if (tmp < 0) {
					LOG("Error reading EID length, " \
					    "closing connection.");
					close(socket_conn_fd);
					continue;
				}
				sdnv_read_u32(&header_sdnv_state,
					      &header_sdnv_value,
					      (uint8_t)tmp);
			}
			if (header_sdnv_state.status == SDNV_ERROR) {
				LOG("Error reading EID length, " \
				    "closing connection.");
				close(socket_conn_fd);
				continue;
			}

			char *eid = malloc(header_sdnv_value + 1);

			/* read received eid from header packet */
			if (cla_read_into(conf, (uint8_t *)eid,
					  header_sdnv_value)
					!= RETURN_SUCCESS) {
				LOG("Error reading EID, closing connection.");
				close(socket_conn_fd);
				free(eid);
				continue;
			}
			eid[header_sdnv_value] = 0;

			LOGF("Connection on TCPCL level established \'%s\'.",
			     eid);

			cla_hand_to_contact(conf, eid);
		}
	}
	/* Should never return. */
	return -1;
}

inline bool cla_is_connection_oriented(void)
{
	return true;
}

int cla_init_config_struct(struct cla_config *config)
{
	config->cla_semaphore = hal_semaphore_init_mutex();
	config->cla_com_rx_semaphore = hal_semaphore_init_mutex();
	config->cla_com_tx_semaphore = hal_semaphore_init_mutex();
	config->connection_established = false;
	config->state = CLA_STATE_INACTIVE;
	config->socket_identifier = -1;
	config->cla_peer_addr.sin_family = AF_UNSPEC;

	hal_semaphore_release(config->cla_semaphore);

	/* Return success. */
	return 0;
}

struct cla_task_pair *cla_create_contact_handler(struct cla_config *config,
						 int val)
{
	struct cla_task_pair *pair = malloc(sizeof(struct cla_task_pair));
	struct cla_contact_tx_task_creation_result res;

	pair->config = config;
	pair->sem_wait_for_shutdown = hal_semaphore_init_mutex();
	pair->rx_task = cla_launch_contact_rx_task(pair);
	ASSERT(pair->rx_task != NULL);
	res = cla_launch_contact_tx_task(get_global_router_signaling_queue(),
					 pair);
	ASSERT(res.result == UPCN_OK);
	pair->tx_task = res.task_handle;
	pair->tx_queue = res.queue_handle;

	hal_semaphore_take_blocking(pair->config->cla_semaphore);
	pair->config->state = CLA_STATE_SCHEDULED;
	hal_semaphore_release(pair->config->cla_semaphore);

	return pair;
}

int cla_remove_scheduled_contact(cla_handler *handler)
{
	if (!handler)
		return 0;
	return cla_set_contact_task_pair_opportunistic(handler);
}

struct cla_config *cla_get_debug_cla_handler(void)
{
	return &log_cla_config;
}

int cla_exit(void)
{
	/* Always false as the pairs handle their sockets on their own. */
	return -1;
}

void cla_init(uint16_t port)
{
	io_socket_port = port;
}
