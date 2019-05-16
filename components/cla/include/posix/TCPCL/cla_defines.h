#ifndef CLA_DEFINES_H_INCLUDED
#define CLA_DEFINES_H_INCLUDED

#include <netinet/in.h>
#include <hal_semaphore.h>
#include "upcn/config.h"

#define CLA_TCPCL_PORT 4556

/* Opportunistic read timeout - if set to 0, disables the timeout. */
#define CLA_OPPO_TIMEOUT_SEC 0

/* number of concurrent pending connections */
#define IO_SOCKET_BACKLOG 3

/* Channels of the CLA, currently limits the count of concurrent contacts */
#define CLA_CHANNELS 100

enum cla_state {
	CLA_STATE_SCHEDULED = 0x01,
	CLA_STATE_OPPORTUNISTIC = 0x02,
	CLA_STATE_INACTIVE = 0x03,
	CLA_STATE_OPPO_RX_ONLY = 0x04,
	CLA_STATE_IGNORE = 0x05
};

enum transmission_state {TM_INACTIVE, TM_ACTIVE, TM_ERROR};

/* Packet data structure */
struct cla_packet {
	uint8_t sndbuffer[BUNDLE_QUOTA];
	uint8_t *packet_position_ptr;
	enum transmission_state state;
	size_t length;
};


struct cla_config {
	/* The handle for an established connection. */
	int socket_identifier;

	/* The address information for establishing a connection. */
	struct sockaddr_in cla_peer_addr;

	/* Information about the current contact. */
	struct contact *contact;

	/*
	 * A semaphore to ensure mutual exclusion when modifying critical
	 * information.
	 */
	Semaphore_t cla_semaphore;

	/* Communication semaphores for both directions. */
	Semaphore_t cla_com_tx_semaphore;
	Semaphore_t cla_com_rx_semaphore;

	/* Connection state information */
	bool connection_established;

	/* General state */
	enum cla_state state;

	struct cla_packet *packet;
};

struct cla_task_pair{
	Task_t rx_task;
	Task_t tx_task;
	QueueIdentifier_t tx_queue;
	struct cla_config *config;
	Semaphore_t sem_wait_for_shutdown;
};

typedef struct cla_task_pair cla_handler;

#endif /* CLA_DEFINES_H_INCLUDED */
