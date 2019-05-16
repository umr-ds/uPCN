#ifndef CLA_MANAGEMENT_H_INCLUDED
#define CLA_MANAGEMENT_H_INCLUDED

#include <cla_defines.h>

int cla_kill_contact_task_pair(struct cla_task_pair *pair);


int cla_assign_contact_to_task(struct cla_task_pair *pair,
			       struct contact *contact);

/**
 * @brief cla_create_oppo_rx_task Creates an contact_rx_task to
 *					opportunistically receive data
 * @param socket_identifier The socket_identifier of the already opened
 *					connection
 * @return a pointer to a cla_task_pair object containing a handle to the rx
 *		task
 */
struct cla_task_pair *cla_create_oppo_rx_task(int socket_identifier);

int cla_ensure_connection(struct cla_config *config);

int cla_disconnect(struct cla_config *config);

int cla_wait_for_rx_connection(struct cla_config *config);

int cla_set_contact_task_pair_opportunistic(struct cla_task_pair *pair);

#endif /*CLA_MANAGEMENT_H_INCLUDED*/
