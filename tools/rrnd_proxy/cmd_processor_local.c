#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include "upcn/upcn.h"
#include "upcn/satpos.h"
#include "upcn/rrnd.h"
#include "drv/json_escape.h"

#include "common.h"

#define MSTIME(t) ((unsigned long long)(t) * 1000ULL)

/* As everything is static, it is initialized by zero */
static char gslist[ND_LOCAL_MAX_GS][ND_LOCAL_MAX_EID_LENGTH];
static struct rrnd_gs_info gsinfo[ND_LOCAL_MAX_GS];
static unsigned long gscount;

static int routing_db_local_get(
	struct rrnd_gs_info *const r, const char *const eid);
static int routing_db_local_update_gs(const struct rrnd_gs_info info);
static int routing_db_local_delete_gs(const struct rrnd_gs_info info);

void nd_cmd_process_locally(
	const struct nd_command *const cmd, struct nd_result *const res,
	const bool verbose)
{
	struct rrnd_gs_info modified_gs, tmp_gs;

	res->rc = RRND_STATUS_FAILED;
	res->gs_data_json[0] = '\0';
	switch (cmd->type) {
	case ND_COMMAND_TEST_CONNECTIVITY:
		res->rc = RRND_STATUS_SUCCESS;
		break;
	case ND_COMMAND_INIT:
		if (cmd->tle[0] == '\0' || cmd->time == LLONG_MAX)
			break;
		rrnd_io_debug_set_enabled(verbose);
		satpos_init(cmd->tle, MSTIME(cmd->time));
		if (satpos_last_result() == SATPOS_SUCCESS) {
			res->rc = RRND_STATUS_SUCCESS;
			puts("> SATPOS initialized successfully");
		}
		break;
	case ND_COMMAND_RESET:
		gscount = 0;
		res->rc = RRND_STATUS_SUCCESS;
		puts("> RRND GS records reset");
		break;
	case ND_COMMAND_PROCESS_BEACON:
		if (cmd->eid[0] == '\0' || cmd->time == LLONG_MAX)
			break;
		if (routing_db_local_get(&modified_gs, cmd->eid) != 0)
			break;
		res->rc = rrnd_process(
			&modified_gs, cmd->beacon,
			MSTIME(cmd->time), 0, satpos_get
		);
		if (HAS_FLAG(res->rc, RRND_STATUS_UPDATED))
			routing_db_local_update_gs(modified_gs);
		else if (HAS_FLAG(res->rc, RRND_STATUS_DELETE))
			routing_db_local_delete_gs(modified_gs);
		break;
	case ND_COMMAND_INFER_CONTACT:
		if (cmd->eid[0] == '\0' || cmd->time == LLONG_MAX)
			break;
		if (routing_db_local_get(&modified_gs, cmd->eid) != 0)
			break;
		res->rc = rrnd_infer_contact(
			&modified_gs, MSTIME(cmd->time), satpos_get,
			satpos_get_age(MSTIME(cmd->time)));
		if (HAS_FLAG(res->rc, RRND_STATUS_UPDATED))
			routing_db_local_update_gs(modified_gs);
		break;
	case ND_COMMAND_GET_GS:
		if (cmd->eid[0] == '\0')
			break;
		if (routing_db_local_get(&modified_gs, cmd->eid) != 0)
			break;
		if (verbose)
			printf("> RRND GS record queried: %s\n", cmd->eid);
		rrnd_print_gs_info(res->gs_data_json, ND_GS_JSON_STR_SIZE,
				   cmd->eid, &modified_gs);
		res->rc = RRND_STATUS_SUCCESS;
		break;
	case ND_COMMAND_RESET_PERF:
		res->rc = RRND_STATUS_SUCCESS;
		break;
	case ND_COMMAND_STORE_PERF:
		printf("> Memory used during run: %lu\n",
		       sizeof(struct rrnd_gs_info) * gscount);
		res->rc = RRND_STATUS_SUCCESS;
		break;
	case ND_COMMAND_UPDATE_METRICS:
		if (cmd->eid[0] == '\0' || cmd->source_eid[0] == '\0')
			break;
		if (routing_db_local_get(&tmp_gs, cmd->source_eid) != 0)
			break;
		if (routing_db_local_get(&modified_gs, cmd->eid) != 0)
			break;
		res->rc = rrnd_integrate_metrics(
			&modified_gs, // GS of which metrics are updated
			&tmp_gs, // source GS
			(struct rrnd_probability_metrics){
				.reliability = cmd->reliability,
				.mean_rx_bitrate = 0,
				.mean_tx_bitrate = 0,
			}
		);
		if (HAS_FLAG(res->rc, RRND_STATUS_UPDATED))
			routing_db_local_update_gs(modified_gs);
		res->rc = RRND_STATUS_SUCCESS;
		break;
	default:
		break;
	}
}

static int routing_db_local_get(
	struct rrnd_gs_info *const r, const char *const eid)
{
	unsigned long i;

	for (i = 0; i < gscount; i++) {
		if (strcmp(gslist[i], eid) == 0) {
			*r = gsinfo[i];
			return 0;
		}
	}
	if (gscount >= ND_LOCAL_MAX_GS)
		return -1;
	if (strlen(eid) >= ND_LOCAL_MAX_EID_LENGTH)
		return -1;
	strncpy(gslist[gscount], eid, ND_LOCAL_MAX_EID_LENGTH);
	memset(&gsinfo[gscount], 0, sizeof(struct rrnd_gs_info));
	gsinfo[gscount].gs_reference = (void *)gscount;
	*r = gsinfo[gscount++];
	return 0;
}

static int routing_db_local_update_gs(const struct rrnd_gs_info info)
{
	unsigned long ref = (unsigned long)info.gs_reference;

	if (ref >= gscount)
		return -1;
	gsinfo[ref] = info;
	return 0;
}

static int routing_db_local_delete_gs(const struct rrnd_gs_info info)
{
	unsigned long ref = (unsigned long)info.gs_reference;

	if (ref >= gscount)
		return -1;
	printf("> RRND GS deleted: %s\n", gslist[ref]);
	gslist[ref][0] = '\0';
	return 0;
}
