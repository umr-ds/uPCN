#include <cla_management.h>
#include <cla.h>
#include <cla_defines.h>
#include <cla_io.h>
#include "upcn/init.h"
#include <cla_contact_rx_task.h>
#include <cla_contact_tx_task.h>
#include <pthread.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include "upcn/sdnv.h"

struct cla_task_pair *cla_create_oppo_rx_task(int socket_identifier)
{
	struct cla_task_pair *pair = malloc(sizeof(struct cla_task_pair));
	struct cla_config *oppo_conf = cla_allocate_cla_config();

	cla_init_config_struct(oppo_conf);
	hal_semaphore_take_blocking(oppo_conf->cla_semaphore);
	oppo_conf->state = CLA_STATE_OPPO_RX_ONLY;
	oppo_conf->socket_identifier = socket_identifier;
	oppo_conf->connection_established = true;
	hal_semaphore_release(oppo_conf->cla_semaphore);
	hal_semaphore_release(oppo_conf->cla_com_rx_semaphore);
	pair->config = oppo_conf;
	pair->sem_wait_for_shutdown = hal_semaphore_init_mutex();

	pair->rx_task = cla_launch_contact_rx_task(pair);
	pair->tx_task = NULL;

	return pair;
}

int cla_set_contact_task_pair_opportunistic(struct cla_task_pair *pair)
{
	hal_semaphore_take_blocking(pair->config->cla_semaphore);

	if (!pair->config->connection_established) {
		hal_semaphore_release(pair->config->cla_semaphore);
		return cla_kill_contact_task_pair(pair);
	}

	pair->config->state = CLA_STATE_OPPORTUNISTIC;

	if (CLA_OPPO_TIMEOUT_SEC) {
		struct timeval tv;

		tv.tv_sec = CLA_OPPO_TIMEOUT_SEC;  /* Timeout */
		tv.tv_usec = 0;
		setsockopt(pair->config->socket_identifier,
			   SOL_SOCKET, SO_RCVTIMEO,
			   (const char *)&tv,
			   sizeof(struct timeval));
		setsockopt(pair->config->socket_identifier,
			   SOL_SOCKET, SO_SNDTIMEO,
			   (const char *)&tv,
			   sizeof(struct timeval));
	}
	hal_semaphore_release(pair->config->cla_semaphore);

	LOG("Set contact pair to opportunistic.");

	return 0;
}


int cla_kill_contact_task_pair(struct cla_task_pair *pair)
{
	Task_t rx_task;
	Task_t tx_task;

	hal_semaphore_take_blocking(pair->config->cla_semaphore);

	rx_task = pair->rx_task;
	tx_task = pair->tx_task;

	cla_disconnect(pair->config);

	hal_semaphore_release(pair->config->cla_semaphore);

	const enum cla_state prev_state = pair->config->state;
	// Restore state associated to GS CLA config to allow further contacts
	pair->config->state = CLA_STATE_INACTIVE;

	if (prev_state == CLA_STATE_OPPO_RX_ONLY) {
		hal_semaphore_delete(pair->sem_wait_for_shutdown);
		free(pair);
		ASSERT(rx_task != NULL);
		pthread_cancel(*rx_task);

	} else if (prev_state == CLA_STATE_OPPORTUNISTIC) {
		ASSERT(rx_task != NULL);
		ASSERT(tx_task != NULL);
		pthread_cancel(*tx_task);
		hal_semaphore_delete(pair->sem_wait_for_shutdown);
		free(pair);
		pthread_cancel(*rx_task);
	} else {
		ASSERT(rx_task != NULL);
		ASSERT(tx_task != NULL);
		pthread_cancel(*tx_task);
		hal_semaphore_delete(pair->sem_wait_for_shutdown);
		free(pair);
		pthread_cancel(*rx_task);
	}

	LOG("Killed contact pair.");

	return 0;
}

int cla_ensure_connection(struct cla_config *config)
{
	int optval;
	int socket_desc;
	unsigned int length = sizeof(optval);
	struct sockaddr_in server;
	char adr[15];
	long int port;
	struct sdnv_state header_sdnv_state;
	uint32_t header_sdnv_value;

	/* check if socket descriptor is valid */
	if (getsockopt(config->socket_identifier, SOL_SOCKET, SO_REUSEADDR,
		   &optval, &length) == 0)
		/* socket descriptor already exists --> nothing to do here! */
		return 0;

	if (config->state != CLA_STATE_SCHEDULED)
		return -1;

	for (;;) {

		//Create socket
		socket_desc = socket(AF_INET, SOCK_STREAM, 0);
		ASSERT(socket_desc != -1);

		if (config->cla_peer_addr.sin_family != AF_INET) {
			ASSERT(config->contact != NULL);
			ASSERT(sscanf(
				config->contact->ground_station->cla_addr,
				"%15[^:]:%ld",
				adr,
				&port) == 2);

			config->cla_peer_addr.sin_addr.s_addr = inet_addr(adr);
			config->cla_peer_addr.sin_family = AF_INET;
			config->cla_peer_addr.sin_port = htons(port);
		}

		//Connect to remote server
		if (connect(socket_desc,
			    (struct sockaddr *)&config->cla_peer_addr,
			    sizeof(server)) < 0) {
			LOGF(
				"Cannot connect to contact cla address %s, " \
				"trying again in 1 second!",
				config->contact->ground_station->cla_addr
			);
			sleep(1);
			continue; // try again
		} else {
			config->socket_identifier = socket_desc;
			cla_unlock_com_tx_semaphore(config);
			cla_send_tcpl_header_packet(config);
			cla_unlock_com_rx_semaphore(config);

			if (cla_read_raw(config) != 0x64 ||
				cla_read_raw(config) != 0x74 ||
				cla_read_raw(config) != 0x6e ||
				cla_read_raw(config) != 0x21) {

				/* remote did not send dtn! magic */
				/* close connection */
				LOG("Remote did not send dtn! magic.");
				LOG("Close connection!");
				close(config->socket_identifier);
				continue; // try again
			}
		}

		LOG("Got dtn! magic!");

		/* dump received header packet */
		for (unsigned int t = 0; t < 4; t++) {
			if (cla_read_raw(config) < 0) {
				LOG("Error reading header, " \
				    "closing connection.");
				close(config->socket_identifier);
				continue;
			}
		}

		/* read size of receiving EID */
		sdnv_reset(&header_sdnv_state);
		header_sdnv_value = 0;

		while (header_sdnv_state.status == SDNV_IN_PROGRESS) {
			int tmp = cla_read_raw(config);

			if (tmp < 0) {
				LOG("Error reading EID length, " \
				    "closing connection.");
				close(config->socket_identifier);
				continue;
			}
			sdnv_read_u32(&header_sdnv_state,
				&header_sdnv_value,
				(uint8_t)tmp);
		}
		if (header_sdnv_state.status == SDNV_ERROR) {
			LOG("Error reading EID length, " \
			    "closing connection.");
			close(config->socket_identifier);
			continue;
		}

		char *eid = malloc(header_sdnv_value + 1);

		/* read received eid from header packet */
		if (cla_read_into(config, (uint8_t *)eid, header_sdnv_value)
				!= RETURN_SUCCESS) {
			LOG("Error reading EID, closing connection.");
			close(config->socket_identifier);
			free(eid);
			continue;
		}
		eid[header_sdnv_value] = 0;

		LOGF("Received EID is %s.", eid);
		free(eid);

		LOG("Scheduled contact established on TCPCL level.");
		config->connection_established = true;
		return 0;
	}
}

int cla_disconnect(struct cla_config *config)
{
	int rv = 0;

	if (config->connection_established &&
			close(config->socket_identifier) != 0) {
		LOG("Closed an invalid socket.");
		rv = -1;
	}
	config->socket_identifier = -1;
	config->connection_established = false;

	return rv;
}

int cla_wait_for_rx_connection(struct cla_config *config)
{
	while (!config->connection_established) {
		hal_semaphore_take_blocking(config->cla_com_rx_semaphore);
		hal_semaphore_release(config->cla_com_rx_semaphore);
	}
	return 1;
}

