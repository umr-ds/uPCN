#ifndef CLA_DEFINES_H_INCLUDED
#define CLA_DEFINES_H_INCLUDED

#include <hal_semaphore.h>
#include "upcn/config.h"

/* Channels of the CLA, currently limits the count of concurrent contacts */
#define CLA_CHANNELS 1

struct cla_config {
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

	struct cla_packet *packet;
};

struct cla_task_tx{
	Task_t tx_task;
	QueueIdentifier_t tx_queue;
	struct cla_config *config;
	Semaphore_t sem_wait_for_shutdown;
};

typedef struct cla_task_tx cla_handler;
typedef struct cla_config cla_conf;

cla_handler worker_tasks[CLA_CHANNELS];

#endif /* CLA_DEFINES_H_INCLUDED */
