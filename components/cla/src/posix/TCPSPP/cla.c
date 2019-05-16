#include <cla.h>
#include <cla_contact_rx_task.h>
#include <cla_contact_tx_task.h>
#include <cla_io.h>
#include <unistd.h>

#include "upcn/init.h"

#include "spp/spp_timecodes.h"

static cla_conf conf;
static cla_handler rx_handler;

int cla_global_setup(void)
{
	cla_handler *worker;
	struct cla_contact_tx_task_creation_result res;

	cla_init_config_struct(&conf);
	rx_handler.config = &conf;

	cla_launch_contact_rx_task(&rx_handler);

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

	cla_io_init();
	cla_io_listen(&conf, CLA_TCPSPP_PORT);

	return 0;
}

const char *cla_get_name(void)
{
	return "TCPSPP";
}

inline bool cla_is_connection_oriented(void)
{
	return false;
}

int cla_init_config_struct(struct cla_config *config)
{
	static const uint8_t preamble[] = {
		CLA_TCPSPP_TIMESTAMP_FORMAT_PREAMBLE
	};
	const bool have_timecode = sizeof(preamble) > 0;

	config->cla_semaphore = hal_semaphore_init_mutex();
	config->cla_com_rx_semaphore = hal_semaphore_init_mutex();
	config->cla_com_tx_semaphore = hal_semaphore_init_mutex();
	config->connection_established = false;
	config->socket_identifier = -1;

	config->spp_ctx = spp_new_context();
	if (!spp_configure_ancillary_data(config->spp_ctx, 0))
		LOG("tcpspp: failed to configure ancillary data size!");

	if (have_timecode) {
		config->spp_timecode.with_p_field =
				CLA_TCPSPP_TIMESTAMP_USE_P_FIELD;
		if (spp_tc_configure_from_preamble(
					&config->spp_timecode,
					&preamble[0], sizeof(preamble)) != 0) {
			/* preamble uses unknown format */
			LOG("tcpspp: failed to configure timecode!");
		} else if (!spp_configure_timecode(config->spp_ctx,
						   &config->spp_timecode)) {
			/* this can’t actually fail atm, but may in the future
			 */
			LOG("tcpspp: failed to apply timecode!");
		}
	} else {
		LOG("tcpspp: not using timecode.");
	}

	hal_semaphore_release(config->cla_semaphore);

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
	cla_io_exit();
	return close(rx_handler.config->socket_identifier);
}

void cla_init(uint16_t port)
{
	/* nothing to do here yet! */
}


