#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdbool.h>

#include <errno.h>
#include <signal.h>

#include <zmq.h>

#include "common.h"

static const char *const COMMAND_TYPES[] = {
#define X(x) #x
#include "nd_commands.def"
#undef X
};

static void *ctx;
static void *subscriber, *client;
static void *server;

static bool verbose; // = false

static int init_zmq(
	const char *const listen_addr,
	const char *const ucpub_addr, const char *const ucrep_addr);

static int nd_cmd_recv_next(void *const server, struct nd_command *const cmd);
static int nd_cmd_send_result(
	void *const server, const struct nd_result *const result);

static void exit_handler(void);
static void signal_handler(int sig);

int main(int argc, char *argv[])
{
	int remote, ret;
	struct nd_command cmd;
	struct nd_result result;

	if (argc > 1 && strcmp(argv[1], "-v") == 0) {
		verbose = true;
		argc--;
		argv = &argv[1];
	}

	if (argc != 2 && argc != 4) {
		fprintf(stderr, "Usage: upcn_nd_proxy [-v] <listen_address> %s",
			"[upcn_connect_publisher upcn_connect_listener]\n");
		exit_handler();
		return EXIT_FAILURE;
	}

	remote = (argc == 4);
	printf(
		"uPCN Neigbor Discovery Proxy - %s mode\n",
		remote ? "REMOTE (upcn_connect)" : "LOCAL");
	ret = init_zmq(argv[1],
		remote ? argv[2] : NULL,
		remote ? argv[3] : NULL);
	if (ret != 0) {
		exit_handler();
		return EXIT_FAILURE;
	}

	signal(SIGPIPE, &signal_handler);
	signal(SIGINT, &signal_handler);
	signal(SIGTERM, &signal_handler);
	signal(SIGHUP, &signal_handler);

	if (remote && !upcn_is_available(&client, &subscriber, 1)) {
		exit_handler();
		return EXIT_FAILURE;
	}

	for (;;) {
		if (nd_cmd_recv_next(server, &cmd) != 0)
			break;
		if (remote)
			nd_cmd_process_upcn(&cmd, &result, &client, &subscriber,
					    verbose);
		else
			nd_cmd_process_locally(&cmd, &result, verbose);
		if ((result.rc & RRND_STATUS_FAILED) != 0 && verbose) {
			printf("> Failed processing %s: rc = 0x%04x\n",
				COMMAND_TYPES[cmd.type], result.rc);
		}
		if (nd_cmd_send_result(server, &result) != 0)
			break;
	}

	exit_handler();
	return EXIT_FAILURE;
}

static int init_zmq(
	const char *const listen_addr,
	const char *const ucpub_addr, const char *const ucrep_addr)
{
	const int timeout = ND_ZMQ_TIMEOUT;
	const int linger = ND_ZMQ_LINGER_TIMEOUT;

	ctx = zmq_ctx_new();
	server = zmq_socket(ctx, ZMQ_REP);
	zmq_setsockopt(server, ZMQ_SNDTIMEO, &timeout, sizeof(int));
	zmq_setsockopt(server, ZMQ_LINGER, &linger, sizeof(int));
	if (zmq_bind(server, listen_addr) != 0) {
		perror("zmq_connect()");
		fprintf(stderr, "Could not bind to server socket\n");
		return -1;
	}
	if (ucpub_addr != NULL && ucrep_addr != NULL)
		return nd_connect_upcn(
			ctx, &client, &subscriber, ucrep_addr, ucpub_addr);
	return 0;
}

static int nd_cmd_recv_next(void *const server, struct nd_command *const cmd)
{
	static char buf[4096];
	int len;
	char *err;

	memset(cmd, 0, sizeof(struct nd_command));
	cmd->time = LLONG_MAX;
	len = zmq_recv(server, buf, ARRAY_SIZE(buf) - 1, 0);
	if (len < 0) {
		perror("zmq_recv()");
		fprintf(stderr, "Could not receive from socket\n");
		return -1;
	}
	buf[len] = '\0';
	err = nd_cmd_parse(buf, len, cmd);
	if (err != NULL) {
		cmd->type = ND_COMMAND_UNKNOWN;
		fprintf(stderr, "%s\n", err);
	}
	return 0;
}

static int nd_cmd_send_result(
	void *const server, const struct nd_result *const result)
{
	static char buf[4096];
	int len;

	if (result->gs_data_json[0] != '\0')
		sprintf(buf, "{\"result\": %u, \"gs\": %s}",
			result->rc, result->gs_data_json);
	else
		sprintf(buf, "{\"result\": %u}", result->rc);
	len = zmq_send(server, buf, strlen(buf), 0);
	if (len < 0) {
		perror("zmq_send()");
		fprintf(stderr, "Could not send via socket\n");
		return -1;
	}
	return 0;
}

static void exit_handler(void)
{
	if (ctx != NULL) {
		if (subscriber != NULL)
			zmq_close(subscriber);
		if (client != NULL)
			zmq_close(client);
		if (server != NULL)
			zmq_close(server);
		zmq_ctx_destroy(ctx);
		ctx = server = NULL;
		client = subscriber = NULL;
	}
}

static void signal_handler(int sig)
{
	exit_handler();
	exit(EXIT_SUCCESS);
}
