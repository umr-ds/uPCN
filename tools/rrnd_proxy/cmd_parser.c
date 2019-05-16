#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <limits.h>
#include <string.h>
#include <math.h>

#include "drv/jsmn.h"
#include "drv/json_unescape.h"

#include "common.h"

#define BEACON_ARRAY_LENGTH 7

static int json_is_equal(
	const char *const json, const jsmntok_t *const tok, const char *s);
static long long json_get_long(
	const char *const json, const jsmntok_t *const tok);
static float json_get_float(
	const char *const json, const jsmntok_t *const tok);
static int json_populate_string(
	const char *const json, const jsmntok_t *const tok,
	char *const target);
static int json_populate_beacon(
	const char *json, const jsmntok_t *const tok,
	struct rrnd_beacon *const beacon,
	char *const *const eids, size_t *const eid_count);

char *nd_cmd_parse(
	const char *const buf, const size_t length,
	struct nd_command *const cmd)
{
	static jsmn_parser parser;
	static jsmntok_t tokens[256];

	char *err = NULL;
	int i, result;

	jsmn_init(&parser);
	result = jsmn_parse(
		&parser, buf, length,
		tokens, ARRAY_SIZE(tokens) - 1);
	if (result < 0)
		err = "Command could not be parsed";
	else if (result < 1 || tokens[0].type != JSMN_OBJECT)
		err = "No top-level object found";
	if (err != NULL)
		return err;
	for (i = 1; i < result; i++) {
		if (i + 1 >= result) {
			err = "No argument identified in JSON";
		} else if (json_is_equal(buf, &tokens[i], "command")) {
			i++;
			if (json_is_equal(buf, &tokens[i], "process"))
				cmd->type = ND_COMMAND_PROCESS_BEACON;
			else if (json_is_equal(buf, &tokens[i], "next_contact"))
				cmd->type = ND_COMMAND_INFER_CONTACT;
			else if (json_is_equal(buf, &tokens[i], "get_gs"))
				cmd->type = ND_COMMAND_GET_GS;
			else if (json_is_equal(buf, &tokens[i], "test"))
				cmd->type = ND_COMMAND_TEST_CONNECTIVITY;
			else if (json_is_equal(buf, &tokens[i], "init"))
				cmd->type = ND_COMMAND_INIT;
			else if (json_is_equal(buf, &tokens[i], "reset"))
				cmd->type = ND_COMMAND_RESET;
			else if (json_is_equal(buf, &tokens[i], "reset_perf"))
				cmd->type = ND_COMMAND_RESET_PERF;
			else if (json_is_equal(buf, &tokens[i], "store_perf"))
				cmd->type = ND_COMMAND_STORE_PERF;
			else if (json_is_equal(buf, &tokens[i], "update_prob"))
				cmd->type = ND_COMMAND_UPDATE_METRICS;
			else
				err = "Invalid token value";
		} else if (json_is_equal(buf, &tokens[i], "time")) {
			i++;
			cmd->time = json_get_long(buf, &tokens[i]);
			if (cmd->time == LLONG_MAX)
				err = "Could not convert time";
		} else if (json_is_equal(buf, &tokens[i], "eid")) {
			i++;
			if (json_populate_string(buf, &tokens[i], cmd->eid))
				err = "EID string too long";
		} else if (json_is_equal(buf, &tokens[i], "tle")) {
			i++;
			if (json_populate_string(buf, &tokens[i], cmd->tle))
				err = "TLE string too long";
		} else if (json_is_equal(buf, &tokens[i], "source_eid")) {
			i++;
			if (json_populate_string(buf, &tokens[i],
						 cmd->source_eid))
				err = "EID string too long";
		} else if (json_is_equal(buf, &tokens[i], "reliability")) {
			i++;
			cmd->reliability = json_get_float(buf, &tokens[i]);
			if (isnan(cmd->reliability))
				err = "Could not convert reliability";
		} else if (json_is_equal(buf, &tokens[i], "beacon") &&
			tokens[i + 1].type == JSMN_ARRAY
		) {
			i++;
			if (tokens[i].size != BEACON_ARRAY_LENGTH
					|| (i + BEACON_ARRAY_LENGTH) >= result)
				err = "Beacon list dimensions wrong";
			else if (json_populate_beacon(
					buf, &tokens[i], &cmd->beacon,
					(char **)cmd->reported_eids,
					&cmd->reported_eid_count)
						!= 0)
				err = "Could not populate beacon";
			i += BEACON_ARRAY_LENGTH;
		} else {
			err = "Unexpected token";
		}
		if (err != NULL)
			return err;
	}
	return NULL;
}

static int json_is_equal(
	const char *const json, const jsmntok_t *const tok, const char *s)
{
	return (
		tok->type == JSMN_STRING &&
		(int)strlen(s) == (tok->end - tok->start) &&
		memcmp(json + tok->start, s, tok->end - tok->start) == 0
	);
}

static long long json_get_long(
	const char *const json, const jsmntok_t *const tok)
{
	static char buf[16];
	static char *end;
	long long ret;

	snprintf(buf, ARRAY_SIZE(buf), "%.*s",
		 tok->end - tok->start,
		 json + tok->start);
	ret = strtoll(buf, &end, 10);
	if (*end != '\0')
		ret = LLONG_MAX;
	return ret;
}

static float json_get_float(
	const char *const json, const jsmntok_t *const tok)
{
	static char buf[16];
	static char *end;
	double result;

	snprintf(buf, ARRAY_SIZE(buf), "%.*s",
		 tok->end - tok->start,
		 json + tok->start);
	result = strtod(buf, &end);
	if (*end != '\0')
		result = NAN;
	return result;
}

static int json_populate_string(
	const char *const json, const jsmntok_t *const tok,
	char *const target)
{
	int tmplen = tok->end - tok->start;

	if (tmplen > ND_COMMAND_STR_SIZE - 1)
		return -1;
	memcpy(target, json + tok->start, tmplen);
	target[tmplen] = '\0';
	json_unescape_string(target);
	return 0;
}

static int json_get_eids(
	const char *const json, const jsmntok_t *const tok,
	char *const *const eids, size_t *const eid_count)
{
	int i, len;
	const jsmntok_t *t;

	if (tok->type != JSMN_ARRAY || tok->size > ND_LOCAL_MAX_EID_COUNT)
		return -1;
	t = tok + 1;
	for (i = 0; i < tok->size; i++) {
		len = t[i].end - t[i].start;
		if (t[i].type != JSMN_STRING || len > ND_LOCAL_MAX_EID_LENGTH)
			return -1;
		memcpy(eids[i], json + t[i].start, len);
		eids[i][len] = '\0';
		json_unescape_string(eids[i]);
	}
	*eid_count = tok->size;
	return 0;
}

static int json_populate_beacon(
	const char *json, const jsmntok_t *const tok,
	struct rrnd_beacon *const beacon,
	char *const *const eids, size_t *const eid_count)
{
	const int c = 1; /* Start at first token after array token */
	long long flags, tmp;

	beacon->sequence_number = tmp = json_get_long(json, &tok[c]);
	if (tmp == LLONG_MAX)
		return -1;
	beacon->period = tmp = json_get_long(json, &tok[c + 1]);
	if (tmp == LLONG_MAX)
		return -1;
	beacon->tx_bitrate = tmp = json_get_long(json, &tok[c + 2]);
	if (tmp == LLONG_MAX)
		return -1;
	beacon->rx_bitrate = tmp = json_get_long(json, &tok[c + 3]);
	if (tmp == LLONG_MAX)
		return -1;
	flags = json_get_long(json, &tok[c + 4]);
	if (flags == LLONG_MAX)
		return -1;
	beacon->availability_duration = tmp = json_get_long(json, &tok[c + 5]);
	if (tmp == LLONG_MAX)
		return -1;
	beacon->flags = (enum rrnd_beacon_flags)flags;
	if (json_get_eids(json, &tok[c + 6], eids, eid_count) != 0)
		return -1;
	return 0;
}
