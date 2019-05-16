#include <stdlib.h>

#include "upcn/rrndCommand.h"

#include "upcn/satpos.h"
#include "upcn/rrnd.h"
#include "upcn/routingTable.h"
#include "upcn/upcn.h"

#include <stddef.h>
#include <stdint.h>

static const unsigned int RRND_CHARSTATE_SUCCESS = RRND_STATUS_SUCCESS;
static const unsigned int RRND_CHARSTATE_FAILURE = RRND_STATUS_FAILED;

static void send_rrnd_success(void)
{
	LOG("RRND command executed successfully");
	hal_io_send_packet(
		&RRND_CHARSTATE_SUCCESS,
		RRND_STATUS_SIZEOF,
		COMM_TYPE_RRND_STATUS
	);
}

static void send_rrnd_failure(void)
{
	LOG("RRND command FAILED");
	hal_io_send_packet(
		&RRND_CHARSTATE_FAILURE,
		RRND_STATUS_SIZEOF,
		COMM_TYPE_RRND_STATUS
	);
}

static void perform_rrnd_contact_inference(const char *const gs_eid)
{
	uint64_t cur_time = hal_time_get_timestamp_ms();
	struct ground_station *const gs =
		routing_table_lookup_ground_station(gs_eid);
	enum rrnd_status res;

	if (!gs || !gs->rrnd_info) {
		LOG("GS for inferring contact not found");
		send_rrnd_failure();
		return;
	}
	res = rrnd_infer_contact(gs->rrnd_info, cur_time, satpos_get,
				 satpos_get_age(cur_time));
#ifdef INTEGRATE_DEMANDED_CONTACTS
	if (!HAS_FLAG(res, RRND_STATUS_FAILED))
		routing_table_integrate_inferred_contact(gs, 0);
#endif /* INTEGRATE_DEMANDED_CONTACTS */
	hal_io_send_packet(&res, RRND_STATUS_SIZEOF, COMM_TYPE_RRND_STATUS);
}

static void perform_rrnd_gs_output(const char *const gs_eid)
{
	// TODO: Reduce the size of this static buffer or allocate dynamically
	static char gsbuf[4096];
	const struct ground_station *const gs =
		routing_table_lookup_ground_station(gs_eid);
	size_t buflen;

	buflen = rrnd_print_gs_info(gsbuf, 4096, gs_eid,
				    gs ? gs->rrnd_info : NULL);
	hal_io_send_packet(gsbuf, buflen, COMM_TYPE_GS_INFO);
}

static void perform_rrnd_metric_integration(
	const char *const gs_eid, const char *const source_gs_eid,
	const float probability)
{
	struct ground_station *const gs =
		routing_table_lookup_ground_station(gs_eid);
	const struct ground_station *const source_gs =
		routing_table_lookup_ground_station(source_gs_eid);
	static struct rrnd_probability_metrics prob_metrics;
	enum rrnd_status res;

	if (!gs || !gs->rrnd_info || !source_gs || !source_gs->rrnd_info) {
		LOG("GS for integrating metrics not found");
		send_rrnd_failure();
		return;
	}
	prob_metrics.reliability = probability;
	res = rrnd_integrate_metrics(gs->rrnd_info, source_gs->rrnd_info,
				     prob_metrics);
	hal_io_send_packet(&res, RRND_STATUS_SIZEOF, COMM_TYPE_RRND_STATUS);
}

void rrnd_execute_command(struct rrnd_command *cmd)
{
	ASSERT(cmd != NULL);
	switch (cmd->type) {
	case RRND_COMMAND_INITIALIZE_SATPOS:
		LOG("Initializing SATPOS...");
		satpos_init(
			cmd->tle,
			hal_time_get_timestamp_ms()
		);
		if (satpos_last_result() == SATPOS_SUCCESS)
			send_rrnd_success();
		else
			send_rrnd_failure();
		break;
	case RRND_COMMAND_INFER_CONTACT:
		perform_rrnd_contact_inference(cmd->gs);
		break;
	case RRND_COMMAND_QUERY_GS:
		perform_rrnd_gs_output(cmd->gs);
		break;
	case RRND_COMMAND_INTEGRATE_METRICS:
		perform_rrnd_metric_integration(
			cmd->gs,
			cmd->source_gs,
			cmd->gs_reliability
		);
		break;
	default:
		break;
	}
	if (cmd->gs != NULL)
		free(cmd->gs);
	if (cmd->source_gs != NULL)
		free(cmd->source_gs);
	if (cmd->tle != NULL)
		free(cmd->tle);
	free(cmd);
}
