#include "cla/cla.h"
#include "cla/mtcp_proto.h"
#include "cla/posix/cla_mtcp.h"
#include "cla/posix/cla_smtcp.h"
#include "cla/posix/cla_tcp_common.h"
#include "cla/posix/cla_tcp_util.h"

#include "bundle6/parser.h"
#include "bundle7/parser.h"

#include "platform/hal_config.h"
#include "platform/hal_io.h"
#include "platform/hal_task.h"

#include "upcn/bundle_agent_interface.h"
#include "upcn/cmdline.h"
#include "upcn/common.h"
#include "upcn/config.h"
#include "upcn/result.h"
#include "upcn/task_tags.h"

#include <sys/socket.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>


static void smtcp_link_creation_task(void *param)
{
	struct cla_tcp_single_config *const smtcp_config = param;

	LOGF("smtcp: Using %s mode",
	     smtcp_config->tcp_active ? "active" : "passive");

	cla_tcp_single_link_creation_task(
		smtcp_config,
		sizeof(struct mtcp_link)
	);
	ASSERT(0);
}

static enum upcn_result smtcp_launch(struct cla_config *const config)
{
	struct cla_tcp_single_config *const smtcp_config =
		(struct cla_tcp_single_config *)config;

	smtcp_config->base.listen_task = hal_task_create(
		smtcp_link_creation_task,
		"smtcp_listen_t",
		CONTACT_LISTEN_TASK_PRIORITY,
		config,
		CONTACT_LISTEN_TASK_STACK_SIZE,
		(void *)CLA_SPECIFIC_TASK_TAG
	);

	if (!smtcp_config->base.listen_task)
		return UPCN_FAIL;

	return UPCN_OK;
}

static const char *smtcp_name_get(void)
{
	return "smtcp";
}

const struct cla_vtable smtcp_vtable = {
	.cla_name_get = smtcp_name_get,
	.cla_launch = smtcp_launch,

	.cla_mbs_get = mtcp_mbs_get,

	.cla_get_tx_queue = cla_tcp_single_get_tx_queue,
	.cla_start_scheduled_contact = cla_tcp_single_start_scheduled_contact,
	.cla_end_scheduled_contact = cla_tcp_single_end_scheduled_contact,

	.cla_begin_packet = mtcp_begin_packet,
	.cla_end_packet = mtcp_end_packet,
	.cla_send_packet_data = mtcp_send_packet_data,

	.cla_rx_task_reset_parsers = mtcp_reset_parsers,
	.cla_rx_task_forward_to_specific_parser =
		mtcp_forward_to_specific_parser,

	.cla_read = cla_tcp_read,

	.cla_disconnect_handler = cla_tcp_single_disconnect_handler,
};

static enum upcn_result smtcp_init(
	struct cla_tcp_single_config *config,
	const char *node, const char *service, const bool tcp_active,
	const struct bundle_agent_interface *bundle_agent_interface)
{
	/* Initialize base_config */
	if (cla_tcp_single_config_init(config, bundle_agent_interface)
			!= UPCN_OK)
		return UPCN_FAIL;

	/* set base_config vtable */
	config->base.base.vtable = &smtcp_vtable;

	config->tcp_active = tcp_active;

	/* Start listening */
	if (!tcp_active &&
			cla_tcp_listen(&config->base,
				       node, service,
				       CLA_TCP_SINGLE_BACKLOG) != UPCN_OK)
		return UPCN_FAIL;
	else if (tcp_active &&
			cla_tcp_connect(&config->base,
					node, service) != UPCN_OK)
		return UPCN_FAIL;

	return UPCN_OK;
}

struct cla_config *smtcp_create(
	const char *const options[], const size_t option_count,
	const struct bundle_agent_interface *bundle_agent_interface)
{
	if (option_count < 2 || option_count > 3) {
		LOG("smtcp: Options format has to be: <IP>,<PORT>[,<TCP_ACTIVE>]");
		return NULL;
	}

	bool tcp_active = false;

	if (option_count > 2) {
		if (parse_tcp_active(options[2], &tcp_active) != UPCN_OK) {
			LOGF("smtcp: Could not parse TCP active flag: %s",
			     options[2]);
			return NULL;
		}
	}

	struct cla_tcp_single_config *config =
		malloc(sizeof(struct cla_tcp_single_config));

	if (!config) {
		LOG("smtcp: Memory allocation failed!");
		return NULL;
	}

	if (smtcp_init(config, options[0], options[1], tcp_active,
		       bundle_agent_interface) != UPCN_OK) {
		free(config);
		LOG("smtcp: Initialization failed!");
		return NULL;
	}

	return &config->base.base;
}
