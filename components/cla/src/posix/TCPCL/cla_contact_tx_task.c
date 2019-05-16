#include <stdlib.h>

#include "upcn/upcn.h"
#include "upcn/bundle.h"
#include "upcn/groundStation.h"
#include "upcn/routerTask.h"
#include "upcn/bundleStorageManager.h"
#include <cla_contact_tx_task.h>

#include <cla_io.h>
#include <cla_management.h>

#ifdef THROUGHPUT_TEST
uint64_t timestamp_mp3[47];
uint64_t timestamp_mp4[47];
#endif

struct contact_tx_task_parameters {
	QueueIdentifier_t queue_handle;
	QueueIdentifier_t router_signaling_queue;
	cla_handler *pair;
};

static inline void report_bundle(QueueIdentifier_t signaling_queue,
	struct routed_bundle *bundle, enum router_signal_type type)
{
	struct router_signal signal = {
		.type = type,
		.data = bundle
	};

	hal_queue_push_to_back(signaling_queue, &signal);
}

static void cla_contact_tx_task(void *p)
{
	QueueIdentifier_t cmd_queue =
		((struct contact_tx_task_parameters *)p)->queue_handle;
	QueueIdentifier_t rt_queue =
		((struct contact_tx_task_parameters *)p)
			->router_signaling_queue;
	struct cla_task_pair *pair =
		((struct contact_tx_task_parameters *)p)->pair;
	struct cla_contact_tx_task_command cmd;
	struct bundle *b;
	struct routed_bundle_list *cur;
	enum upcn_result s;
	struct cla_config *cla_config;

	LOGI("contact_tx_task: Started successfully",
		((struct contact_tx_task_parameters *)p)->queue_handle);
	for (;;) {
		if (hal_queue_receive(cmd_queue, &cmd, -1) == RETURN_FAILURE)
			continue;
		else if (cmd.type == GS_COMMAND_FINALIZE)
			break;
		cla_config = cmd.contact->ground_station->cla_config;
		cla_ensure_connection(cla_config);
		/* GS_COMMAND_BUNDLES received */
		while (cmd.bundles != NULL) {
			cur = cmd.bundles;
			cmd.bundles = cmd.bundles->next;
			cur->data->serialized++;
			b = bundle_storage_get(cur->data->id);
			/* LOGI("GroundStationTask: Sending bundle", b->id); */
#ifdef VERBOSE
#ifdef BUNDLE_PRINT
			bundle_print_stack_conserving(b);
#endif /* BUNDLE_PRINT */
#endif /* VERBOSE */
			if (b != NULL) {
#ifdef THROUGHPUT_TEST
				timestamp_mp3[b->sequence_number] =
					hal_time_get_timestamp_us();
#endif
				cla_begin_packet(cla_config,
					bundle_get_serialized_size(b),
					COMM_TYPE_BUNDLE);
				s = bundle_serialize(b,
						     &cla_send_packet_data,
						     (void *)cla_config);
#ifdef THROUGHPUT_TEST
				timestamp_mp4[b->sequence_number] =
					hal_time_get_timestamp_us();
#endif
				cla_end_packet(cla_config);
				LOGI("Sending bundle with SN",
				     b->sequence_number);
			} else {
				LOG("Sending bundle failed!");
				s = UPCN_FAIL;
			}

			if (s == UPCN_OK) {
				cur->data->transmitted++;
				report_bundle(rt_queue, cur->data,
					ROUTER_SIGNAL_TRANSMISSION_SUCCESS);
			} else {
				report_bundle(rt_queue, cur->data,
					ROUTER_SIGNAL_TRANSMISSION_FAILURE);
			}
			/* Free only the RB list, the RB is reported */
			free(cur);
		}
	}
	/* Free vars and delete task */
	free(p);
	hal_queue_delete(cmd_queue);
	LOG("Waiting for shutdown!");
	hal_semaphore_take_blocking(pair->sem_wait_for_shutdown);
}

struct cla_contact_tx_task_creation_result cla_launch_contact_tx_task(
	QueueIdentifier_t router_signaling_queue,
	cla_handler *task)
{
	static uint8_t ctr = 1;
	static char tname_buf[6];
	struct contact_tx_task_parameters *param;
	struct cla_contact_tx_task_creation_result res = {
		.task_handle = NULL,
		.queue_handle = NULL,
		.result = UPCN_FAIL
	};

	res.queue_handle = hal_queue_create(CONTACT_TX_TASK_QUEUE_LENGTH,
			sizeof(struct cla_contact_tx_task_command));
	if (res.queue_handle == NULL)
		return res;
	param = malloc(sizeof(struct contact_tx_task_parameters));
	if (param == NULL) {
		hal_queue_delete(res.queue_handle);
		return res;
	}
	param->queue_handle = res.queue_handle;
	param->router_signaling_queue = router_signaling_queue;
	param->pair = task;
	tname_buf[0] = 'g';
	tname_buf[1] = 's';
	hal_platform_sprintu32(tname_buf + 2, ctr++);
	res.task_handle = hal_task_create(cla_contact_tx_task,
			    tname_buf,
			    CONTACT_TX_TASK_PRIORITY,
			    param,
			    CONTACT_TX_TASK_STACK_SIZE,
			    (void *)CONTACT_TX_TASK_TAG);
	if (res.task_handle != NULL) {
		res.result = UPCN_OK;
	} else {
		hal_queue_delete(res.queue_handle);
		free(param);
	}
	return res;
}

void cla_contact_tx_task_delete(QueueIdentifier_t queue)
{
	struct cla_contact_tx_task_command command = {
		.type = GS_COMMAND_FINALIZE,
		.bundles = NULL,
		.contact = NULL
	};

	ASSERT(queue != NULL);
	hal_queue_push_to_back(queue, &command);
}
