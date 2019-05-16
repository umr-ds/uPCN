/*
 * hal_config.h
 *
 * Description: contains platform-specific configuration information
 *
 */

#ifndef HAL_CONFIG_H_INCLUDED
#define HAL_CONFIG_H_INCLUDED

#define RETURN_SUCCESS pdTRUE
#define RETURN_FAILURE pdFALSE

#define CONTACT_RX_TASK_PRIORITY 2
#define ROUTER_TASK_PRIORITY 2
#define BUNDLE_PROCESSOR_TASK_PRIORITY 2
#define CONTACT_MANAGER_TASK_PRIORITY 1
#define CONTACT_TX_TASK_PRIORITY 3
#define ROUTER_OPTIMIZER_TASK_PRIORITY 0

/* hal_debug_printf() should use less stack now */
#define DEFAULT_TASK_STACK_SIZE 768
/* XXX To be measured */
/* TODO: Needs to be > 512 because of SGP-4 (RRND) init in input task */
#define CONTACT_RX_TASK_STACK_SIZE 2048
/* NOTE Increased for RRND */
#define CONTACT_MANAGER_TASK_STACK_SIZE 2048
/* To be measured */
#define ROUTER_OPTIMIZER_TASK_STACK_SIZE 512
/* Max stack depth measured @ bundle_print_stack_conserving(): ~120 B */
#define CONTACT_TX_TASK_STACK_SIZE 160

#endif /* HAL_CONFIG_H_INCLUDED */
