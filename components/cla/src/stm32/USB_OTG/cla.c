#include <cla.h>
#include <cla_contact_rx_task.h>
#include <cla_contact_tx_task.h>
#include <cla_io.h>
#include "upcn/init.h"
#include <unistd.h>

static struct cla_config conf;
static cla_handler rx_handler;

int cla_global_setup(void)
{
	cla_handler *worker;
	struct cla_contact_tx_task_creation_result res;

	cla_launch_contact_rx_task();

	for (int i = 0; i < CLA_CHANNELS; i++) {
		worker = &worker_tasks[i];
		worker->config = &conf;
		worker->sem_wait_for_shutdown = hal_semaphore_init_mutex();
		res = cla_launch_contact_tx_task(
				get_global_router_signaling_queue(),
				worker);
		ASSERT(res.result == UPCN_OK);
		worker->tx_task = res.task_handle;
		worker->tx_queue = res.queue_handle;
	}

	return EXIT_SUCCESS;
}

void cla_init(uint16_t port)
{
	(void)port;
	cla_init_config_struct(&conf);
	rx_handler.config = &conf;
	cla_io_init();
}

const char *cla_get_name(void)
{
	return "USB_OTG";
}

inline bool cla_is_connection_oriented(void)
{
	return false;
}

int cla_init_config_struct(struct cla_config *config)
{
	config->cla_semaphore = hal_semaphore_init_mutex();
	config->cla_com_rx_semaphore = hal_semaphore_init_mutex();
	config->cla_com_tx_semaphore = hal_semaphore_init_mutex();

	hal_semaphore_release(config->cla_semaphore);
	hal_semaphore_release(config->cla_com_tx_semaphore);
	hal_semaphore_release(config->cla_com_rx_semaphore);
	/* return success */
	return 0;
}

cla_handler *cla_create_contact_handler(struct cla_config *config, int val)
{
	return &worker_tasks[val];
}

int cla_remove_scheduled_contact(cla_handler *handler)
{
	return 0;
}

struct cla_config *cla_allocate_cla_config(void)
{
	return malloc(sizeof(struct cla_config));
}

struct cla_config *cla_get_debug_cla_handler(void)
{
	return rx_handler.config;
}

int cla_exit(void)
{
	return EXIT_SUCCESS;
}

