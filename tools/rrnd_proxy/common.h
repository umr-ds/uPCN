#ifndef COMMON_H_INCLUDED
#define COMMON_H_INCLUDED

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "upcn/rrnd.h"

#define ND_ZMQ_TIMEOUT 1000
#define ND_ZMQ_LINGER_TIMEOUT 100
#define ND_ZMQ_BUFFER_SIZE 2048

#define ND_LOCAL_MAX_GS 100
#define ND_LOCAL_MAX_EID_LENGTH 192
#define ND_LOCAL_MAX_EID_COUNT 10

#define ND_COMMAND_STR_SIZE (ND_LOCAL_MAX_EID_LENGTH)
#define ND_GS_JSON_STR_SIZE (ND_LOCAL_MAX_EID_LENGTH + 2048)

/* Does NOT reject pointers! */
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

enum nd_command_type {
#define X(x) x
#include "nd_commands.def"
#undef X
};

struct nd_command {
	enum nd_command_type type;
	long long time;
	char eid[ND_COMMAND_STR_SIZE + 1];
	char tle[ND_COMMAND_STR_SIZE + 1];
	char source_eid[ND_COMMAND_STR_SIZE + 1];
	float reliability;
	struct rrnd_beacon beacon;
	char reported_eids[ND_LOCAL_MAX_EID_COUNT][ND_LOCAL_MAX_EID_LENGTH + 1];
	size_t reported_eid_count;
};

struct nd_result {
	enum rrnd_status rc;
	char gs_data_json[ND_GS_JSON_STR_SIZE];
};

char *nd_cmd_parse(
	const char *const buffer, const size_t length,
	struct nd_command *const cmd);

void nd_cmd_process_locally(
	const struct nd_command *const cmd, struct nd_result *const res,
	const bool verbose);

int nd_connect_upcn(
	void *ctx, void **client, void **subscriber,
	const char *ucrep_addr, const char *ucpub_addr);
void nd_cmd_process_upcn(
	const struct nd_command *const cmd, struct nd_result *const res,
	void **const client, void **const info_subscriber, const bool verbose);
int upcn_is_available(
	void **const client, void **const subscriber, const int print);

#endif /* COMMON_H_INCLUDED */
