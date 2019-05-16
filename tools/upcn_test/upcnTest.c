#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>

#include <unistd.h>

#include "upcn/upcn.h"
#include "upcn/bundle.h"
#include "bundle6/serializer.h"
#include "bundle6/parser.h"
#include "cbor.h"
#include "upcn/beacon.h"
#include "upcn/beaconSerializer.h"
#include "upcn/beaconParser.h"
#include "upcn/eidList.h"
#include "upcn/eidManager.h"
#include "upcn/routerTask.h"

#include <upcnTest.h>
#include <tools.h>
#include <testlib.h>

static enum upcntest_command command = UCMD_INVALID;
static char **argv;
static int argc = -1;

void upcntest_setparam(enum upcntest_command cmd, int arg_count, char *args[])
{
	command = cmd;
	argv = args;
	argc = arg_count;
}

void upcntest_run(void)
{
	char *eid, *cla, *ep;
	bool frag_flag;
	unsigned long size, from, to, rate;
	uint8_t protocol;

	eidmanager_init();

	switch (command) {
	case UCMD_RESET:
		ASSERT(argc == 1);
		printf("Sending a RESET command and waiting 5s...\n");
		upcntest_send_resetsystem();
		sleep(5);
		printf("Checking connectivity...\n");
		upcntest_check_connectivity();
		break;
	case UCMD_CHECK:
		ASSERT(argc == 1);
		printf("Checking connectivity...\n");
		upcntest_check_connectivity();
		break;
	case UCMD_QUERY:
		ASSERT(argc == 1);
		upcntest_send_query();
		break;
	case UCMD_RQUERY:
		ASSERT(argc == 1);
		upcntest_send_router_query();
		break;
	case UCMD_RESETTIME:
		ASSERT(argc == 1);
		upcntest_send_resettime();
		break;
	case UCMD_RESETSTATS:
		ASSERT(argc == 1);
		upcntest_send_resetstats();
		break;
	case UCMD_STORETRACE:
		ASSERT(argc == 1);
		upcntest_send_storetrace();
		break;
	case UCMD_CLEARTRACES:
		ASSERT(argc == 1);
		upcntest_send_cleartraces();
		break;
	case UCMD_BUNDLE:
		ASSERT(argc <= 5);
		size = UPCNTEST_DEFAULT_BUNDLE_SIZE;
		eid = UPCNTEST_DEFAULT_BUNDLE_DESTINATION;
		frag_flag = UPCNTEST_DEFAULT_BUNDLE_FRAG_FLAG;
		protocol = UPCNTEST_DEFAULT_BUNDLE_VERSION;

		if (argc >= 2) {
			eid = argv[1];
			check_eid(eid);
			if (argc >= 3) {
				size = conv_ulong(argv[2]);
				if (argc >= 4) {
					frag_flag = conv_bool(argv[3]);
					if (argc >= 5)
						protocol = conv_ulong(argv[4]);
				}
			}
		}
		upcntest_send_payload(size, eid, frag_flag, protocol);
		break;
	case UCMD_BEACON:
		ASSERT(argc <= 2);
		eid = UPCNTEST_EID;
		if (argc >= 2) {
			eid = argv[1];
			check_eid(eid);
		}
		upcntest_send_beacon(eid);
		break;
	case UCMD_MKGS:
		ASSERT(argc >= 2 && argc <= 3);
		cla = UPCNTEST_DEFAULT_GS_CLA;
		eid = argv[1];
		check_eid(eid);
		if (argc >= 3) {
			cla = argv[2];
			check_eid(cla);
		}
		upcntest_send_mkgs(eid, cla, 1);
		break;
	case UCMD_RMGS:
		ASSERT(argc == 2);
		eid = argv[1];
		check_eid(eid);
		upcntest_send_rmgs(eid);
		break;
	case UCMD_MKCT:
		ASSERT(argc >= 3 && argc <= 5);
		eid = argv[1];
		check_eid(eid);
		rate = UPCNTEST_DEFAULT_CONTACT_RATE;
		from = conv_ulong(argv[2]);
		if (argc >= 4) {
			to = conv_ulong(argv[3]);
			ASSERT(to > from);
			if (argc >= 5) {
				rate = conv_ulong(argv[4]);
				ASSERT(rate > 0);
			}
		} else {
			to = from + UPCNTEST_DEFAULT_CONTACT_TIME;
		}
		upcntest_send_mkct(eid, from, to, rate);
		break;
	case UCMD_RMCT:
		ASSERT(argc >= 3 && argc <= 4);
		eid = argv[1];
		check_eid(eid);
		from = conv_ulong(argv[2]);
		if (argc >= 4) {
			to = conv_ulong(argv[3]);
			ASSERT(to > from);
		} else {
			to = from + UPCNTEST_DEFAULT_CONTACT_TIME;
		}
		upcntest_send_rmct(eid, from, to);
		break;
	case UCMD_MKEP:
		ASSERT(argc == 3);
		eid = argv[1];
		check_eid(eid);
		ep = argv[2];
		check_eid(ep);
		upcntest_send_mkep(eid, ep);
		break;
	case UCMD_RMEP:
		ASSERT(argc == 3);
		eid = argv[1];
		check_eid(eid);
		ep = argv[2];
		check_eid(ep);
		upcntest_send_rmep(eid, ep);
		break;
	case UCMD_CHECKUNIT:
		ASSERT(argc == 1);
		upcntest_check_unit();
		break;
	default:
		FAIL("Invalid command!");
		break;
	}
	printf("OK!\n");
}

void upcntest_init(void)
{
}

void upcntest_cleanup(void)
{
}

/* LIB */

static uint8_t const BEGIN_DELIMITER   /* 0x00 */;
static uint8_t const BEGIN_MARKER       = 0xFF;
static uint8_t const DATA_DELIMITER     = ':';
static uint8_t const END_DELIMITER      = 0xFF;
static uint8_t const COMMAND_SEPARATOR  = '\n';

void upcntest_begin_send(enum input_type type)
{
	uint8_t buf[3] = { BEGIN_DELIMITER, BEGIN_MARKER, (uint8_t)type };

	SEND(buf, 3);
}

void upcntest_send_command(uint8_t command_id, char *format, ...)
{
	static char buffer[2048];
	va_list args;
	int length;
	char *output;

	SEND(&DATA_DELIMITER, 1);
	SEND(&command_id, 1);
	va_start(args, format);
	length = vsnprintf(buffer, sizeof(buffer), format, args);
	va_end(args);
	/* If the string to be printed is longer, we have to allocate memory */
	/* and do the printing again... */
	if (length > (int)sizeof(buffer)) {
		length++; /* \0 termination */
		output = malloc(length);
		va_start(args, format);
		length = vsnprintf(output, length, format, args);
		va_end(args);
		if (length >= 0)
			SEND(output, length);
		free(output);
	} else if (length >= 0) {
		SEND(buffer, length);
	}
}

void upcntest_send_binary(uint8_t *buffer, size_t length)
{
	SEND(&DATA_DELIMITER, 1);
	SEND(buffer, length);
}

void upcntest_finish_send(void)
{
	SEND(&END_DELIMITER, 1);
	SEND(&COMMAND_SEPARATOR, 1);
}

static uint8_t *tempbuf;
static size_t written;
static void write_to_tempbuf(const void *config,
			     const void *bytes,
			     const size_t len)
{
	memcpy(tempbuf, bytes, len);
	tempbuf += len;
	written += len;
}

uint8_t *upcntest_bundle_to_bin(struct bundle *b, size_t *l, int free)
{
	size_t length;
	uint8_t *bin;

	bundle_recalculate_header_length(b);
	length = bundle_get_serialized_size(b);

	bin = malloc(length);
	tempbuf = bin;
	written = 0;
	bundle_serialize(b, &write_to_tempbuf, NULL);
	if (free)
		bundle_free(b);

	tempbuf = NULL;
	ASSERT(written == length);
	*l = length;
	return bin;
}

uint8_t *upcntest_beacon_to_bin(struct beacon *b, size_t *l, int free)
{
	uint16_t length = 0;
	uint8_t *buf = beacon_serialize(b, &length);
	uint8_t *ret = malloc(length); /* We need a test-managed malloc() */

	memcpy(ret, buf, length);
	free_unmanaged(buf);
	if (free)
		beacon_free(b);
	*l = (size_t)length;
	return ret;
}

struct bundle *upcntest_create_bundle7(
	size_t payload_size, char *dest, int fragmentable, int high_prio)
{
	static uint16_t seqnum;
	struct bundle *bundle = bundle_init();

	/* Protocol */
	bundle->protocol_version = 7;
	bundle->crc_type = BUNDLE_CRC_TYPE_NONE;

	/* EIDs */
	bundle->source      = eidmanager_alloc_ref(UPCNTEST_EID, false);
	bundle->destination = eidmanager_alloc_ref(dest, false);
	bundle->report_to   = eidmanager_alloc_ref(NULL_EID, false);

	/* Flags */
	bundle->proc_flags |= (high_prio
		? BUNDLE_FLAG_NORMAL_PRIORITY
		: BUNDLE_FLAG_EXPEDITED_PRIORITY);

	if (!fragmentable)
		bundle->proc_flags |= BUNDLE_FLAG_MUST_NOT_BE_FRAGMENTED;

	/* Times */
	bundle->creation_timestamp = dtn_timestamp();
	bundle->sequence_number = seqnum++;
	bundle->lifetime = UPCNTEST_DEFAULT_BUNDLE_LIFETIME;

	/* Payload
	 *
	 * The CBOR header is at most 9 bytes long:
	 *
	 *   - 1 byte type info
	 *   - 8 byte length info (uint64)
	 */
	uint8_t *payload = malloc_unmanaged(payload_size + 9);

	for (size_t p = 0; p < payload_size + 9; p++)
		payload[p] = (uint8_t)(rand() % 256);

	/* We have to prepend the CBOR byte string header to the payload.
	 *
	 * CBOR parser only gets 9 byte for encoding to prevent the
	 * unnecassary memcpy() into the same buffer
	 */
	CborEncoder encoder;

	cbor_encoder_init(&encoder, payload, 9, 0);
	cbor_encode_byte_string(&encoder, payload, payload_size);

	size_t extra = cbor_encoder_get_extra_bytes_needed(&encoder);
	struct bundle_block *block = bundle_block_create(
		BUNDLE_BLOCK_TYPE_PAYLOAD);

	block->crc_type = BUNDLE_CRC_TYPE_NONE;
	block->data = payload;

	/* CBOR output buffer was too small -- the needed extra bytes + 9 bytes
	 * of the output buffer are the total payload size
	 */
	if (extra)
		block->length = 9 + extra;
	else
		block->length = cbor_encoder_get_buffer_size(&encoder, payload);

	bundle->blocks = bundle_block_entry_create(block);
	bundle->payload_block = block;

	/* Done */
	return bundle;
}

struct bundle *upcntest_create_bundle6(
	size_t payload_size, char *dest, int fragmentable, int high_prio)
{
	static uint16_t seqnum;
	struct bundle *b = bundle_init();
	char *src = UPCNTEST_EID;
	size_t src_len = strlen(src);
	size_t src_ssp_index = eid_ssp_index(src);
	char *null = NULL_EID;
	size_t null_len = strlen(null);
	size_t null_ssp_index = eid_ssp_index(null);
	size_t dst_len = strlen(dest);
	size_t dst_ssp_index = eid_ssp_index(dest);
	size_t dict_len = null_len + src_len + dst_len + 3;
	char *dictionary = malloc_unmanaged(dict_len);
	uint8_t *payload = malloc_unmanaged(payload_size);

	/* Dictionary */
	memcpy(&dictionary[0], null, null_len);
	dictionary[null_ssp_index] = '\0';
	dictionary[null_len] = '\0';
	memcpy(&dictionary[null_len + 1], src, src_len);
	dictionary[null_len + 1 + src_ssp_index] = '\0';
	dictionary[null_len + 1 + src_len] = '\0';
	memcpy(&dictionary[null_len + 1 + src_len + 1], dest, dst_len);
	dictionary[null_len + 1 + src_len + 1 + dst_ssp_index] = '\0';
	dictionary[null_len + 1 + src_len + 1 + dst_len] = '\0';
	b->dict = dictionary;
	b->dict_length = dict_len;
	/* Offsets */
	b->source_eid.scheme_offset = null_len + 1;
	b->source_eid.ssp_offset = null_len + 1 + src_ssp_index + 1;
	b->destination_eid.scheme_offset = null_len + 1 + src_len + 1;
	b->destination_eid.ssp_offset
		= null_len + 1 + src_len + 1 + dst_ssp_index + 1;
	b->report_eid.scheme_offset = 0;
	b->report_eid.ssp_offset = null_ssp_index + 1;
	b->custodian_eid.scheme_offset = 0;
	b->custodian_eid.ssp_offset = null_ssp_index + 1;
	/* Flags, prio, ... */
	b->proc_flags = BUNDLE_V6_FLAG_SINGLETON_ENDPOINT;
	b->proc_flags |= (high_prio
		? BUNDLE_FLAG_NORMAL_PRIORITY
		: BUNDLE_FLAG_EXPEDITED_PRIORITY);
	if (!fragmentable)
		b->proc_flags |= BUNDLE_FLAG_MUST_NOT_BE_FRAGMENTED;
	/* Times */
	b->creation_timestamp = dtn_timestamp();
	b->sequence_number = seqnum++;
	b->lifetime = UPCNTEST_DEFAULT_BUNDLE_LIFETIME;
	/* Payload */
	for (size_t p = 0; p < payload_size; p++)
		payload[p] = (uint8_t)(rand() % 256);
	struct bundle_block *pb
		= bundle_block_create(BUNDLE_BLOCK_TYPE_PAYLOAD);
	pb->flags |= BUNDLE_BLOCK_FLAG_LAST_BLOCK;
	pb->data = payload;
	pb->length = (uint32_t)payload_size;
	b->blocks = bundle_block_entry_create(pb);
	b->payload_block = pb;
	/* Done */
	return b;
}

struct beacon *upcntest_create_beacon(char *source,
	uint16_t period, uint16_t bitrate,
	uint8_t cookie[], size_t cookie_length,
	const char **eids, size_t eid_count)
{
	static uint16_t seqnum;
	struct beacon *b = beacon_init();
	struct tlv_definition *t;
	int tmp;

	b->version = 0x04;
	b->flags = BEACON_FLAG_HAS_EID
		| BEACON_FLAG_HAS_SERVICES | BEACON_FLAG_HAS_PERIOD;
	tmp = strlen(source) + 1;
	b->eid = malloc_unmanaged(tmp);
	strncpy(b->eid, source, tmp);
	b->sequence_number = seqnum;
	b->period = period;
	b->service_count = 3;
	b->services = calloc(b->service_count, sizeof(struct tlv_definition));
	t = tlv_populate_ct(b->services, 0, TLV_TYPE_PRIVATE_TX_RX_BITRATE, 1);
	if (t != NULL) {
		t[0].tag = TLV_TYPE_FIXED32;
		t[0].value.u32 = (bitrate << 16) | bitrate;
	}
	t = tlv_populate_ct(b->services, 1, TLV_TYPE_PRIVATE_COOKIES, 1);
	if (t != NULL) {
		t[0].tag = TLV_TYPE_BYTES;
		t[0].value.b = malloc_unmanaged(cookie_length);
		if (t[0].value.b != NULL) {
			t[0].length = (uint16_t)cookie_length;
			memcpy(t[0].value.b, cookie, cookie_length);
		}
	}
	t = tlv_populate_ct(b->services, 2, TLV_TYPE_PRIVATE_NEIGHBOR_EIDS, 1);
	if (t != NULL) {
		t[0].tag = TLV_TYPE_BYTES;
		t[0].value.b = eidlist_encode(eids, eid_count, &tmp);
		if (t[0].value.b != NULL)
			t[0].length = (uint16_t)tmp;
	}
	return b;
}

void upcntest_send_resetsystem(void)
{
	upcntest_begin_send(INPUT_TYPE_RESET);
	SEND(&DATA_DELIMITER, 1);
	upcntest_finish_send();
}

void upcntest_check_connectivity(void)
{
	uint8_t *buf, ok;

	upcntest_clear_rcvbuffer();
	upcntest_begin_send(INPUT_TYPE_ECHO);
	SEND(&DATA_DELIMITER, 1);
	upcntest_finish_send();
	buf = upcntest_receive_next_info(1000);
	ASSERT(buf != NULL);
	ok = (strstr((char *)buf, "uPCN") != NULL);
	free(buf);
	ASSERT(ok);
}

void upcntest_send_query(void)
{
	upcntest_begin_send(INPUT_TYPE_DBG_QUERY);
	SEND(&DATA_DELIMITER, 1);
	upcntest_finish_send();
}

void upcntest_send_router_query(void)
{
	upcntest_begin_send(INPUT_TYPE_ROUTER_COMMAND_DATA);
	upcntest_send_command(ROUTER_COMMAND_QUERY, "(dtn:none)");
	upcntest_finish_send();
}

void upcntest_send_resettime(void)
{
	upcntest_begin_send(INPUT_TYPE_RESET_TIME);
	SEND(&DATA_DELIMITER, 1);
	upcntest_finish_send();
}

void upcntest_send_settime(uint32_t time)
{
	int i;
	uint8_t buf[1];

	upcntest_begin_send(INPUT_TYPE_SET_TIME);
	SEND(&DATA_DELIMITER, 1);
	for (i = 0; i < 4; i++) {
		/* Send MSB */
		buf[0] = (time & 0xFF000000) >> 24;
		SEND(buf, 1);
		time <<= 8;
	}
	upcntest_finish_send();
}

void upcntest_send_resetstats(void)
{
	upcntest_begin_send(INPUT_TYPE_RESET_STATS);
	SEND(&DATA_DELIMITER, 1);
	upcntest_finish_send();
}

void upcntest_send_storetrace(void)
{
	upcntest_begin_send(INPUT_TYPE_STORE_TRACE);
	SEND(&DATA_DELIMITER, 1);
	upcntest_finish_send();
}

void upcntest_send_cleartraces(void)
{
	upcntest_begin_send(INPUT_TYPE_CLEAR_TRACES);
	SEND(&DATA_DELIMITER, 1);
	upcntest_finish_send();
}

void upcntest_send_bundle(struct bundle *b)
{
	size_t s = 0;
	uint8_t *bin = upcntest_bundle_to_bin(b, &s, 0);

	upcntest_begin_send(INPUT_TYPE_BUNDLE_VERSION);
	upcntest_send_binary(bin, s);
}

void upcntest_send_payload(size_t payload_size, char *dest, bool fragmentable,
	uint8_t protocol)
{
	struct bundle *b = NULL;

	if (protocol == 6)
		b = upcntest_create_bundle6(
			payload_size, dest, fragmentable, 0);
	else if (protocol == 7)
		b = upcntest_create_bundle7(
			payload_size, dest, fragmentable, 0);
	else
		FAIL("Unsupported protocol version");

	upcntest_send_bundle(b);
	bundle_free(b);
}

void upcntest_send_beacon(char *source)
{
	static uint8_t cookie[] = UPCNTEST_DEFAULT_BEACON_COOKIE;
	struct beacon *b
		= upcntest_create_beacon(source, UPCNTEST_DEFAULT_BEACON_PERIOD,
			UPCNTEST_DEFAULT_CONTACT_RATE, cookie, sizeof(cookie),
			NULL, 0);
	size_t s = 0;
	uint8_t *bin = upcntest_beacon_to_bin(b, &s, 1); /* This frees b */

	upcntest_begin_send(INPUT_TYPE_BEACON_DATA);
	upcntest_send_binary(bin, s);
	upcntest_finish_send();
}

void upcntest_send_mkgs(char *eid, char *cla, float reliability)
{
	uint16_t r = reliability * 1000;

	ASSERT(r >= 100 && r <= 1000);
	upcntest_begin_send(INPUT_TYPE_ROUTER_COMMAND_DATA);
	upcntest_send_command(ROUTER_COMMAND_ADD, "(%s),%d:(%s)", eid, r, cla);
	upcntest_finish_send();
}

void upcntest_send_rmgs(char *eid)
{
	upcntest_begin_send(INPUT_TYPE_ROUTER_COMMAND_DATA);
	upcntest_send_command(ROUTER_COMMAND_DELETE, "(%s)", eid);
	upcntest_finish_send();
}

void upcntest_send_mkct(char *gs, uint32_t from, uint32_t to, uint32_t rate)
{
	printf("created contact from %u to %u, d: %d\n", from, to, (to-from));
	upcntest_begin_send(INPUT_TYPE_ROUTER_COMMAND_DATA);
	upcntest_send_command(ROUTER_COMMAND_ADD, "(%s):::[{%d,%d,%d}]",
			      gs, from, to, rate);
	upcntest_finish_send();
}

void upcntest_send_rmct(char *gs, uint32_t from, uint32_t to)
{
	upcntest_begin_send(INPUT_TYPE_ROUTER_COMMAND_DATA);
	upcntest_send_command(ROUTER_COMMAND_DELETE, "(%s):::[{%d,%d,%d}]",
			      gs, from, to, 100);
	upcntest_finish_send();
}

void upcntest_send_mkep(char *gs, char *ep)
{
	upcntest_begin_send(INPUT_TYPE_ROUTER_COMMAND_DATA);
	upcntest_send_command(ROUTER_COMMAND_ADD, "(%s)::[(%s)]", gs, ep);
	upcntest_finish_send();
}

void upcntest_send_rmep(char *gs, char *ep)
{
	upcntest_begin_send(INPUT_TYPE_ROUTER_COMMAND_DATA);
	upcntest_send_command(ROUTER_COMMAND_DELETE, "(%s)::[(%s)]", gs, ep);
	upcntest_finish_send();
}

void upcntest_check_unit(void)
{
	uint8_t *buf, ok;

	upcntest_clear_rcvbuffer();
	upcntest_begin_send(INPUT_TYPE_ECHO);
	SEND(&DATA_DELIMITER, 1);
	upcntest_finish_send();
	buf = upcntest_receive_next_info(1000);
	ASSERT(buf != NULL);
	ok = (strstr((char *)buf, "uPCN") != NULL);
	free(buf);
	ASSERT(ok);
	upcntest_clear_rcvbuffer();
	upcntest_begin_send(INPUT_TYPE_DBG_QUERY);
	SEND(&DATA_DELIMITER, 1);
	upcntest_finish_send();
	buf = upcntest_receive_next_info(1000);
	ASSERT(buf != NULL);
	ok = (strstr((char *)buf, "unittests succeeded") != NULL);
	free(buf);
	ASSERT(ok);
}

enum utt_type {
	UTT_BUNDLE = 1,
	UTT_BEACON = 2,
	UTT_UNKNOWN = 255
};

static uint8_t *upcntest_receive_next_data(
	size_t *sz, int *type, uint32_t ms_timeout)
{
	static uint8_t buffer[1024 * 1024], *rbuf;

	*type = UTT_UNKNOWN;
	*sz = RECEIVE(buffer, 1024 * 1024);
	if (*sz <= 3)
		return NULL;
	*sz -= 3;
	if (memcmp(buffer, "02 ", 3) == 0)
		*type = UTT_BUNDLE;
	else if (memcmp(buffer, "03 ", 3) == 0)
		*type = UTT_BEACON;
	else
		*type = UTT_UNKNOWN;
	rbuf = malloc(*sz + 1);
	rbuf[*sz] = '\0';
	memcpy(rbuf, buffer + 3, *sz);
	return rbuf;
}

static inline int tdiff(struct timespec a, struct timespec b)
{
	return (a.tv_sec - b.tv_sec) * 1000 + (a.tv_nsec - b.tv_nsec) / 1000000;
}

static struct bundle *bdl;

static void bundle_send(struct bundle *bundle)
{
	if (bdl != NULL)
		bundle_free(bdl);
	bdl = bundle;
}

void upcntest_clear_rcvbuffer(void)
{
	uint8_t buf[1];

	while (RECEIVE_NOBLOCK(buf, 1) > 0)
		;
}

uint8_t *upcntest_receive_next_info(uint32_t ms_timeout)
{
	uint8_t *buf = NULL;
	size_t size;
	int type = 0;
	static struct timespec cb, cc;

	clock_gettime(CLOCK_REALTIME, &cb);
	clock_gettime(CLOCK_REALTIME, &cc);
	while (type != (int)UTT_UNKNOWN && tdiff(cc, cb) < (int)ms_timeout) {
		if (buf != NULL)
			free(buf);
		buf = upcntest_receive_next_data(&size, &type, ms_timeout);
		clock_gettime(CLOCK_REALTIME, &cc);
	}

	if (type != (int)UTT_UNKNOWN) {
		if (buf != NULL)
			free(buf);
		return NULL;
	}

	return buf;
}

struct bundle *upcntest_receive_next_bundle(uint32_t ms_timeout)
{
	uint8_t *buf = NULL;
	size_t size;
	int type = 0;
	static struct timespec cb, cc;
	struct bundle6_parser p;
	struct bundle *res;

	clock_gettime(CLOCK_REALTIME, &cb);
	clock_gettime(CLOCK_REALTIME, &cc);
	while (type != (int)UTT_BUNDLE && tdiff(cc, cb) < (int)ms_timeout) {
		if (buf != NULL)
			free(buf);
		buf = upcntest_receive_next_data(&size, &type, ms_timeout);
		clock_gettime(CLOCK_REALTIME, &cc);
	}

	if (type != (int)UTT_BUNDLE) {
		if (buf != NULL)
			free(buf);
		return NULL;
	}

	if (bdl != NULL) {
		bundle_free(bdl);
		bdl = NULL;
	}
	bundle6_parser_init(&p, &bundle_send);
	bundle6_parser_read(&p, buf, size);
	free_unmanaged(p.basedata);
	if (p.bundle != NULL)
		bundle_free(p.bundle);
	res = bdl;
	bdl = NULL;
	return res;
}

static struct beacon *beac;

static void beac_send(struct beacon *b)
{
	if (beac != NULL)
		beacon_free(beac);
	beac = b;
}

struct beacon *upcntest_receive_next_beacon(uint32_t ms_timeout)
{
	uint8_t *buf = NULL;
	size_t size;
	int type = 0;
	static struct timespec cb, cc;
	struct beacon_parser p;
	struct beacon *res;

	clock_gettime(CLOCK_REALTIME, &cb);
	clock_gettime(CLOCK_REALTIME, &cc);
	while (type != (int)UTT_BEACON && tdiff(cc, cb) < (int)ms_timeout) {
		if (buf != NULL)
			free(buf);
		buf = upcntest_receive_next_data(&size, &type, ms_timeout);
		clock_gettime(CLOCK_REALTIME, &cc);
	}

	if (type != (int)UTT_BEACON) {
		if (buf != NULL)
			free(buf);
		return NULL;
	}

	if (beac != NULL) {
		beacon_free(beac);
		beac = NULL;
	}
	beacon_parser_init(&p, &beac_send);
	beacon_parser_read(&p, buf, size);
	free_unmanaged(p.basedata);
	if (p.beacon != NULL)
		beacon_free(p.beacon);
	res = beac;
	beac = NULL;
	return res;
}
