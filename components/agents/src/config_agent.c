#include <stdio.h>
#include <string.h>

#include "agents/config_agent.h"
#include "hal_debug.h"
#include "upcn/agent_manager.h"
#include "upcn/init.h"

#include "agents/config_parser.h"

static void config_agent_task(void * const param);

static QueueIdentifier_t config_agent_queue;
static QueueIdentifier_t router_signaling_queue;
static struct config_parser parser;



void callback(const void *data, const size_t length)
{
	/* copy data to own data structure */
	uint8_t *data_new = malloc(length);
	struct config_agent_item item;

	memcpy(data_new, data, length);
	item.data = data_new;
	item.data_length = length;

	hal_queue_push_to_back(config_agent_queue, (void *)&item);
}

int config_agent_setup(void)
{
	/* create queue to store received payload commands */
	config_agent_queue = hal_queue_create(CONFIG_AGENT_QUEUE_SIZE,
					      sizeof(struct config_agent_item));

	/* create task that parses the payload commands */
	hal_task_create(config_agent_task,
			"ca_t",
			CONFIG_AGENT_TASK_PRIORITY,
			NULL,
			DEFAULT_TASK_STACK_SIZE,
			(void *)CONFIG_AGENT_TASK_TAG);

	return agent_register("config", callback);
}

static void router_command_send(struct router_command *cmd)
{
	struct router_signal signal = {
		.type = ROUTER_SIGNAL_PROCESS_COMMAND,
		.data = cmd
	};

	ASSERT(cmd != NULL);
	hal_queue_push_to_back(router_signaling_queue, &signal);
}


static void config_agent_task(void * const param)
{
	struct config_agent_item item;

	LOG("Started configAgent task successfully!");

	router_signaling_queue = get_global_router_signaling_queue();

	ASSERT(config_parser_init(&parser, &router_command_send));

	for (;;) {
		hal_queue_receive(config_agent_queue, &item, -1);
		LOG("configAgentTask: got routing command!");
		config_parser_reset(&parser);
		config_parser_read(&parser, item.data, item.data_length);
		free(item.data);
	}
}
