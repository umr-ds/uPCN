/*
 * hal_queue.c
 *
 * Description: contains the stm32-implementation of the hardware
 * abstraction layer interface for thread-related functionality
 *
 */

#include <assert.h>
#include <FreeRTOS.h>
#include <queue.h>
#include <stdlib.h>
#include <stdio.h>
#include "hal_queue.h"


QueueIdentifier_t hal_queue_create(int queue_length, int item_size)
{
	return xQueueCreate(queue_length, item_size);
}


void hal_queue_push_to_back(QueueIdentifier_t queue, const void *item)
{
	while (pdPASS != xQueueSendToBack(queue, item, portMAX_DELAY))
		;
}


uint8_t hal_queue_receive(QueueIdentifier_t queue, void *targetBuffer,
			  int timeout)
{
	/* try indefinitely */
	if (timeout == -1)
		return xQueueReceive(queue, targetBuffer, portMAX_DELAY);

	/* otherwise abort after timeout */
	return xQueueReceive(queue, targetBuffer, timeout/portTICK_PERIOD_MS);
}


void hal_queue_reset(QueueHandle_t queue)
{
	xQueueReset(queue);
}


uint8_t hal_queue_try_push_to_back(QueueIdentifier_t queue,
					const void *item, int timeout)
{
	/* try indefinitely */
	if (timeout == -1)
		return xQueueSendToBack(queue, item, portMAX_DELAY);

	return xQueueSendToBack(queue, item, timeout/portTICK_PERIOD_MS);
}


void hal_queue_delete(QueueIdentifier_t queue)
{
	vQueueDelete(queue);
}


uint8_t hal_queue_override_to_back(QueueIdentifier_t queue, const void *item)
{
	return xQueueOverwrite(queue, item);
}

uint8_t hal_queue_nr_of_items_waiting(QueueIdentifier_t queue)
{
	if (queue == NULL)
		return 0;

	return uxQueueMessagesWaiting(queue);
}
