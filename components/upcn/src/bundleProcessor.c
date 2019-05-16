#include <stddef.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "upcn/upcn.h"
#include "upcn/routerTask.h"
#include "upcn/bundleProcessor.h"
#include "upcn/reportManager.h"
#include "upcn/custodyManager.h"
#include "upcn/bundleStorageManager.h"
#include <bundle6/bundle6.h>
#include <bundle7/hopcount.h>
#include "upcn/agent_manager.h"


#ifdef THROUGHPUT_TEST
uint64_t timestamp_mp2[47];
#endif

enum bundle_handling_result {
	BUNDLE_HRESULT_OK = 0,
	BUNDLE_HRESULT_DELETED,
	BUNDLE_HRESULT_BLOCK_DISCARDED,
};

static QueueIdentifier_t out_queue;

/* DECLARATIONS */

static inline void handle_signal(const struct bundle_processor_signal signal);

static void bundle_dispatch(struct bundle *bundle);
static bool bundle_endpoint_is_local(struct bundle *bundle);
static void bundle_forward(struct bundle *bundle);
static void bundle_forwarding_scheduled(struct bundle *bundle);
static void bundle_forwarding_success(struct bundle *bundle);
static void bundle_forwarding_contraindicated(
	struct bundle *bundle, enum bundle_status_report_reason reason);
static void bundle_forwarding_failed(
	struct bundle *bundle, enum bundle_status_report_reason reason);
static void bundle_expired(struct bundle *bundle);
static void bundle_receive(struct bundle *bundle);
static enum bundle_handling_result handle_unknown_block_flags(
	struct bundle *bundle, enum bundle_block_flags flags);
static void bundle_deliver_local(struct bundle *bundle);
static void bundle_custody_accept(struct bundle *bundle);
static void bundle_custody_success(struct bundle *bundle);
static void bundle_custody_failure(
	struct bundle *bundle, enum bundle_custody_signal_reason reason);
static void bundle_delete(
	struct bundle *bundle, enum bundle_status_report_reason reason);
static void bundle_discard(struct bundle *bundle);
static void bundle_handle_custody_signal(
	struct bundle_administrative_record *signal);
static void bundle_dangling(struct bundle *bundle);
static bool hop_count_validation(struct bundle *bundle);

static void send_status_report(
	struct bundle *bundle,
	const enum bundle_status_report_status_flags status,
	const enum bundle_status_report_reason reason);
static void send_custody_signal(struct bundle *bundle,
	const enum bundle_custody_signal_type,
	const enum bundle_custody_signal_reason reason);
static uint8_t send_bundle(bundleid_t bundle, uint16_t timeout);
static struct bundle_block *find_block_by_type(struct bundle_block_list *blocks,
	enum bundle_block_type type);

static inline void bundle_add_rc(struct bundle *bundle,
	const enum bundle_retention_constraints constraint)
{
	bundle->ret_constraints |= constraint;
}

static inline void bundle_rem_rc(struct bundle *bundle,
	const enum bundle_retention_constraints constraint, int discard)
{
	bundle->ret_constraints &= ~constraint;
	if (discard && bundle->ret_constraints == BUNDLE_RET_CONSTRAINT_NONE)
		bundle_discard(bundle);
}

/* COMMUNICATION */

void bundle_processor_inform(
	QueueIdentifier_t signaling_queue, bundleid_t bundle,
	enum bundle_processor_signal_type type,
	enum bundle_status_report_reason reason)
{
	struct bundle_processor_signal signal = {
		.type = type,
		.reason = reason,
		.bundle = bundle
	};

	hal_queue_push_to_back(signaling_queue, &signal);
}

void bundle_processor_task(void * const param)
{
	struct bundle_processor_task_parameters *p =
		(struct bundle_processor_task_parameters *)param;
	struct bundle_processor_signal signal;

	out_queue = p->router_signaling_queue;

	for (;;) {
		if (hal_queue_receive(p->signaling_queue, &signal,
			-1) == RETURN_SUCCESS
		) {
			handle_signal(signal);
		}
	}
}

static inline void handle_signal(const struct bundle_processor_signal signal)
{
	struct bundle *b = bundle_storage_get(signal.bundle);

	if (b == NULL) {
		LOGI("BundleProcessor: Could not process signal", signal.type);
		LOGI("BundleProcessor: Bundle not found", signal.bundle);
		return;
	}
	switch (signal.type) {
	case BP_SIGNAL_BUNDLE_INCOMING:
		bundle_receive(b);
		break;
	case BP_SIGNAL_BUNDLE_ROUTED:
		bundle_forwarding_scheduled(b);
		break;
	case BP_SIGNAL_FORWARDING_CONTRAINDICATED:
		bundle_forwarding_contraindicated(b, signal.reason);
		break;
	case BP_SIGNAL_BUNDLE_EXPIRED:
		bundle_expired(b);
		break;
	case BP_SIGNAL_RESCHEDULE_BUNDLE:
		bundle_dangling(b);
		break;
	case BP_SIGNAL_TRANSMISSION_SUCCESS:
		bundle_forwarding_success(b);
		break;
	case BP_SIGNAL_TRANSMISSION_FAILURE:
		bundle_forwarding_failed(b,
			BUNDLE_SR_REASON_TRANSMISSION_CANCELED);
		break;
	default:
		LOGA("BundleProcessor: Invalid signal detected",
			signal.type, LOG_NO_ITEM);
		break;
	}
	/* bundle_storage_update(b); */
}

/* BUNDLE HANDLING */

/* 5.3 */
static void bundle_dispatch(struct bundle *bundle)
{
	/* 5.3-1 */
	if (bundle_endpoint_is_local(bundle)) {
		bundle_deliver_local(bundle);
		return;
	}
	/* 5.3-2 */
	bundle_forward(bundle);
}

/* 5.3-1 */
static bool bundle_endpoint_is_local(struct bundle *bundle)
{
	/* Calculate the length of the predefined EID part */
	size_t id_length = strlen(UPCN_SCHEME)
				+ strlen(UPCN_SSP)
				+ 1;
	/* Compare bundle destination EID with configured upcn EID */
	return strncmp(bundle->destination,
		       UPCN_SCHEME ":" UPCN_SSP,
		       id_length) == 0;
}

/* 5.4 */
static void bundle_forward(struct bundle *bundle)
{
	/* 4.3.4. Hop Count (BPv7-bis) */
	/* TODO: Is this the correct point to perform the hop-count check? */
	if (!hop_count_validation(bundle))
		return;

	/* 5.4-1 */
	bundle_add_rc(bundle, BUNDLE_RET_CONSTRAINT_FORWARD_PENDING);
	bundle_rem_rc(bundle, BUNDLE_RET_CONSTRAINT_DISPATCH_PENDING, 0);
	/* 5.4-2 */
	send_bundle(bundle->id, 0);
	/* For steps after 5.4-2, see below */
}

/* 5.4-4 */
static void bundle_forwarding_scheduled(struct bundle *bundle)
{
	/* 5.4-4 */
	/* Custody may have already been accepted if we are re-scheduling */
	if (
		HAS_FLAG(bundle->proc_flags,
			 BUNDLE_FLAG_CUSTODY_TRANSFER_REQUESTED)
		&& !HAS_FLAG(bundle->ret_constraints,
			BUNDLE_RET_CONSTRAINT_CUSTODY_ACCEPTED)
		&& bundle->protocol_version == 6
	) {
		/* bundle_receive already checked if bundle is acceptable */
		bundle_custody_accept(bundle);
	}
	/* 5.4-5 is done by contact manager / ground station task */
}

/* 5.4-6 */
static void bundle_forwarding_success(struct bundle *bundle)
{
	if (HAS_FLAG(bundle->proc_flags, BUNDLE_FLAG_REPORT_FORWARDING)) {
		/* See 5.4-6: reason code vs. unidirectional links */
		send_status_report(bundle,
			BUNDLE_SR_FLAG_BUNDLE_FORWARDED,
			BUNDLE_SR_REASON_NO_INFO);
	}
	bundle_rem_rc(bundle, BUNDLE_RET_CONSTRAINT_FORWARD_PENDING, 0);
	bundle_rem_rc(bundle, BUNDLE_RET_CONSTRAINT_FLAG_OWN, 1);
}

/* 5.4.1 */
static void bundle_forwarding_contraindicated(
	struct bundle *bundle, enum bundle_status_report_reason reason)
{
	/* 5.4.1-1: For now, we declare forwarding failure everytime */
	bundle_forwarding_failed(bundle, reason);
	/* 5.4.1-2 (a): At the moment, custody transfer is declared as failed */
	/* 5.4.1-2 (b): Will not be handled */
}

/* 5.4.2 */
static void bundle_forwarding_failed(
	struct bundle *bundle, enum bundle_status_report_reason reason)
{
	enum bundle_custody_signal_reason cs_reason;

	if (HAS_FLAG(bundle->proc_flags,
		BUNDLE_FLAG_CUSTODY_TRANSFER_REQUESTED)
	) {
		if (HAS_FLAG(bundle->ret_constraints,
			BUNDLE_RET_CONSTRAINT_CUSTODY_ACCEPTED)
		) {
			/* TODO: See 5.12, evaluate what to do here */
			/* We are the current custodian and... */
			/* a) were re-scheduling but it wasn't possible or */
			/* b) the GS task failed */
		} else {
			/* We tried to schedule the bundle but it failed */
			cs_reason = (reason < BUNDLE_SR_REASON_DEPLETED_STORAGE
					? BUNDLE_CS_REASON_NO_INFO
					: (enum bundle_custody_signal_reason)
						reason);
			send_custody_signal(bundle, BUNDLE_CS_TYPE_REFUSAL,
				cs_reason);
		}
	}
	bundle_delete(bundle, reason);
}

/* 5.5 */
static void bundle_expired(struct bundle *bundle)
{
	bundle_delete(bundle, BUNDLE_SR_REASON_LIFETIME_EXPIRED);
}

/* 5.6 */
static void bundle_receive(struct bundle *bundle)
{
	struct bundle_block_list **e;
	enum bundle_handling_result res;
	/* 5.6-1 Add retention constraint */
	bundle_add_rc(bundle, BUNDLE_RET_CONSTRAINT_DISPATCH_PENDING);
	/* 5.6-2 Request reception */
	if (HAS_FLAG(bundle->proc_flags, BUNDLE_FLAG_REPORT_RECEPTION))
		send_status_report(bundle,
			BUNDLE_SR_FLAG_BUNDLE_RECEIVED,
			BUNDLE_SR_REASON_NO_INFO);
	/* 5.6-3 Handle blocks */
	e = &bundle->blocks;
	while (*e != NULL) {
		if ((*e)->data->type != BUNDLE_BLOCK_TYPE_PAYLOAD) {
			res = handle_unknown_block_flags(
				bundle, (*e)->data->flags);
			switch (res) {
			case BUNDLE_HRESULT_OK:
				(*e)->data->flags |=
					BUNDLE_BLOCK_FLAG_FWD_UNPROC;
				break;
			case BUNDLE_HRESULT_DELETED:
				bundle_delete(bundle,
					BUNDLE_SR_REASON_BLOCK_UNINTELLIGIBLE);
				return;
			case BUNDLE_HRESULT_BLOCK_DISCARDED:
				*e = bundle_block_entry_free(*e);
				break;
			}

		}
		if (*e != NULL)
			e = &(*e)->next;
	}
	if (HAS_FLAG(bundle->proc_flags,
		     BUNDLE_FLAG_CUSTODY_TRANSFER_REQUESTED)
		&& bundle->protocol_version == 6
	) {
		if (custody_manager_has_redundant_bundle(bundle)) {
			/* 5.6-4 */
			send_custody_signal(bundle, BUNDLE_CS_TYPE_REFUSAL,
				BUNDLE_CS_REASON_REDUNDANT_RECEPTION);
			bundle_rem_rc(bundle,
				BUNDLE_RET_CONSTRAINT_DISPATCH_PENDING, 1);
		} else if (!custody_manager_storage_is_acceptable(bundle)) {
			/* This is not part of the specification, but we have */
			/* to check if we want to reject custody before */
			/* dispatching the bundle. */
			/* In that case, the bundle would be deleted. */
			send_custody_signal(bundle, BUNDLE_CS_TYPE_REFUSAL,
				BUNDLE_CS_REASON_DEPLETED_STORAGE);
			bundle_delete(bundle,
				BUNDLE_SR_REASON_DEPLETED_STORAGE);
		}
	} else {
		/* 5.6-5 */
		bundle_dispatch(bundle);
	}

#ifdef THROUGHPUT_TEST
	timestamp_mp2[bundle->sequence_number] = hal_time_get_timestamp_us();
#endif

}

/* 5.6-3 */
static enum bundle_handling_result handle_unknown_block_flags(
	struct bundle *bundle, enum bundle_block_flags flags)
{
	if (HAS_FLAG(flags,
		BUNDLE_BLOCK_FLAG_REPORT_IF_UNPROC)
	) {
		send_status_report(bundle,
			BUNDLE_SR_FLAG_BUNDLE_RECEIVED,
			BUNDLE_SR_REASON_BLOCK_UNINTELLIGIBLE);
	}
	if (HAS_FLAG(flags, BUNDLE_BLOCK_FLAG_DELETE_BUNDLE_IF_UNPROC))
		return BUNDLE_HRESULT_DELETED;
	else if (HAS_FLAG(flags, BUNDLE_BLOCK_FLAG_DISCARD_IF_UNPROC))
		return BUNDLE_HRESULT_BLOCK_DISCARDED;
	return BUNDLE_HRESULT_OK;
}

/* 5.7 */
static void bundle_deliver_local(struct bundle *bundle)
{
	LOGF("Payload size is %d", bundle->payload_block->length);
	struct bundle_administrative_record *record;

	/* We can't handle bundles which are fragmented (see 5.9) */
	/* or no administrative records */
	if (
		!HAS_FLAG(bundle->proc_flags, BUNDLE_FLAG_IS_FRAGMENT)
		&& HAS_FLAG(bundle->proc_flags,
			BUNDLE_FLAG_ADMINISTRATIVE_RECORD)
	) {
		record = parse_administrative_record(bundle);
		if (record != NULL && record->type == BUNDLE_AR_CUSTODY_SIGNAL)
			bundle_handle_custody_signal(record);
		free_administrative_record(record);
	}
	LOG("Received local bundle!");
	/* Calculate the length of the predefined EID part */
	size_t id_length = strlen(UPCN_SCHEME)
				+ strlen(UPCN_SSP)
				+ 1;
	size_t pl_length;
	uint8_t *pl_data = bundle_get_payload_data(bundle, &pl_length);

	agent_forward((bundle->destination + id_length + 1),
		      (char *)pl_data, pl_length);
	/* This drops the bundle */
	bundle_rem_rc(bundle, BUNDLE_RET_CONSTRAINT_DISPATCH_PENDING, 1);
}

/* 5.10 */
static void bundle_custody_accept(struct bundle *bundle)
{
	if (custody_manager_accept(bundle) != UPCN_OK) {
		/* TODO */
		return;
	}

	if (HAS_FLAG(bundle->proc_flags,
		BUNDLE_FLAG_REPORT_CUSTODY_PROCESSING)
	) {
		/* TODO: 5.10.1: */
		/* However, if a bundle reception status report was */
		/* generated for this bundle (Step 1 of Section 5.6), */
		/* then this report should be generated by simply turning on */
		/* the "Reporting node accepted custody of bundle" flag */
		/* in that earlier report's status flags byte. */
		send_status_report(bundle, BUNDLE_SR_FLAG_CUSTODY_TRANSFER,
			BUNDLE_SR_REASON_NO_INFO);
	}
	send_custody_signal(bundle, BUNDLE_CS_TYPE_ACCEPTANCE,
		BUNDLE_CS_REASON_NO_INFO);
}

/* 5.11 */
static void bundle_custody_success(struct bundle *bundle)
{
	/* 5.10.2 */
	custody_manager_release(bundle);
}

/* 5.12 */
/* "Custody failed" signal received or timer expired (TODO) */
static void bundle_custody_failure(
	struct bundle *bundle, enum bundle_custody_signal_reason reason)
{
	switch (reason) {
		/* TODO: Which reports should be generated? */
	default:
		custody_manager_release(bundle);
		break;
	}
}

/* 5.13 (BPv7) */
/* TODO: Custody Transfer Deferral */

/* 5.13 (RFC 5050) */
/* 5.14 (BPv7-bis) */
static void bundle_delete(
	struct bundle *bundle, enum bundle_status_report_reason reason)
{
	bool generate_report = false;

	if (HAS_FLAG(bundle->ret_constraints,
		BUNDLE_RET_CONSTRAINT_CUSTODY_ACCEPTED)
	) {
		custody_manager_release(bundle);
		generate_report = true;
	} else if (HAS_FLAG(bundle->proc_flags,
		BUNDLE_FLAG_REPORT_DELETION)) {
		generate_report = true;
	}

	if (generate_report)
		send_status_report(bundle,
			BUNDLE_SR_FLAG_BUNDLE_DELETED, reason);

	bundle->ret_constraints &= BUNDLE_RET_CONSTRAINT_NONE;
	bundle_discard(bundle);
}

/* 5.14 (RFC 5050) */
/* 5.15 (BPv7-bis) */
static void bundle_discard(struct bundle *bundle)
{
	bundle_storage_delete(bundle->id);
	bundle_drop(bundle);
}

/* 6.3 */
static void bundle_handle_custody_signal(
	struct bundle_administrative_record *signal)
{
	struct bundle *bundle = custody_manager_get_by_record(signal);

	if (bundle == NULL)
		return;
	if (signal->custody_signal->type == BUNDLE_CS_TYPE_ACCEPTANCE)
		bundle_custody_success(bundle);
	else
		bundle_custody_failure(bundle, signal->custody_signal->reason);
}

/* RE-SCHEDULING */
static void bundle_dangling(struct bundle *bundle)
{
	uint8_t resched = 0;

	switch (FAILED_FORWARD_POLICY) {
	case POLICY_TRY_RE_SCHEDULE:
		resched = 1;
		break;
	case POLICY_DROP_IF_NO_CUSTODY:
		resched = (HAS_FLAG(bundle->ret_constraints,
				BUNDLE_RET_CONSTRAINT_CUSTODY_ACCEPTED) ||
			HAS_FLAG(bundle->ret_constraints,
				BUNDLE_RET_CONSTRAINT_FLAG_OWN));
		break;
	}
	/* Send it to the router task again after evaluating policy. */
	if (!resched || send_bundle(bundle->id, FAILED_FORWARD_TIMEOUT)
			== RETURN_FAILURE
	) {
		bundle_delete(bundle, BUNDLE_SR_REASON_TRANSMISSION_CANCELED);
	}
}

/* HELPERS */

static void send_status_report(
	struct bundle *bundle,
	const enum bundle_status_report_status_flags status,
	const enum bundle_status_report_reason reason)
{
	/* If the report-to EID is the null endpoint or upcn itself we do not */
	/* need to create a status report */
	if (strcmp(bundle->destination, "dtn:none") == 0
		|| strcmp(bundle->destination, UPCN_SCHEME ":" UPCN_SSP) == 0)
		return;

	struct bundle_status_report report = {
		.status = status,
		.reason = reason
	};
	struct bundle *b = generate_status_report(bundle, &report);

	if (b != NULL) {
		bundle_add_rc(b, BUNDLE_RET_CONSTRAINT_DISPATCH_PENDING);
		bundle_storage_add(b);
		bundle_forward(b);
	}
}

static void send_custody_signal(struct bundle *bundle,
	const enum bundle_custody_signal_type type,
	const enum bundle_custody_signal_reason reason)
{
	struct bundle_custody_signal signal = {
		.type = type,
		.reason = reason,
	};
	struct bundle_list *next;
	struct bundle_list *signals =
		generate_custody_signals(bundle, &signal);

	/* Walk through all signals and forward them to their destination */
	while (signals != NULL) {
		bundle_add_rc(signals->data,
			BUNDLE_RET_CONSTRAINT_DISPATCH_PENDING);
		bundle_storage_add(signals->data);
		bundle_forward(signals->data);

		/* Free current list entry (but not the bundle itself) */
		next = signals->next;
		free(signals);
		signals = next;
	}
}

static uint8_t send_bundle(bundleid_t bundle, uint16_t timeout)
{
	struct router_signal signal = {
		.type = ROUTER_SIGNAL_ROUTE_BUNDLE,
		/* A little bit ugly but saves RAM... */
		.data = (void *)(uintptr_t)bundle
	};

	if (timeout == 0) {
		hal_queue_push_to_back(out_queue, &signal);
		return RETURN_SUCCESS;
	}
	return hal_queue_try_push_to_back(out_queue,
					  &signal,
					  timeout);
}

/**
 * Returns the first occurence if a specific block type in the given list
 */
static struct bundle_block *find_block_by_type(struct bundle_block_list *blocks,
	enum bundle_block_type type)
{
	while (blocks != NULL) {
		if (blocks->data->type == BUNDLE_BLOCK_TYPE_HOP_COUNT)
			return blocks->data;
		blocks = blocks->next;
	}

	return NULL;
}

/**
 * 4.3.4. Hop Count (BPv7-bis)
 *
 * Checks if the hop limit exceeds the hop limit. If yes, the bundle gets
 * deleted and false is returned. Otherwise the hop count is incremented
 * and true is returned.
 *
 * TODO: And which method should be invoked: "Delete" or "Discard"?
 *       A question regarding this topic has been sent to the
 *       DTN IETF mailing list. Waiting for response ...
 *
 * @return false if the hop count exeeds the hop limit, true otherwise
 */
static bool hop_count_validation(struct bundle *bundle)
{
	struct bundle_block *block = find_block_by_type(bundle->blocks,
		BUNDLE_BLOCK_TYPE_HOP_COUNT);

	/* No Hop Count block was found */
	if (block == NULL)
		return true;

	struct bundle_hop_count hop_count;
	bool success = bundle7_hop_count_parse(&hop_count,
		block->data, block->length);

	/* If block data cannot be parsed, ignore it */
	if (!success) {
		LOGI("BundleProcessor: Could not parse hop-count block",
			bundle->id);
		return true;
	}

	/* Hop count exeeded, delete bundle */
	if (hop_count.count >= hop_count.limit) {
		/* TODO: Which reason code should be used? */
		bundle_delete(bundle, BUNDLE_SR_REASON_NO_INFO);
		return false;
	}

	/* Increment Hop Count */
	hop_count.count++;

	/* CBOR-encoding */
	uint8_t *buffer = malloc(BUNDLE7_HOP_COUNT_MAX_ENCODED_SIZE);

	/* Out of memory - validation passes none the less */
	if (buffer == NULL) {
		LOGI("BundleProcessor: Could not increment hop-count",
			bundle->id);
		return true;
	}

	free(block->data);

	block->data = buffer;
	block->length = bundle7_hop_count_serialize(&hop_count,
		buffer, BUNDLE7_HOP_COUNT_MAX_ENCODED_SIZE);

	return true;
}
