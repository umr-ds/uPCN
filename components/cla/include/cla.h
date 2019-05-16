#ifndef CLA_H_INCLUDED
#define CLA_H_INCLUDED

#include <stdint.h>
#include <stdbool.h>

#include <cla_defines.h>
#include "upcn/groundStation.h"
#include <hal_io.h>

struct cla_config *cla_allocate_cla_config();

/**
 * @brief cla_global_setup Initialization of underlying OS/HW for I/O
 * @return EXIT_SUCCESS or EXIT_FAILURE (macros are resolved to convention of
 *         the underlying OS infrastructure
 */
int cla_global_setup(void);

bool cla_is_connection_oriented(void);

int cla_init_config_struct(struct cla_config *config);

cla_handler *cla_create_contact_handler(struct cla_config *config, int val);

int cla_remove_scheduled_contact(cla_handler *handler);

struct cla_config *cla_get_debug_cla_handler(void);

const char *cla_get_name(void);

void cla_init(uint16_t port);

/**
 * @brief cla_exit Flushes the output buffer of all connections and ends
 *		      all IO activity
 * @return Whether the closing of the connections was successful
 */
int cla_exit(void);


#endif /* CLA_H_INCLUDED */
