#ifndef INIT_H_INCLUDED
#define INIT_H_INCLUDED

#include <stdint.h>
#include "hal_types.h"

/**
 * @brief init
 * @param io_socket_port the port which will be used to open the socket;
 *                       0 is default value, in this case the default port
 *                       is used
 */
void init(uint16_t io_socket_port);
void start_tasks(void);
int start_os(void);
QueueIdentifier_t get_global_router_signaling_queue(void);
QueueIdentifier_t get_global_bundle_signaling_queue(void);

#endif /* INIT_H_INCLUDED */
