#ifndef CLA_CONTACT_RX_TASK_H_INCLUDED
#define CLA_CONTACT_RX_TASK_H_INCLUDED

#include <cla_defines.h>

void cla_contact_rx_task(void *param);

struct cla_contact_rx_task_parameters {
	QueueIdentifier_t router_signaling_queue;
	QueueIdentifier_t bundle_signaling_queue;
	cla_handler *pair;
};

/**
 * @brief cla_launch_contact_rx_task Creates a new contact_rx_task with the
 *					configuration struct config
 * @param config The aforementioned configuration struct that also contains
 *					the socket identifier
 * @return a handle for the created task
 */
Task_t cla_launch_contact_rx_task(cla_handler *pair);

#endif /* CLA_CONTACT_RX_TASK_H_INCLUDED */

