/*
 * hal_queue.c
 *
 * Description: contains the stm32-implementation of the hardware
 * abstraction layer interface for thread-related functionality
 *
 */

#include "platform/hal_queue.h"

#include "upcn/result.h"

#include <FreeRTOS.h>
#include <queue.h>

#include <stdlib.h>
#include <stdio.h>

static inline enum upcn_result pdBOOL2upcnresult(BaseType_t pdBOOL)
{
	if (pdBOOL == pdTRUE)
		return UPCN_OK;
	return UPCN_FAIL;
}


QueueIdentifier_t hal_queue_create(int queue_length, int item_size)
{
	return xQueueCreate(queue_length, item_size);
}


void hal_queue_push_to_back(QueueIdentifier_t queue, const void *item)
{
	while (pdPASS != xQueueSendToBack(queue, item, portMAX_DELAY))
		;
}


enum upcn_result hal_queue_receive(QueueIdentifier_t queue,
				   void *targetBuffer,
				   int timeout)
{
	/* try indefinitely */
	if (timeout == -1)
		return pdBOOL2upcnresult(
			xQueueReceive(queue, targetBuffer, portMAX_DELAY)
		);

	/* otherwise abort after timeout */
	return pdBOOL2upcnresult(
		xQueueReceive(queue, targetBuffer, timeout/portTICK_PERIOD_MS)
	);
}


void hal_queue_reset(QueueHandle_t queue)
{
	xQueueReset(queue);
}


enum upcn_result hal_queue_try_push_to_back(QueueIdentifier_t queue,
					    const void *item, int timeout)
{
	/* try indefinitely */
	if (timeout == -1)
		return pdBOOL2upcnresult(
			xQueueSendToBack(queue, item, portMAX_DELAY)
		);

	return pdBOOL2upcnresult(
		xQueueSendToBack(queue, item, timeout/portTICK_PERIOD_MS)
	);
}


void hal_queue_delete(QueueIdentifier_t queue)
{
	vQueueDelete(queue);
}


enum upcn_result hal_queue_override_to_back(QueueIdentifier_t queue,
					    const void *item)
{
	xQueueOverwrite(queue, item); // will always return pdPASS
	return UPCN_OK;
}

uint8_t hal_queue_nr_of_items_waiting(QueueIdentifier_t queue)
{
	if (queue == NULL)
		return 0;

	return uxQueueMessagesWaiting(queue);
}
