#include <stddef.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "upcn/upcn.h"
#include "upcn/contactManager.h"
#include "upcn/routerTask.h"
#include "upcn/groundStation.h"
#include <cla_contact_tx_task.h>
#include "upcn/beaconProcessor.h"
#include <cla_management.h>
#include <cla.h>
#include "upcn/beaconGenerator.h"
#include "cla_defines.h"

struct contact_manager_task_parameters {
	Semaphore_t semaphore;
	QueueIdentifier_t control_queue;
	QueueIdentifier_t beacon_queue;
	QueueIdentifier_t router_signaling_queue;
	struct contact_list **contact_list_ptr;
};

struct contact_info {
	struct contact *contact;
	uint16_t expected_beacons;
	uint16_t received_beacons;
	cla_handler *handler;
};

struct contact_task_info {
	Task_t tx_task;
	Task_t rx_task;
	QueueIdentifier_t tx_queue;
	struct cla_config *conf;
};

static struct contact_info current_contacts[CLA_CHANNELS];
static int8_t current_contact_count;
static uint64_t next_contact_time = UINT64_MAX;

static bool contact_active(const struct contact *contact)
{
	int8_t i;

	for (i = 0; i < current_contact_count; i++) {
		if (current_contacts[i].contact == contact)
			return true;
	}
	return false;
}

static void reset_bundles(int8_t index)
{
	struct contact *c = current_contacts[index].contact;
	struct cla_contact_tx_task_command cmd;
	struct routed_bundle_list **tmp;
	uint8_t fin = 0;

	// The handler is NULL for opportunistic contacts
	if (!current_contacts[index].handler)
		return;
	while (hal_queue_receive(current_contacts[index].handler->tx_queue,
				 &cmd, 0) == RETURN_SUCCESS) {
		if (cmd.type != GS_COMMAND_BUNDLES) {
			if (cmd.type == GS_COMMAND_FINALIZE)
				fin = 1;
			continue;
		}
		if (c->contact_bundles == NULL) {
			c->contact_bundles = cmd.bundles;
		} else {
			tmp = &cmd.bundles;
			while (*tmp != NULL)
				tmp = &(*tmp)->next;
			*tmp = c->contact_bundles;
			c->contact_bundles = cmd.bundles;
		}

	}
	if (fin) /* NOTE: Should never occur */
		cla_contact_tx_task_delete(
			current_contacts[index].handler->tx_queue);
}

static int8_t remove_expired_contacts(
	struct contact_list *contact_list, const uint64_t current_timestamp,
	struct contact_info list[])
{
	/* Check for ending contacts */
	/* We do this first as it frees space in the list */
	int8_t i, c, removed = 0;
	cla_handler *handle;

	for (i = current_contact_count - 1; i >= 0; i--) {
		if (current_contacts[i].contact->to <= current_timestamp) {
			ASSERT(i <= CLA_CHANNELS);
			/* Unset "active" constraint */
			current_contacts[i].contact->active = 0;
			/* Move bundles to contact again for rescheduling */
			reset_bundles(i);
			list[removed++] = current_contacts[i];
			/* If it's not the last element, we have to move mem */
			if (i != current_contact_count - 1) {
				for (c = i; c < current_contact_count; c++) {
					current_contacts[c] =
						current_contacts[c + 1];
					handle = current_contacts[c+1].handler;
					current_contacts[c+1].handler =
						current_contacts[c].handler;
					current_contacts[c].handler =
						handle;
				}
			}
			current_contact_count--;
		}
	}
	return removed;
}

static uint8_t check_upcoming(
	struct contact *c, struct contact_info list[], const uint8_t index)
{
	uint16_t exp;

	if (!contact_active(c) && current_contact_count < CLA_CHANNELS) {
		/* Set "active" constraint */
		c->active = 1;
		/* Add contact */
		current_contacts[current_contact_count].contact = c;
		if (HAS_FLAG(c->ground_station->flags, GS_FLAG_DISCOVERED)) {
			exp = (c->to - c->from) / c->ground_station
				->rrnd_info->beacon_period;
		} else {
			exp = 0;
		}
		current_contacts[current_contact_count].expected_beacons = exp;
		current_contacts[current_contact_count].received_beacons = 0;

		c->ground_station->cla_config->contact = c;
		current_contacts[current_contact_count].handler =
			cla_create_contact_handler(
				c->ground_station->cla_config,
				current_contact_count
			);

		list[index] = current_contacts[current_contact_count];
		current_contact_count++;
		return 1;
	}
	return 0;
}

static int8_t process_upcoming_list(
	struct contact_list *contact_list, const uint64_t current_timestamp,
	struct contact_info list[])
{
	int8_t added = 0;
	struct contact_list *cur_entry;

	next_contact_time = UINT64_MAX;
	cur_entry = contact_list;
	while (cur_entry != NULL) {
		if (cur_entry->data->from <= current_timestamp) {
			if (cur_entry->data->to > current_timestamp) {
				added += check_upcoming(
					cur_entry->data, list, added);
				if (cur_entry->data->to < next_contact_time)
					next_contact_time = cur_entry->data->to;
			}
		} else {
			if (cur_entry->data->from < next_contact_time)
				next_contact_time = cur_entry->data->from;
			/* As our contact_list is sorted ascending by */
			/* from-time we can stop checking here */
			break;
		}
		cur_entry = cur_entry->next;
	}
	return added;
}

static void hand_over_contact_bundles(int8_t index, struct contact_info c)
{
	struct cla_contact_tx_task_command command = {
		.type = GS_COMMAND_BUNDLES,
		.bundles = NULL,
		.contact = c.contact
	};

	ASSERT(c.contact != NULL);
	if (c.contact->contact_bundles == NULL)
		return;
	LOGI("Queuing bundles for contact.", c.contact);
	command.bundles = c.contact->contact_bundles;
	c.contact->contact_bundles = NULL;
	ASSERT(c.handler != NULL);
	hal_queue_push_to_back(c.handler->tx_queue, &command);
}

static void process_current_contacts(void)
{
	int8_t i;

	for (i = 0; i < current_contact_count; i++)
		hand_over_contact_bundles(i, current_contacts[i]);
}

static void send_contact_state_info(struct contact *contact, uint8_t state)
{
	char *eid = contact->ground_station->eid;
	size_t eid_len = strlen(eid);

	// Send information if a contact started/ended: (0x0|0x1)<EID>
	hal_io_lock_com_semaphore();
	hal_io_begin_packet(eid_len + 2, COMM_TYPE_CONTACT_STATE);
	hal_io_send_packet_data(&state, 1);
	hal_io_send_packet_data(eid, eid_len + 1);
	hal_io_end_packet();
	hal_io_unlock_com_semaphore();
}

static uint8_t check_for_contacts(struct contact_list *contact_list,
	struct contact_info removed_contacts[])
{
	int8_t i;
	static struct contact_info added_contacts[CLA_CHANNELS];
	uint64_t current_timestamp = hal_time_get_timestamp_s();
	int8_t removed_count = remove_expired_contacts(
		contact_list, current_timestamp, removed_contacts);
	int8_t added_count = process_upcoming_list(
		contact_list, current_timestamp, added_contacts);

	for (i = 0; i < added_count; i++) {
		send_contact_state_info(added_contacts[i].contact, 1);
		LOGI("ContactManager: Scheduled contact added.",
			added_contacts[i].contact);
		hand_over_contact_bundles(i, added_contacts[i]);
	}
	for (i = 0; i < removed_count; i++) {
		send_contact_state_info(removed_contacts[i].contact, 0);
		LOGI("ContactManager: Contact removed.",
			removed_contacts[i].contact);
		cla_remove_scheduled_contact(removed_contacts[i].handler);
	}
	return removed_count;
}

static void inform_router(
	enum router_signal_type type, void *data, QueueIdentifier_t queue)
{
	struct router_signal signal = {
		.type = type,
		.data = data
	};

	ASSERT(data != NULL);
	hal_queue_push_to_back(queue, &signal);
}

/* We assume that contact_list will not change. */
static void manage_contacts(
	struct contact_list **contact_list, enum contact_manager_signal signal,
	Semaphore_t semphr, QueueIdentifier_t queue)
{
	static struct contact_info rem[CLA_CHANNELS];
	int8_t removed, i;

	ASSERT(semphr != NULL);
	ASSERT(queue != NULL);
	hal_semaphore_take_blocking(semphr);
	if (HAS_FLAG(signal, CM_SIGNAL_BUNDLE_SCHEDULED))
		process_current_contacts();
	/* If there only was a new bundle, skip further checks */
	if (signal == CM_SIGNAL_BUNDLE_SCHEDULED) {
		hal_semaphore_release(semphr);
		return;
	}
	removed = check_for_contacts(*contact_list, rem);
	hal_semaphore_release(semphr);
	for (i = 0; i < removed; i++) {
		/* The contact has to be deleted first... */
		inform_router(ROUTER_SIGNAL_CONTACT_OVER,
			rem[i].contact, queue);
	}
}

static void contact_manager_task(void *cm_parameters)
{
	struct contact_manager_task_parameters *parameters
		= (struct contact_manager_task_parameters *)cm_parameters;
	int8_t led_state = 0;
	enum contact_manager_signal signal = CM_SIGNAL_NONE;
	struct beacon *beacon;
	uint64_t cur_time, next_time, delay;

	ASSERT(parameters != NULL);
	LOG("ContactManager: Started up successfully.");
	for (;;) {
		hal_platform_led_set((led_state = 1 - led_state) + 3);
		while (hal_queue_receive(parameters->beacon_queue,
					 &beacon, 0) == RETURN_SUCCESS) {
			/* TODO: Do sth. with the result! */
			beacon_processor_process(
				beacon,
				parameters->semaphore,
				parameters->router_signaling_queue
			);
			beacon = NULL;
		}
		/* TODO: Only begin contact with indic. of bidirectionalty */
		if (signal != CM_SIGNAL_NONE) { /* TODO: end by query to BP */
			manage_contacts(parameters->contact_list_ptr, signal,
				parameters->semaphore,
				parameters->router_signaling_queue);
		}
		signal = CM_SIGNAL_UNKNOWN;
		cur_time = hal_time_get_timestamp_ms();
#ifdef ND_DISABLE_BEACONS
		next_time = UINT64_MAX;
#else /* ND_DISABLE_BEACONS */
		// NOTE: The beacon generator uses the GS list!
		hal_semaphore_take_blocking(parameters->semaphore);
		next_time = beacon_generator_check_send(cur_time);
		hal_semaphore_release(parameters->semaphore);
#endif /* ND_DISABLE_BEACONS */
		next_time = MIN(next_time, next_contact_time * 1000);
		if (next_time > (cur_time + CONTACT_CHECKING_MAX_PERIOD))
			delay = CONTACT_CHECKING_MAX_PERIOD;
		else if (next_time <= cur_time)
			continue;
		else
			delay = next_time - cur_time;
		/*LOGA("ContactManager: Suspended on queue.",*/
		/*	(uint8_t)(delay / 1000), LOG_NO_ITEM);*/
		hal_queue_receive(parameters->control_queue, &signal,
			delay+1);
	}
}

struct contact_manager_params contact_manager_start(
	QueueIdentifier_t router_signaling_queue,
	struct contact_list **clistptr)
{
	struct contact_manager_params ret = {
		.semaphore = NULL,
		.control_queue = NULL,
		.beacon_queue = NULL
	};
	QueueIdentifier_t queue;
	QueueIdentifier_t beacon_queue;
	struct contact_manager_task_parameters *cmt_params;
	Semaphore_t semaphore = hal_semaphore_init_mutex();

	if (semaphore == NULL)
		return ret;
	hal_semaphore_release(semaphore);
	queue = hal_queue_create(1, sizeof(enum contact_manager_signal));
	beacon_queue = hal_queue_create(5, sizeof(void *));
	if (queue == NULL) {
		hal_semaphore_delete(semaphore);
		return ret;
	}
	cmt_params = malloc(sizeof(struct contact_manager_task_parameters));
	if (cmt_params == NULL) {
		hal_semaphore_delete(semaphore);
		hal_queue_delete(queue);
		return ret;
	}
	cmt_params->semaphore = semaphore;
	cmt_params->control_queue = queue;
	cmt_params->beacon_queue = beacon_queue;
	cmt_params->router_signaling_queue = router_signaling_queue;
	cmt_params->contact_list_ptr = clistptr;
	hal_task_create(contact_manager_task,
			"cont_man_t",
			CONTACT_MANAGER_TASK_PRIORITY,
			cmt_params,
			CONTACT_MANAGER_TASK_STACK_SIZE,
			(void *)CONTACT_MANAGER_TASK_TAG);
	ret.semaphore = semaphore;
	ret.control_queue = queue;
	ret.beacon_queue = beacon_queue;
	return ret;
}

uint64_t contact_manager_get_next_contact_time(void)
{
	return next_contact_time;
}

uint8_t contact_manager_in_contact(void)
{
	return current_contact_count != 0;
}

void contact_manager_reset_time(void)
{
	next_contact_time = hal_time_get_timestamp_s();
#ifndef ND_DISABLE_BEACONS
	beacon_generator_reset_next_time();
#endif
}
