/*
 * hal_semaphore.c
 *
 * Description: contains the stm32-implementation of the hardware
 * abstraction layer interface for semaphore-related functionality
 *
 */

#include <assert.h>
#include <FreeRTOS.h>
#include <queue.h>
#include <stdlib.h>
#include <stdio.h>
#include "hal_semaphore.h"


Semaphore_t hal_semaphore_init_binary(void)
{
	return xSemaphoreCreateBinary();
}

Semaphore_t hal_semaphore_init_mutex(void)
{
	return xSemaphoreCreateMutex();
}

void hal_semaphore_take_blocking(Semaphore_t sem)
{
	while (pdFALSE == xSemaphoreTake(sem, portMAX_DELAY))
		;
}

void hal_semaphore_release(Semaphore_t sem)
{
	xSemaphoreGive(sem);
}


void hal_semaphore_poll(SemaphoreHandle_t sem)
{
	xSemaphoreTake(sem, 0);
}

void hal_semaphore_delete(Semaphore_t sem)
{
	vSemaphoreDelete(sem);
}
