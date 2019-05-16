#ifndef UPCNTEST_H_INCLUDED
#define UPCNTEST_H_INCLUDED

#include <stdint.h>
#include <stddef.h>

#include "upcn/inputParser.h"
#include "upcn/bundle.h"
#include "upcn/beacon.h"

/* Command "test" implementation */

enum upcntest_command {
	UCMD_INVALID,
	UCMD_CHECK,
	UCMD_RESET,
	UCMD_QUERY,
	UCMD_RQUERY,
	UCMD_RESETTIME,
	UCMD_RESETSTATS,
	UCMD_STORETRACE,
	UCMD_CLEARTRACES,
	UCMD_BUNDLE,
	UCMD_BEACON,
	UCMD_MKGS,
	UCMD_RMGS,
	UCMD_MKCT,
	UCMD_RMCT,
	UCMD_MKEP,
	UCMD_RMEP,
	UCMD_CHECKUNIT
};

void upcntest_setparam(enum upcntest_command cmd, int arg_count, char *args[]);

/* Test function defs */
void upcntest_run(void);
void upcntest_init(void);
void upcntest_cleanup(void);

/* uPCN interaction */

#define UPCNTEST_EID "dtn:upcntest"
#define NULL_EID "dtn:none"

#define UPCNTEST_DEFAULT_BUNDLE_SIZE 500
#define UPCNTEST_DEFAULT_BUNDLE_DESTINATION "dtn:somedest"
#define UPCNTEST_DEFAULT_BUNDLE_FRAG_FLAG 1
#define UPCNTEST_DEFAULT_BUNDLE_VERSION 6
#define UPCNTEST_DEFAULT_BUNDLE_LIFETIME 86400

#define UPCNTEST_DEFAULT_BEACON_PERIOD 10
#define UPCNTEST_DEFAULT_BEACON_COOKIE { }

#define UPCNTEST_DEFAULT_GS_CLA "ABCDE"

#define UPCNTEST_DEFAULT_CONTACT_TIME 2
#define UPCNTEST_DEFAULT_CONTACT_RATE 1200

void upcntest_begin_send(enum input_type type);
void upcntest_send_command(uint8_t command_id, char *format, ...);
void upcntest_send_binary(uint8_t *buffer, size_t length);
void upcntest_finish_send(void);

uint8_t *upcntest_bundle_to_bin(struct bundle *b, size_t *l, int free);
uint8_t *upcntest_beacon_to_bin(struct beacon *b, size_t *l, int free);

struct bundle *upcntest_create_bundle6(
	size_t payload_size, char *dest, int fragmentable, int high_prio);

struct bundle *upcntest_create_bundle7(
	size_t payload_size, char *dest, int fragmentable, int high_prio);

struct beacon *upcntest_create_beacon(char *source,
	uint16_t period, uint16_t bitrate,
	uint8_t cookie[], size_t cookie_length,
	const char **eids, size_t eid_count);

void upcntest_send_resetsystem(void);
void upcntest_check_connectivity(void);
void upcntest_send_query(void);
void upcntest_send_router_query(void);
void upcntest_send_resettime(void);
void upcntest_send_settime(uint32_t time);
void upcntest_send_resetstats(void);
void upcntest_send_storetrace(void);
void upcntest_send_cleartraces(void);
void upcntest_send_bundle(struct bundle *b);
void upcntest_send_payload(size_t payload_size, char *dest, bool fragmentable,
	uint8_t protocol);
void upcntest_send_beacon(char *source);
void upcntest_send_mkgs(char *eid, char *cla, float reliability);
void upcntest_send_rmgs(char *eid);
void upcntest_send_mkct(char *gs, uint32_t from, uint32_t to, uint32_t rate);
void upcntest_send_rmct(char *gs, uint32_t from, uint32_t to);
void upcntest_send_mkep(char *gs, char *ep);
void upcntest_send_rmep(char *gs, char *ep);
void upcntest_check_unit(void);

void upcntest_clear_rcvbuffer(void);

uint8_t *upcntest_receive_next_info(uint32_t ms_timeout);
struct bundle *upcntest_receive_next_bundle(uint32_t ms_timeout);
struct beacon *upcntest_receive_next_beacon(uint32_t ms_timeout);

#endif /* UPCNTEST_H_INCLUDED */
