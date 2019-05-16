#ifndef CLA_DEFINES_H_INCLUDED
#define CLA_DEFINES_H_INCLUDED

#include <netinet/in.h>
#include <hal_semaphore.h>
#include "upcn/config.h"

#define CLA_TCP_LEGACY_PORT 4200

/* number of concurrent pending connections */
#define IO_SOCKET_BACKLOG 3

/* Channels of the CLA, currently limits the count of concurrent contacts */
#define CLA_CHANNELS 1


enum transmission_state {TM_INACTIVE, TM_ACTIVE, TM_ERROR};

/* Packet data structure */
struct cla_packet {
	uint8_t sndbuffer[BUNDLE_QUOTA];
	uint8_t *packet_position_ptr;
	enum transmission_state state;
	size_t length;
};


struct cla_config {
	/* The handle for an established connection */
	int socket_identifier;

	/* Information about the current contact */
	struct contact *contact;

	/*
	 * A semaphore to ensure mutual exclusion when modifying critical
	 * information
	 */
	Semaphore_t cla_semaphore;

	/* Communication semaphores for both directions */
	Semaphore_t cla_com_tx_semaphore;
	Semaphore_t cla_com_rx_semaphore;

	/* Connection state information */
	bool connection_established;

	struct cla_packet *packet;
};

struct cla_task_tx {
	Task_t tx_task;
	QueueIdentifier_t tx_queue;
	struct cla_config *config;
	Semaphore_t sem_wait_for_shutdown;
};

typedef struct cla_task_tx cla_handler;
typedef struct cla_config cla_conf;

cla_handler worker_tasks[CLA_CHANNELS];

#endif /* CLA_DEFINES_H_INCLUDED */
