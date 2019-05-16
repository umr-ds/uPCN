#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>

#include <zmq.h>

#include "upcn/rrnd.h"
#include "upcn/rrndCommand.h"
#include "upcn/inputParser.h"
#include "upcn/eidManager.h"
#include "upcn/hal/hal_io.h"

#include "serialize_beacon.h"

#include "common.h"

static const char *rep_addr;
static void *zmq_context;

static long upcn_try_rpc(
	void **const client, void **const subscriber,
	uint8_t *const receivebuffer, const size_t bufsize,
	const enum input_type command_id,
	uint8_t *const data, const size_t data_length,
	const enum comm_type result_type);

static long upcn_try_rrnd_cmd(
	void **const client, void **const subscriber,
	uint8_t *const receivebuffer, const size_t bufsize,
	const struct nd_command *cmd, const enum comm_type result_type);

static int set_time(void **const client, void **const subscriber,
	const unsigned long long time_sec);

static long beacon_to_buf(
	void *const buffer, const size_t max,
	const unsigned short seq, const char *const eid, const uint16_t period,
	const uint16_t bitrate_up, const uint16_t bitrate_down,
	const void *const cookie, const size_t cookie_length,
	const char *const *const eids, const size_t eid_count,
	const enum rrnd_beacon_flags rrnd_flags,
	const unsigned long availability_duration);

static int store_upcn_perf_data(const unsigned long long *const pbuf, int size);
static void reset_upcn_perf_data(void);

void nd_cmd_process_upcn(
	const struct nd_command *const cmd, struct nd_result *const res,
	void **const client, void **const info_subscriber, const bool verbose)
{
	static int em_initialized;
	long rc;
	uint8_t buffer[ND_ZMQ_BUFFER_SIZE];

	_Static_assert(RRND_STATUS_SIZEOF == 2, "RRND_STATUS_SIZEOF changed");
	res->rc = RRND_STATUS_FAILED;
	res->gs_data_json[0] = '\0';
	switch (cmd->type) {
	case ND_COMMAND_TEST_CONNECTIVITY:
		if (upcn_is_available(client, info_subscriber, 0))
			res->rc = RRND_STATUS_SUCCESS;
		break;
	case ND_COMMAND_INIT:
		if (cmd->tle[0] == '\0' || cmd->time == LLONG_MAX)
			break;
		puts("> Initializing SATPOS");
		rc = set_time(client, info_subscriber, cmd->time);
		if (rc != 0)
			break;
		rc = upcn_try_rrnd_cmd(
			client, info_subscriber,
			buffer, sizeof(buffer), cmd,
			COMM_TYPE_RRND_STATUS
		);
		if (rc == 5)
			res->rc = (unsigned short)buffer[4] << 8 | buffer[3];
		break;
	case ND_COMMAND_RESET:
		puts("> Performing RESET command...");
		rc = upcn_try_rpc(
			client, info_subscriber,
			buffer, sizeof(buffer),
			INPUT_TYPE_RESET,
			NULL, 0,
			COMM_TYPE_GENERIC_RESULT
		);
		if (rc != 4)
			break;
		if (verbose)
			puts("> Waiting 1s for the reset to occur...");
		sleep(1); /* The board needs some time to perform the reset */
		if (verbose)
			puts("> Testing connectivity and resetting stats...");
		if (upcn_is_available(client, info_subscriber, 0)) {
			rc = upcn_try_rpc(
				client, info_subscriber,
				buffer, sizeof(buffer),
				INPUT_TYPE_RESET_STATS,
				NULL, 0,
				COMM_TYPE_GENERIC_RESULT
			);
			if (rc != 4)
				break;
			res->rc = RRND_STATUS_SUCCESS;
		}
		break;
	case ND_COMMAND_PROCESS_BEACON:
		if (cmd->eid[0] == '\0' || cmd->time == LLONG_MAX)
			break;
		rc = set_time(client, info_subscriber, cmd->time);
		if (rc != 0)
			break;
		if (!em_initialized) {
			/* This is needed for serializing beacons */
			eidmanager_init();
			em_initialized = 1;
		}
		rc = beacon_to_buf(
			buffer,
			sizeof(buffer),
			cmd->beacon.sequence_number,
			cmd->eid, cmd->beacon.period / 100,
			cmd->beacon.tx_bitrate / 8,
			cmd->beacon.rx_bitrate / 8,
			NULL,
			0,
			(const char **)cmd->reported_eids,
			cmd->reported_eid_count,
			cmd->beacon.flags,
			cmd->beacon.availability_duration
		);
		if (rc <= 0)
			break;
		rc = upcn_try_rpc(
			client, info_subscriber,
			buffer, sizeof(buffer),
			INPUT_TYPE_BEACON_DATA,
			buffer, rc,
			COMM_TYPE_RRND_STATUS
		);
		if (rc == 5)
			res->rc = (unsigned short)buffer[4] << 8 | buffer[3];
		break;
	case ND_COMMAND_INFER_CONTACT:
		if (cmd->eid[0] == '\0' || cmd->time == LLONG_MAX)
			break;
		if (verbose)
			printf("> Performing contact inference for GS %s\n",
			       cmd->eid);
		rc = set_time(client, info_subscriber, cmd->time);
		if (rc != 0)
			break;
		rc = upcn_try_rrnd_cmd(
			client, info_subscriber,
			buffer, sizeof(buffer), cmd,
			COMM_TYPE_RRND_STATUS
		);
		if (rc == 5)
			res->rc = (unsigned short)buffer[4] << 8 | buffer[3];
		break;
	case ND_COMMAND_GET_GS:
		if (cmd->eid[0] == '\0')
			break;
		if (verbose)
			printf("> RRND GS record queried for GS %s\n",
			       cmd->eid);
		rc = upcn_try_rrnd_cmd(
			client, info_subscriber,
			buffer, sizeof(buffer), cmd,
			COMM_TYPE_GS_INFO
		);
		if (rc > 10) {
			memcpy(res->gs_data_json, buffer + 3, rc - 3);
			res->gs_data_json[rc - 3] = '\0';
			res->rc = RRND_STATUS_SUCCESS;
		}
		break;
	case ND_COMMAND_UPDATE_METRICS:
		if (cmd->eid[0] == '\0' || cmd->source_eid[0] == '\0')
			break;
		if (verbose)
			printf("> Performing metric integration for GS %s\n",
			       cmd->eid);
		rc = upcn_try_rrnd_cmd(
			client, info_subscriber,
			buffer, sizeof(buffer), cmd,
			COMM_TYPE_RRND_STATUS
		);
		if (rc == 5)
			res->rc = (unsigned short)buffer[4] << 8 | buffer[3];
		break;
	case ND_COMMAND_RESET_PERF:
		printf("> Perf stats reset\n");
		reset_upcn_perf_data();
		res->rc = RRND_STATUS_SUCCESS;
		break;
	case ND_COMMAND_STORE_PERF:
		printf("> Storing perf stats\n");
		rc = upcn_try_rpc(
			client, info_subscriber,
			buffer, sizeof(buffer),
			INPUT_TYPE_DBG_QUERY,
			NULL, 0,
			COMM_TYPE_PERF_DATA
		);
		if (rc < 4 || ((rc - 3) % 8) != 0)
			break;
		if (store_upcn_perf_data((unsigned long long *)(buffer + 3),
					 rc - 3) != 0)
			break;
		res->rc = RRND_STATUS_SUCCESS;
		break;
	default:
		break;
	}
}

int nd_connect_upcn(
	void *ctx, void **client, void **subscriber,
	const char *ucrep_addr, const char *ucpub_addr)
{
	int to = ND_ZMQ_TIMEOUT, li = ND_ZMQ_LINGER_TIMEOUT;

	if (ctx != NULL)
		zmq_context = ctx;
	else
		ctx = zmq_context;
	if (ucpub_addr != NULL) {
		*subscriber = zmq_socket(ctx, ZMQ_SUB);
		zmq_setsockopt(*subscriber, ZMQ_RCVTIMEO, &to, sizeof(int));
		zmq_setsockopt(*subscriber, ZMQ_SUBSCRIBE, "", 0);
		if (zmq_connect(*subscriber, ucpub_addr) != 0) {
			perror("zmq_connect()");
			fprintf(stderr, "Could not connect subscriber\n");
			return -1;
		}
	}
	if (ucrep_addr != NULL)
		rep_addr = ucrep_addr;
	else
		ucrep_addr = rep_addr;
	if (*client != NULL)
		zmq_close(*client);
	*client = zmq_socket(ctx, ZMQ_REQ);
	zmq_setsockopt(*client, ZMQ_SNDTIMEO, &to, sizeof(int));
	zmq_setsockopt(*client, ZMQ_RCVTIMEO, &to, sizeof(int));
	zmq_setsockopt(*client, ZMQ_LINGER, &li, sizeof(int));
	if (zmq_connect(*client, ucrep_addr) != 0) {
		perror("zmq_connect()");
		fprintf(stderr, "Could not connect command client\n");
		return -1;
	}
	return 0;
}

static long write_command(
	void *const buf, const size_t bufsize,
	const enum input_type command_id,
	const void *const data, size_t data_length)
{
	uint8_t *buffer = buf;

	if (bufsize < (data_length + 5))
		return -1;
	buffer[0] = 0x00;
	buffer[1] = 0xFF;
	buffer[2] = (uint8_t)command_id;
	buffer[3] = ':';
	buffer += 4;
	if (data != NULL) {
		memcpy(buffer, data, data_length);
		buffer += data_length;
	}
	buffer[0] = 0xFF;
	return (long)((void *)buffer - buf + 1);
}

static long write_string(
	uint8_t *const buf, const size_t bufsize,
	const char *const string)
{
	size_t str_len = strlen(string);

	if (str_len + 3 > bufsize)
		return -1;
	buf[0] = (str_len & 0xFF00) >> 8;
	buf[1] = (str_len & 0x00FF);
	memcpy(&buf[2], string, str_len);
	return str_len + 2;
}

static long upcn_try_rpc(
	void **const client, void **const subscriber,
	uint8_t *const receivebuffer, const size_t bufsize,
	const enum input_type command_id,
	uint8_t *const data, const size_t data_length,
	const enum comm_type result_type)
{
	uint8_t sendbuffer[ND_ZMQ_BUFFER_SIZE];
	char sub_buf[4];
	long tmp_len = write_command(
		sendbuffer, sizeof(sendbuffer),
		command_id, data, data_length
	);

	if (tmp_len < 5) {
		fprintf(stderr, "Could not build message to uPCN\n");
		return -1;
	}
	snprintf(sub_buf, 4, "%02X ", result_type);
	while (zmq_recv(*subscriber, receivebuffer, 1, ZMQ_DONTWAIT) >= 0)
		;
	if (zmq_send(*client, sendbuffer, tmp_len, 0) < 0) {
		if (errno != EFSM ||
		    nd_connect_upcn(NULL, client, NULL, NULL, NULL) != 0 ||
		    zmq_send(*client, sendbuffer, tmp_len, 0) < 0) {
			perror("zmq_send()");
			fprintf(stderr,
				"Could not communicate with upcn_connect\n");
			return -1;
		}
	}
	if (zmq_recv(*client, receivebuffer, 1, 0) < 0) {
		perror("zmq_recv()");
		fprintf(stderr, "Could not communicate with upcn_connect\n");
		return -1;
	}
	tmp_len = -1;
	while (tmp_len < 3 || memcmp(receivebuffer, sub_buf, 3) != 0) {
		tmp_len = zmq_recv(*subscriber, receivebuffer, bufsize, 0);
		if (tmp_len < 0) {
			perror("zmq_recv()");
			if (errno == EAGAIN)
				fprintf(stderr,
					"Did not receive a response in time\n");
			else
				fprintf(stderr,
					"Could not recv from publisher\n");
			return -1;
		}
	}
	return tmp_len;
}

static long upcn_try_rrnd_cmd(
	void **const client, void **const subscriber,
	uint8_t *const receivebuffer, const size_t bufsize,
	const struct nd_command *cmd, const enum comm_type result_type)
{
	uint8_t nd_cmd_buffer[ND_ZMQ_BUFFER_SIZE];
	size_t nd_cmd_len = 1;
	long ret;
	uint32_t tmpprob;

	// Set correct type which is recognized by uPCN
	switch (cmd->type) {
	case ND_COMMAND_INIT:
		nd_cmd_buffer[0] = RRND_COMMAND_INITIALIZE_SATPOS;
		break;
	case ND_COMMAND_INFER_CONTACT:
		nd_cmd_buffer[0] = RRND_COMMAND_INFER_CONTACT;
		break;
	case ND_COMMAND_GET_GS:
		nd_cmd_buffer[0] = RRND_COMMAND_QUERY_GS;
		break;
	case ND_COMMAND_UPDATE_METRICS:
		nd_cmd_buffer[0] = RRND_COMMAND_INTEGRATE_METRICS;
		break;
	default:
		return -1;
	}
	// Write first EID (primary GS or TLE)
	ret = write_string(
		&nd_cmd_buffer[nd_cmd_len],
		sizeof(nd_cmd_buffer) - nd_cmd_len,
		cmd->type == ND_COMMAND_INIT ? cmd->tle : cmd->eid
	);
	if (ret < 0)
		return -1;
	nd_cmd_len += ret;
	// For updating metrics, write second EID and metrics
	if (cmd->type == ND_COMMAND_UPDATE_METRICS) {
		ret = write_string(
			&nd_cmd_buffer[nd_cmd_len],
			sizeof(nd_cmd_buffer) - nd_cmd_len,
			cmd->source_eid
		);
		if (ret < 0)
			return -1;
		nd_cmd_len += ret;
		if (sizeof(nd_cmd_buffer) - nd_cmd_len < 4)
			return -1;
		// NOTE: currently only reliability is supported
		tmpprob = *(uint32_t *)(&cmd->reliability);
		nd_cmd_buffer[nd_cmd_len + 0] = (tmpprob >> 24) & 0xFF;
		nd_cmd_buffer[nd_cmd_len + 1] = (tmpprob >> 16) & 0xFF;
		nd_cmd_buffer[nd_cmd_len + 2] = (tmpprob >> 8) & 0xFF;
		nd_cmd_buffer[nd_cmd_len + 3] = (tmpprob >> 0) & 0xFF;
		nd_cmd_len += 4;
	}
	return upcn_try_rpc(
		client, subscriber,
		receivebuffer, bufsize,
		INPUT_TYPE_RRND_DATA,
		nd_cmd_buffer, nd_cmd_len,
		result_type
	);
}

int upcn_is_available(
	void **const client, void **const subscriber, const int print)
{
	char receivebuffer[130];
	int length, available;
	const char *string;

	length = upcn_try_rpc(
		client, subscriber,
		(uint8_t *)receivebuffer, sizeof(receivebuffer) - 1,
		INPUT_TYPE_ECHO,
		NULL, 0,
		COMM_TYPE_ECHO
	);
	if (length <= 0)
		return 0;
	receivebuffer[length] = '\0';
	string = receivebuffer + 3;
	available = (strstr(string, "uPCN") != NULL);
	if (available && print)
		printf("Connection established 'n verified.\n*** %s\n", string);
	return available;
}

static int set_time(
	void **const client, void **const subscriber,
	const unsigned long long time_sec)
{
	uint8_t receivebuffer[20];
	unsigned char stime[4];
	size_t length;

	stime[0] = (time_sec & 0xFF000000) >> 24;
	stime[1] = (time_sec & 0x00FF0000) >> 16;
	stime[2] = (time_sec & 0x0000FF00) >> 8;
	stime[3] = (time_sec & 0x000000FF) >> 0;
	length = upcn_try_rpc(
		client, subscriber,
		(uint8_t *)receivebuffer, sizeof(receivebuffer) - 1,
		INPUT_TYPE_SET_TIME,
		stime, 4,
		COMM_TYPE_GENERIC_RESULT
	);
	if (length <= 0 || receivebuffer[3] != 1)
		return -1;
	return 0;
}

static long beacon_to_buf(
	void *const buffer, const size_t max,
	const unsigned short seq, const char *const eid, const uint16_t period,
	const uint16_t tx_bitrate, const uint16_t rx_bitrate,
	const void *const cookie, const size_t cookie_length,
	const char *const *const eids, const size_t eid_count,
	const enum rrnd_beacon_flags rrnd_flags,
	const unsigned long availability_duration)
{
	struct serialized_data serdata = serialize_new_beacon(
		seq, eid, period, tx_bitrate, rx_bitrate, cookie, cookie_length,
		eids, eid_count, rrnd_flags, availability_duration);

	if (serdata.size >= UINT16_MAX || serdata.size > max) {
		free(serdata.dataptr);
		return -1;
	}
	memcpy(buffer, serdata.dataptr, serdata.size);
	free(serdata.dataptr);
	return serdata.size;
}

#define TASK_COUNT 8
#define MAX_PERF_RECORDS 50
static int perf_index;
static unsigned long long task_us[MAX_PERF_RECORDS][4];

static int store_upcn_perf_data(const unsigned long long *const pbuf, int size)
{
	int i;

	if (size / 8 != TASK_COUNT)
		return -1;
	if (perf_index == MAX_PERF_RECORDS)
		return 0;
	task_us[perf_index][0] = 0;
	for (i = 0; i < TASK_COUNT; i++)
		task_us[perf_index][0] += pbuf[i];
	task_us[perf_index][1] = pbuf[0];
	task_us[perf_index][2] = pbuf[1];
	task_us[perf_index][3] = pbuf[4];
	perf_index++;
	printf("* Run-Perf [[total, idle, inpt, cman], ...]: [");
	for (i = 0; i < perf_index; i++)
		printf("[%llu, %llu, %llu, %llu], ",
			task_us[i][0], task_us[i][1],
			task_us[i][2], task_us[i][3]);
	printf("null]\n");
	return 0;
}

static void reset_upcn_perf_data(void)
{
	perf_index = 0;
}
