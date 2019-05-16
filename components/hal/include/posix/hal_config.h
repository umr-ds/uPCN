/*
 * hal_config.h
 *
 * Description: contains platform-specific configuration information
 *
 */

#ifndef HAL_CONFIG_H_INCLUDED
#define HAL_CONFIG_H_INCLUDED

#include <stdlib.h>
#include <sys/socket.h>


#define RETURN_SUCCESS EXIT_SUCCESS
#define RETURN_FAILURE EXIT_FAILURE


/* THREAD CONFIGURATION */

#define CONTACT_RX_TASK_PRIORITY 2
#define ROUTER_TASK_PRIORITY 2
#define BUNDLE_PROCESSOR_TASK_PRIORITY 2
#define CONTACT_MANAGER_TASK_PRIORITY 1
#define CONTACT_TX_TASK_PRIORITY 3
#define ROUTER_OPTIMIZER_TASK_PRIORITY 0

/* 0 means inheriting the stack size from the parent task */
#define DEFAULT_TASK_STACK_SIZE 0
#define CONTACT_RX_TASK_STACK_SIZE 0
#define CONTACT_MANAGER_TASK_STACK_SIZE 0
#define ROUTER_OPTIMIZER_TASK_STACK_SIZE 0
#define CONTACT_TX_TASK_STACK_SIZE 0


/* IO CONFIGURATION */

/* for test purposes use the IPv4-Domain */
#define IO_SOCKET_DOMAIN AF_INET

/* maximum length of the cached commandline argument */
#define CMD_ARG_MAX_LENGTH 10


/* DEBUG CONFIGURATION */

/* log into a logfile */
#define DEBUG_LOG_FILE 1
#define DEBUG_LOG_FILE_PATH "upcn.log"
#define DEBUG_PRINTF_BUFFER 500

/* show the log output on stdout */
#define DEBUG_STD_OUT 1


/* PLATFORM CONFIGURATION */
#define LINUX_SPECIFIC_API 0

#endif /* HAL_CONFIG_H_INCLUDED */
