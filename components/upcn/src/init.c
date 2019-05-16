#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "hal_task.h"
#include "hal_queue.h"
#include "hal_platform.h"
#include "hal_debug.h"

#include "upcn/upcn.h"
#include "upcn/routerTask.h"
#include "upcn/bundleProcessor.h"
#include "upcn/beacon.h"
#include "upcn/beaconProcessor.h"
#include "upcn/eidManager.h"
#include <cla.h>

static QueueIdentifier_t router_signaling_queue;
static QueueIdentifier_t bundle_signaling_queue;

static void init_subsystems(void)
{
	uint8_t i, randlen = (BEACON_PRIVATE_KEY_LENGTH / 4)
			+ ((BEACON_PRIVATE_KEY_LENGTH % 4) ? 1 : 0);
	uint32_t *randbuf = malloc(randlen * 4);

	ASSERT(randbuf != NULL);
	eidmanager_init();
	for (i = 0; i < randlen; i++)
		randbuf[i] = hal_random_get();
	ASSERT(beacon_processor_init((uint8_t *)randbuf) == UPCN_OK);
	free(randbuf);
}

void init(uint16_t io_socket_port)
{
	hal_debug_init();
#ifndef UPCN_POSIX_TEST_BUILD
	hal_platform_init(io_socket_port);
#endif
	init_subsystems();
	cla_init(io_socket_port);
	hal_debug_printf("\n\n#############################################\n");
	hal_debug_printf("This is uPCN, compiled %s %s\n", __DATE__, __TIME__);
	hal_debug_printf("All subsystems initialized, starting system...\n");
	hal_debug_printf("#############################################\n\n");
}

#ifdef TEST_SGP4_PERFORMANCE
void sgp4testtask(void *task_params);
#endif /* TEST_SGP4_PERFORMANCE */

void start_tasks(void)
{
	/* Initialize queues to communicate with the subsystems */
	router_signaling_queue
			= hal_queue_create(ROUTER_QUEUE_LENGTH,
					   sizeof(struct router_signal));
	ASSERT(router_signaling_queue != NULL);
	bundle_signaling_queue
			= hal_queue_create(BUNDLE_QUEUE_LENGTH,
				sizeof(struct bundle_processor_signal));
	ASSERT(bundle_signaling_queue != NULL);

	struct router_task_parameters *router_task_params =
			malloc(sizeof(struct router_task_parameters));
	ASSERT(router_task_params != NULL);
	router_task_params->router_signaling_queue = router_signaling_queue;
	router_task_params->bundle_processor_signaling_queue
			= bundle_signaling_queue;

	struct bundle_processor_task_parameters *bundle_processor_task_params
		= malloc(sizeof(struct bundle_processor_task_parameters));

	ASSERT(bundle_processor_task_params != NULL);
	bundle_processor_task_params->router_signaling_queue
			= router_signaling_queue;
	bundle_processor_task_params->signaling_queue = bundle_signaling_queue;

	hal_task_create(router_task,
			"router_t",
			ROUTER_TASK_PRIORITY,
			router_task_params,
			DEFAULT_TASK_STACK_SIZE,
			(void *)ROUTER_TASK_TAG);

	hal_task_create(bundle_processor_task,
			"bundl_proc_t",
			BUNDLE_PROCESSOR_TASK_PRIORITY,
			bundle_processor_task_params,
			DEFAULT_TASK_STACK_SIZE,
			(void *)BUNDLE_PROCESSOR_TASK_TAG);

#ifdef TEST_SGP4_PERFORMANCE
	hal_task_create(sgp4testtask,
			"sgp4test",
			CONTACT_MANAGER_TASK_PRIORITY,
			NULL,
			4096,
			(void *)13)
		#endif /* TEST_SGP4_PERFORMANCE */
}

int start_os(void)
{
	/* Initialize the communication subsystem (CLA) */
	LOGF("Active CLA: %s", cla_get_name());
	cla_global_setup();

	hal_task_start_scheduler();
	/* Should never get here! */
	hal_platform_hard_restart_upcn();
	return 0;
}

QueueIdentifier_t get_global_router_signaling_queue(void)
{
	return router_signaling_queue;
}

QueueIdentifier_t get_global_bundle_signaling_queue(void)
{
	return bundle_signaling_queue;
}

