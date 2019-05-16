/*
 * hal_types.h
 *
 * Description: maps the generic abstraction types of the HAL to
 *              architecture-dependent types
 *
 */

#ifndef HAL_TYPES_H_INCLUDED
#define HAL_TYPES_H_INCLUDED

#include <sys/types.h>
#include <semaphore.h>
#include <fcntl.h>
#include <simple_queue.h>
#include <pthread.h>

#define QueueIdentifier_t Queue_t*
#define Semaphore_t sem_t*
#define Task_t pthread_t*

#endif /* HAL_TYPES_H_INCLUDED */
