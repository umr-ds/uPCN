#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "upcn/upcn.h"
#include "upcn/bundle.h"
#include "upcn/routerTask.h"
#include "upcn/routingTable.h"
#include "upcn/router.h"
#include "upcn/routerOptimizer.h"
#include "upcn/bundleProcessor.h"
#include "upcn/bundleStorageManager.h"
#include "upcn/contactManager.h"
#include "upcn/bundleFragmenter.h"
#include "upcn/beacon.h"
#include "upcn/eidManager.h"

static bool process_signal(
	struct router_signal signal,
	QueueIdentifier_t bp_signaling_queue,
	QueueIdentifier_t router_signaling_queue,
	Semaphore_t cm_semaphore,
	QueueIdentifier_t cm_queue,
	QueueIdentifier_t beacon_queue,
	Semaphore_t ro_sem);

static bool process_router_command(struct router_command *router_cmd,
	QueueIdentifier_t router_queue, QueueIdentifier_t bp_signaling_queue);

#define BUNDLE_RESULT_NO_ROUTE 0
#define BUNDLE_RESULT_NO_TIMELY_CONTACTS -1
#define BUNDLE_RESULT_NO_MEMORY -2
#define BUNDLE_RESULT_INVALID -3

static int8_t process_bundle(struct bundle *bundle);

/* See below for that */
#ifdef VERBOSE
static void print_bundle_debug_info(bundleid_t id, uint8_t fragments);
#else
#define print_bundle_debug_info(a, b)
#endif

void router_task(void *rt_parameters)
{
	struct router_task_parameters *parameters;
	struct contact_manager_params cm_param;
	Semaphore_t ro_sem;
	struct router_signal signal;

	ASSERT(rt_parameters != NULL);
	parameters = (struct router_task_parameters *)rt_parameters;

	/* Init routing tables */
	ASSERT(routing_table_init() == UPCN_OK);
	/* Start contact manager */
	cm_param = contact_manager_start(
		parameters->router_signaling_queue,
		routing_table_get_raw_contact_list_ptr());
	ASSERT(cm_param.control_queue != NULL);
	/* Start optimizer */
	ro_sem = router_start_optimizer_task(
		parameters->router_signaling_queue, cm_param.semaphore,
		routing_table_get_raw_contact_list_ptr());
	ASSERT(ro_sem != NULL);

	LOG("RouterTask: Started up successfully");
	for (;;) {
		if (hal_queue_receive(
			parameters->router_signaling_queue, &signal,
	    -1) == RETURN_SUCCESS
		) {
			process_signal(signal,
				parameters->bundle_processor_signaling_queue,
				parameters->router_signaling_queue,
				cm_param.semaphore, cm_param.control_queue,
				cm_param.beacon_queue, ro_sem);
		}
	}
}

static inline enum bundle_status_report_reason get_reason(int8_t bh_result)
{
	switch (bh_result) {
	case BUNDLE_RESULT_NO_ROUTE:
		return BUNDLE_SR_REASON_NO_KNOWN_ROUTE;
	case BUNDLE_RESULT_NO_MEMORY:
		return BUNDLE_SR_REASON_DEPLETED_STORAGE;
	case BUNDLE_RESULT_NO_TIMELY_CONTACTS:
	default:
		return BUNDLE_SR_REASON_NO_TIMELY_CONTACT;
	}
}

static bool process_signal(
	struct router_signal signal,
	QueueIdentifier_t bp_signaling_queue,
	QueueIdentifier_t router_signaling_queue,
	Semaphore_t cm_semaphore,
	QueueIdentifier_t cm_queue,
	QueueIdentifier_t beacon_queue,
	Semaphore_t ro_sem)
{
	bool success = true;
	bundleid_t b_id;
	struct bundle *b;
	struct routed_bundle *rb;
	struct contact *contact;
	struct router_command *command;
	struct beacon *beacon;
	struct ground_station *station;
	enum contact_manager_signal cm_signal;
	int8_t fragments;

	switch (signal.type) {
	case ROUTER_SIGNAL_PROCESS_COMMAND:
		command = (struct router_command *)signal.data;
		hal_semaphore_take_blocking(cm_semaphore);
		success = process_router_command(
			command, router_signaling_queue, bp_signaling_queue);
		hal_semaphore_release(cm_semaphore);
		/* Wake up CM */
		if (success) {
			cm_signal = CM_SIGNAL_CONTACTS_UPDATED;
			if (hal_queue_try_push_to_back(cm_queue, &cm_signal, 0)
				== RETURN_FAILURE
			) {
				cm_signal |= CM_SIGNAL_BUNDLE_SCHEDULED;
				hal_queue_override_to_back(cm_queue,
							   &cm_signal);
			}
		}
		if (success) {
			LOGA("RouterTask: Command processed successfully",
				command->type, LOG_NO_ITEM);
		} else {
			LOGA("RouterTask: Processing command failed",
				command->type, LOG_NO_ITEM);
		}
		free(command);
		hal_semaphore_release(ro_sem); /* Allow optimizer to run */
		break;
	case ROUTER_SIGNAL_ROUTE_BUNDLE:
		b_id = (bundleid_t)(uintptr_t)signal.data;
		b = bundle_storage_get(b_id);
		hal_semaphore_take_blocking(cm_semaphore);
		/*
		 * TODO: Check bundle expiration time
		 * => no timely contact signal
		 */
		if (b != NULL)
			fragments = process_bundle(b);
		else
			fragments = BUNDLE_RESULT_INVALID;
		b = NULL; /* b may be invalid or free'd now */
		hal_semaphore_release(cm_semaphore);
		print_bundle_debug_info(
			b_id, (fragments < 1) ? 0 : (uint8_t)fragments);
		if (fragments < 1) {
			enum bundle_status_report_reason reason
				= get_reason(fragments);
			bundle_processor_inform(
				bp_signaling_queue, b_id,
				BP_SIGNAL_FORWARDING_CONTRAINDICATED,
				reason);
			success = false;
			// TODO: output type of error
			LOGI("RouterTask: Processing bundle failed", b_id);
		} else {
			bundle_processor_inform(
				bp_signaling_queue, b_id,
				BP_SIGNAL_BUNDLE_ROUTED,
				BUNDLE_SR_REASON_NO_INFO);
			cm_signal = CM_SIGNAL_BUNDLE_SCHEDULED;
			if (hal_queue_try_push_to_back(cm_queue, &cm_signal, 0)
				== RETURN_FAILURE
			) {
				cm_signal |= CM_SIGNAL_CONTACTS_UPDATED;
				hal_queue_override_to_back(cm_queue,
							   &cm_signal);
			}
			LOGI("RouterTask: Processed bundle successfully",
				b_id);
		}
		hal_semaphore_release(ro_sem); /* Allow optimizer to run */
		break;
	case ROUTER_SIGNAL_PROCESS_BEACON:
		beacon = (struct beacon *)signal.data;
		hal_queue_try_push_to_back(beacon_queue, &beacon,
					   BEACON_PROC_DELAY);
		/* Sending a CM_SIGNAL_NONE to the contact manager will lead */
		/* to a transmisison of a beacon */
		cm_signal = CM_SIGNAL_NONE;
		hal_queue_try_push_to_back(cm_queue, &cm_signal, 0);
		break;
	case ROUTER_SIGNAL_CONTACT_OVER:
		contact = (struct contact *)signal.data;
		hal_semaphore_take_blocking(cm_semaphore);
		routing_table_contact_passed(
			contact, bp_signaling_queue);
		LOGI("RouterTask: Contact removed from table", contact);
		hal_semaphore_release(cm_semaphore);
		break;
	case ROUTER_SIGNAL_TRANSMISSION_SUCCESS:
	case ROUTER_SIGNAL_TRANSMISSION_FAILURE:
		rb = (struct routed_bundle *)signal.data;
		if (rb->serialized == rb->contact_count) {
			b_id = rb->id;
			bundle_processor_inform(
				bp_signaling_queue, b_id,
				(rb->serialized == rb->transmitted)
					? BP_SIGNAL_TRANSMISSION_SUCCESS
					: BP_SIGNAL_TRANSMISSION_FAILURE,
				BUNDLE_SR_REASON_NO_INFO
			);
			eidmanager_free_ref(rb->destination);
			free(rb->contacts);
			free(rb);
			LOGI("RouterTask: Sent bundle successfully",
				b_id);
		}
		break;
	case ROUTER_SIGNAL_OPTIMIZATION_DROP:
		/* Should probably never occur... */
		/* If optimization failed to schedule a preempted bundle */
		rb = (struct routed_bundle *)signal.data;
		b_id = rb->id;
		bundle_processor_inform(
			bp_signaling_queue, b_id,
			BP_SIGNAL_TRANSMISSION_FAILURE,
			BUNDLE_SR_REASON_NO_INFO
		);
		eidmanager_free_ref(rb->destination);
		free(rb->contacts);
		free(rb);
		LOGI("RouterTask: Optimizer preemption routing failed",
			b_id);
		break;
	case ROUTER_SIGNAL_WITHDRAW_STATION:
		station = (struct ground_station *)signal.data;
		hal_semaphore_take_blocking(cm_semaphore);
		routing_table_delete_gs_by_eid(station->eid,
			router_signaling_queue);
		hal_semaphore_release(cm_semaphore);
		LOGI("RouterTask: GS withdrawn", station);
		break;
	default:
		LOGI("RouterTask: Invalid signal received",
			signal.type);
		success = false;
		break;
	}
	return success;
}

static bool process_router_command(struct router_command *router_cmd,
	QueueIdentifier_t router_queue, QueueIdentifier_t bp_signaling_queue)
{
	/* This sorts and removes duplicates */
	if (!ground_station_prepare_and_verify(router_cmd->data)) {
		free_ground_station(router_cmd->data);
		return false;
	}
	switch (router_cmd->type) {
	case ROUTER_COMMAND_ADD:
		routing_table_add_gs(
			router_cmd->data, router_queue, bp_signaling_queue);
		break;
	case ROUTER_COMMAND_UPDATE:
		routing_table_replace_gs(
			router_cmd->data, router_queue, bp_signaling_queue);
		break;
	case ROUTER_COMMAND_DELETE:
		return (routing_table_delete_gs(
				router_cmd->data, bp_signaling_queue)
					? true : false);
	case ROUTER_COMMAND_QUERY:
		/* Return information to defined GS on defined contact */
#ifdef VERBOSE
		hal_platform_print_system_info();
		routing_table_print_debug_info();
#endif
		free_ground_station(router_cmd->data);
		break;
	default:
		free_ground_station(router_cmd->data);
		return false;
	}
	return true;
}

static int8_t apply_fragmentation(
	struct bundle *bundle, struct router_result route);

static int8_t process_bundle(struct bundle *bundle)
{
	struct router_result route;

	ASSERT(bundle != NULL);
	route = router_get_first_route(bundle);
	/* TODO: Add to list if no route but own OR priority > X */
	if (route.fragments == 0)
		return BUNDLE_RESULT_NO_ROUTE;
	else if (route.fragments == 1)
		return (router_add_bundle_to_route(
			&route.fragment_results[0], bundle) ? 1
				: BUNDLE_RESULT_NO_MEMORY);
	else
		return apply_fragmentation(bundle, route);

}

static int8_t apply_fragmentation(
	struct bundle *bundle, struct router_result route)
{
	struct bundle *frags[ROUTER_MAX_FRAGMENTS];
	uint32_t size;
	int8_t f, g;
	uint8_t fragments = route.fragments;
	/*bundle_storage_delete(old_id);*/
	/*bundle->id = BUNDLE_INVALID_ID;*/
	/* Create fragments */
	frags[0] = bundlefragmenter_initialize_first_fragment(bundle);
	if (frags[0] == NULL)
		return BUNDLE_RESULT_NO_MEMORY;

	for (f = 0; f < fragments - 1; f++) {
		/* Determine minimal fragmented bundle size */
		if (f == 0)
			size = bundle_get_first_fragment_min_size(bundle);
		else if (f == fragments - 1)
			size = bundle_get_last_fragment_min_size(bundle);
		else
			size = bundle_get_mid_fragment_min_size(bundle);

		frags[f + 1] = bundlefragmenter_fragment_bundle(frags[f],
			size + route.fragment_results[f].payload_size);

		if (frags[f + 1] == NULL) {
			for (g = 0; g <= f; g++)
				bundle_free(frags[g]);
			return BUNDLE_RESULT_NO_MEMORY;
		}
	}

	/* Add to route */
	for (f = 0; f < fragments; f++) {
		bundle_storage_add(frags[f]);
		if (!router_add_bundle_to_route(
			&route.fragment_results[f], frags[f])
		) {
			for (g = 0; g < f; g++)
				router_remove_bundle_from_route(
					&route.fragment_results[g],
					frags[g]->id, 1);
			for (g = 0; g < fragments; g++) {
				/* FIXME: Routed bundles not unrouted */
				if (g <= f)
					bundle_storage_delete(frags[g]->id);
				bundle_free(frags[g]);
			}
			return BUNDLE_RESULT_NO_MEMORY;
		}
	}
	/* TODO: Inform BProc of new IDs */
	/* Remove bundle */
	bundle_storage_delete(bundle->id);
	/*bundle->id = BUNDLE_INVALID_ID;*/
	bundle_free(bundle);
	return fragments;
}

/* DEBUG */

#ifdef VERBOSE

static void print_bundle_debug_info(bundleid_t id, uint8_t fragments)
{
	static char wbuf[10];

	hal_io_write_string("\nRouterTask: Bundle #");
	hal_platform_sprintu32(wbuf, (uint32_t)id);
	hal_io_write_string(wbuf);
	if (fragments) {
		hal_io_write_string(" received [ OK ] [ ");
		hal_platform_sprintu32(wbuf, (uint32_t)fragments);
		hal_io_write_string(wbuf);
		hal_io_write_string(" fragment(s) ]\n");
	} else {
		hal_io_write_string(" received [ ERR ]\n");
	}
	hal_io_write_string("\n");
}

#endif
