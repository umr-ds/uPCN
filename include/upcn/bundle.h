#ifndef BUNDLE_H_INCLUDED
#define BUNDLE_H_INCLUDED

#include <stdint.h>   // uint*_t
#include <stddef.h>   // size_t
#include <stdbool.h>  // bool

#ifndef HAS_FLAG
#define HAS_FLAG(value, flag) ((value & flag) != 0)
#endif

#define BUNDLE_INVALID_ID 0
typedef uint16_t bundleid_t;

/**
 * References a Bundle Protocol endpoint ID by two offsets into the dictionary
 * of the containing bundle
 */
struct eid_reference {
	uint16_t scheme_offset;
	uint16_t ssp_offset;
};


enum bundle_proc_flags {
	BUNDLE_FLAG_NONE = 0x0000,

	// Generic Bundle Processing Flags
	//
	// Contains the union of RFC 5050 and BPv7 bundle processing flags.
	// For backward compatibility the generic flags are identical to the
	// RFC 5050 / bundle6 flags. Flags that are specified in BPv7 but
	// do not exist in RFC 5050 are appended at bit position 21.
	//
	// General
	BUNDLE_FLAG_IS_FRAGMENT                  = 0x000001,
	BUNDLE_FLAG_ADMINISTRATIVE_RECORD        = 0x000002,
	BUNDLE_FLAG_MUST_NOT_BE_FRAGMENTED       = 0x000004,
	BUNDLE_FLAG_CUSTODY_TRANSFER_REQUESTED   = 0x000008,
	BUNDLE_FLAG_ACKNOWLEDGEMENT_REQUESTED    = 0x000020,

	// RFC 5050 Priority
	BUNDLE_FLAG_NORMAL_PRIORITY              = 0x000080,
	BUNDLE_FLAG_EXPEDITED_PRIORITY           = 0x000100,

	// RFC 5050 and BPv7 Reports
	BUNDLE_FLAG_REPORT_RECEPTION             = 0x004000,
	// This flag is called "report custody acceptance" in RFC 5050
	BUNDLE_FLAG_REPORT_CUSTODY_PROCESSING    = 0x008000,
	BUNDLE_FLAG_REPORT_FORWARDING            = 0x010000,
	BUNDLE_FLAG_REPORT_DELIVERY              = 0x020000,
	BUNDLE_FLAG_REPORT_DELETION              = 0x040000,

	// BPv7 Reports
	BUNDLE_FLAG_REPORT_STATUS_TIME           = 0x200000,
	BUNDLE_FLAG_CONTAINS_MANIFEST            = 0x400000,


	// Bundle Protocol v6 (RFC 5050)
	//
	// General
	BUNDLE_V6_FLAG_IS_FRAGMENT                = 0x0001,
	BUNDLE_V6_FLAG_ADMINISTRATIVE_RECORD      = 0x0002,
	BUNDLE_V6_FLAG_MUST_NOT_BE_FRAGMENTED     = 0x0004,
	BUNDLE_V6_FLAG_CUSTODY_TRANSFER_REQUESTED = 0x0008,
	BUNDLE_V6_FLAG_SINGLETON_ENDPOINT         = 0x0010,
	BUNDLE_V6_FLAG_ACKNOWLEDGEMENT_REQUESTED  = 0x0020,

	// Priority
	BUNDLE_V6_FLAG_NORMAL_PRIORITY            = 0x0080,
	BUNDLE_V6_FLAG_EXPEDITED_PRIORITY         = 0x0100,

	// Reports
	BUNDLE_V6_FLAG_REPORT_RECEPTION          = 0x04000,
	BUNDLE_V6_FLAG_REPORT_CUSTODY_ACCEPTANCE = 0x08000,
	BUNDLE_V6_FLAG_REPORT_FORWARDING         = 0x10000,
	BUNDLE_V6_FLAG_REPORT_DELIVERY           = 0x20000,
	BUNDLE_V6_FLAG_REPORT_DELETION           = 0x40000,


	// Bundle Protocol v7 (section 4.1.3)
	//
	// General
	BUNDLE_V7_FLAG_IS_FRAGMENT                = 0x0001,
	BUNDLE_V7_FLAG_ADMINISTRATIVE_RECORD      = 0x0002,
	BUNDLE_V7_FLAG_MUST_NOT_BE_FRAGMENTED     = 0x0004,
	BUNDLE_V7_FLAG_ACKNOWLEDGEMENT_REQUESTED  = 0x0020,

	// Reports
	BUNDLE_V7_FLAG_REPORT_STATUS_TIME         = 0x0040,
	BUNDLE_V7_FLAG_CONTAINS_MANIFEST          = 0x0080,
	BUNDLE_V7_FLAG_REPORT_RECEPTION           = 0x0100,
	BUNDLE_V7_FLAG_REPORT_FORWARDING          = 0x0400,
	BUNDLE_V7_FLAG_REPORT_DELIVERY            = 0x0800,
	BUNDLE_V7_FLAG_REPORT_DELETION            = 0x1000,

};

enum bundle_crc_type {
	BUNDLE_CRC_TYPE_NONE = 0,
	BUNDLE_CRC_TYPE_16   = 1,
	BUNDLE_CRC_TYPE_32   = 2
};


enum bundle_retention_constraints {
	BUNDLE_RET_CONSTRAINT_NONE               = 0x00,
	BUNDLE_RET_CONSTRAINT_DISPATCH_PENDING   = 0x01,
	BUNDLE_RET_CONSTRAINT_FORWARD_PENDING    = 0x02,
	BUNDLE_RET_CONSTRAINT_CUSTODY_ACCEPTED   = 0x04,
	BUNDLE_RET_CONSTRAINT_REASSEMBLY_PENDING = 0x08,
	BUNDLE_RET_CONSTRAINT_FLAG_OWN           = 0x80
};

enum bundle_block_type {
	BUNDLE_BLOCK_TYPE_PAYLOAD           = 1,
	BUNDLE_BLOCK_TYPE_PREVIOUS_NODE     = 7,
	BUNDLE_BLOCK_TYPE_BUNDLE_AGE        = 8,
	BUNDLE_BLOCK_TYPE_HOP_COUNT         = 9,
	BUNDLE_BLOCK_TYPE_MAX               = 255
};

enum bundle_block_flags {
	// Bundle Protocol v6 (RFC 5050)
	BUNDLE_BLOCK_FLAG_NONE                    = 0x00,
	BUNDLE_BLOCK_FLAG_MUST_BE_REPLICATED      = 0x01,
	BUNDLE_BLOCK_FLAG_REPORT_IF_UNPROC        = 0x02,
	BUNDLE_BLOCK_FLAG_DELETE_BUNDLE_IF_UNPROC = 0x04,
	BUNDLE_BLOCK_FLAG_LAST_BLOCK              = 0x08,
	BUNDLE_BLOCK_FLAG_DISCARD_IF_UNPROC       = 0x10,
	BUNDLE_BLOCK_FLAG_FWD_UNPROC              = 0x20,
	BUNDLE_BLOCK_FLAG_HAS_EID_REF_FIELD       = 0x40,

	// Bundle Protocol v7 (section 4.1.4)
	//
	// block must be replicated in every fragment
	BUNDLE_V7_BLOCK_FLAG_MUST_BE_REPLICATED       = 0x01,
	// block must be removed from bundle if it can't be processed
	BUNDLE_V7_BLOCK_FLAG_DISCARD_IF_UNPROC        = 0x02,
	// status report must be transmitted if block can't be processed
	BUNDLE_V7_BLOCK_FLAG_REPORT_IF_UNPROC         = 0x04,
	// bundle must be deleted if block can't be processed
	BUNDLE_V7_BLOCK_FLAG_DELETE_BUNDLE_IF_UNPROC  = 0x08
};


enum bundle_eid_schema {
	BUNDLE_V7_EID_SCHEMA_DTN = 1,
	BUNDLE_V7_EID_SCHEMA_IPN = 2
};

/**
 * CRC checksum
 *
 * A union type for simpler access to the underlying CRC bytes but also to
 * have the possibility to interpret the checksum as number.
 */
union crc {
	uint8_t bytes[4];
	uint32_t checksum;
};


struct bundle_block {
	enum bundle_block_type type;
	uint8_t number;
	enum bundle_block_flags flags;
	enum bundle_crc_type crc_type;

	uint16_t eid_ref_count;
	struct eid_reference *eid_refs;
	uint32_t length;
	uint8_t *data;
	union crc crc;
};

struct bundle_hop_count {
	uint16_t limit;
	uint16_t count;
};

struct bundle_block_list {
	struct bundle_block *data;
	struct bundle_block_list *next;
};

struct bundle {
	bundleid_t id;

	uint8_t protocol_version;

	enum bundle_proc_flags proc_flags;
	enum bundle_retention_constraints ret_constraints;
	enum bundle_crc_type crc_type;

	char *destination;
	char *source;
	char *report_to;

	// RFC 5050 dictionary references
	//
	// Attention:
	//     Please assert that these references, the dictionary and the EID
	//     strings above are always in sync.
	//
	struct eid_reference destination_eid;
	struct eid_reference source_eid;
	struct eid_reference report_eid;
	struct eid_reference custodian_eid;

	uint64_t creation_timestamp;
	uint16_t sequence_number;
	uint64_t lifetime;
	union crc crc;

	uint16_t dict_length;
	char *dict;

	uint32_t fragment_offset;
	uint32_t total_adu_length; // NOTE: not used in BPbis v7.

	/**
	 * RFC 5050
	 * --------
	 *
	 * Aggregate length of the following serialized primary block fields:
	 *
	 *  - Version
	 *  - Bundle Processing Control Flags
	 *  - Block Length
	 *
	 * The sum of "primary_prefix_length" is "primary_block_length" is the
	 * total length of the primary block
	 *
	 *
	 * BPv7-bis
	 * --------
	 *
	 * The field is not used for length calculation and is always zero.
	 */
	uint16_t primary_prefix_length;

	/**
	 * RFC 5050
	 * --------
	 *
	 * Aggregate length of the serialized primary block fields excluding:
	 *
	 *  - Version
	 *  - Bundle Processing Control Flags
	 *  - Block Length
	 *
	 * The sum of "primary_prefix_length" is "primary_block_length" is the
	 * total length of the primary block.
	 *
	 *
	 * BPv7-bis
	 * --------
	 *
	 * The field contains the CBOR byte length of the primary block.
	 */
	uint16_t primary_block_length;

	struct bundle_block_list *blocks;
	struct bundle_block *payload_block;
};

enum bundle_administrative_record_type {
	BUNDLE_AR_STATUS_REPORT  = 1,
	BUNDLE_AR_CUSTODY_SIGNAL = 2
};

enum bundle_administrative_record_flags {
	BUNDLE_AR_FLAG_FRAGMENT = 0x1
};

enum bundle_status_report_status_flags {
	BUNDLE_SR_FLAG_BUNDLE_RECEIVED  = 0x01,
	BUNDLE_SR_FLAG_CUSTODY_TRANSFER = 0x02,
	BUNDLE_SR_FLAG_BUNDLE_FORWARDED = 0x04,
	BUNDLE_SR_FLAG_BUNDLE_DELIVERED = 0x08,
	BUNDLE_SR_FLAG_BUNDLE_DELETED   = 0x10
};

enum bundle_status_report_reason {
	BUNDLE_SR_REASON_NO_INFO                  = 0,
	BUNDLE_SR_REASON_LIFETIME_EXPIRED         = 1,
	BUNDLE_SR_REASON_FORWARDED_UNIDIRECTIONAL = 2,
	BUNDLE_SR_REASON_TRANSMISSION_CANCELED    = 3,
	BUNDLE_SR_REASON_DEPLETED_STORAGE         = 4,
	BUNDLE_SR_REASON_DEST_EID_UNINTELLIGIBLE  = 5,
	BUNDLE_SR_REASON_NO_KNOWN_ROUTE           = 6,
	BUNDLE_SR_REASON_NO_TIMELY_CONTACT        = 7,
	BUNDLE_SR_REASON_BLOCK_UNINTELLIGIBLE     = 8
};

struct bundle_status_report {
	enum bundle_status_report_status_flags status;

	// Report status time (BPv7-only)
	//
	// If the "Report status time" bundle processing flag is set and the
	// status indicator (flag) is also set, these fields indicate the time
	// (as reported by the local system clock) at which the indicated
	// status was asserted for this bundle, represented as a DTN time.
	uint64_t bundle_received_time;
	uint64_t bundle_forwarded_time;
	uint64_t bundle_delivered_time;
	uint64_t bundle_deleted_time;

	enum bundle_status_report_reason reason;

	// BPv7-bis:
	//     All other remaining fields are specified in the "generic"
	//     "struct bundle_administrative_record" because these fields are
	//     common for Bundle Status Reports and Custody Signals.
};

enum bundle_custody_signal_reason {
	BUNDLE_CS_REASON_NO_INFO                 = 0,
	BUNDLE_CS_REASON_REDUNDANT_RECEPTION     = 3,
	BUNDLE_CS_REASON_DEPLETED_STORAGE        = 4,
	BUNDLE_CS_REASON_DEST_EID_UNINTELLIGIBLE = 5,
	BUNDLE_CS_REASON_NO_KNOWN_ROUTE          = 6,
	BUNDLE_CS_REASON_NO_TIMELY_CONTACT       = 7,
	BUNDLE_CS_REASON_BLOCK_UNINTELLIGIBLE    = 8,
};

/**
 * Custody Signal Types
 */
enum bundle_custody_signal_type {
	// Reporting node accepted custody of the bundle
	BUNDLE_CS_TYPE_ACCEPTANCE = 0,

	// Reporting node refused custody of the bundle
	BUNDLE_CS_TYPE_REFUSAL    = 1,
};

struct bundle_custody_signal {
	// RFC 5050
	// --------
	//
	// The "custody transfer succeeded" field is expressed in the type.
	//
	//     custody_transfer_succeeded = (type == BUNDLE_CS_TYPE_ACCEPTANCE)
	//
	enum bundle_custody_signal_type type;

	// Additional information
	// ----------------------
	//
	// RFC 5050: always present
	// BPv7-bis: never present
	enum bundle_custody_signal_reason reason;
};

struct bundle_administrative_record {
	enum bundle_administrative_record_type type;
	enum bundle_administrative_record_flags flags;

	struct bundle_status_report *status_report;
	struct bundle_custody_signal *custody_signal;

	uint32_t fragment_offset;
	uint32_t fragment_length;
	uint64_t event_timestamp;
	uint32_t event_nanoseconds;

	uint64_t bundle_creation_timestamp;
	uint16_t bundle_sequence_number;

	uint16_t bundle_source_eid_length;
	char *bundle_source_eid;
};

struct bundle_list {
	struct bundle *data;
	struct bundle_list *next;
};

enum bundle_routing_priority {
	BUNDLE_RPRIO_LOW    = 0,
	BUNDLE_RPRIO_NORMAL = 1,
	BUNDLE_RPRIO_HIGH   = 2,
	BUNDLE_RPRIO_MAX
};



static inline bool bundle_is_fragmented(const struct bundle *bundle)
{
	return HAS_FLAG(bundle->proc_flags, BUNDLE_FLAG_IS_FRAGMENT);
}


static inline bool bundle_must_not_fragment(const struct bundle *bundle)
{
	return HAS_FLAG(bundle->proc_flags, BUNDLE_FLAG_MUST_NOT_BE_FRAGMENTED);
}

static inline bool bundle_block_must_be_replicated(
	const struct bundle_block *block)
{
	return HAS_FLAG(block->flags, BUNDLE_BLOCK_FLAG_MUST_BE_REPLICATED);
}

static inline uint64_t bundle_get_expiration_time(const struct bundle *bundle)
{
	return bundle->creation_timestamp + bundle->lifetime;
}

struct bundle *bundle_init();
void bundle_free_dynamic_parts(struct bundle *bundle);
void bundle_reset(struct bundle *bundle);
void bundle_free(struct bundle *bundle);
void bundle_drop(struct bundle *bundle);

/**
 * Copy bundle's primary block
 *
 * No extension blocks will be copied, thus, the "blocks" and "payload" fields
 * are set to NULL.
 */
void bundle_copy_headers(struct bundle *to, const struct bundle *from);

void bundle_recalculate_header_length(struct bundle *bundle);
struct bundle *bundle_dup(const struct bundle *bundle);

enum bundle_routing_priority bundle_get_routing_priority(
	struct bundle *bundle);

/**
 * Returns the source EID if the bundle as string.
 */
char *bundle_get_source(struct bundle *bundle);

/**
 * Returns the destination EID of the bundle as string.
 */
char *bundle_get_destination(struct bundle *bundle);

/**
 * Returns the Report-To EID of the bundle as string.
 */
char *bundle_get_report_to(struct bundle *bundle);

/**
 * Returns a pointer to the bundle payload data.
 */
uint8_t *bundle_get_payload_data(const struct bundle *bundle, size_t *length);

size_t bundle_sizeof(struct bundle *bundle);
/* TODO: Maybe cache this in bundle struct... */
size_t bundle_block_get_size(struct bundle_block *block);
size_t bundle_get_serialized_size(struct bundle *bundle);
size_t bundle_get_serialized_size_without_payload(struct bundle *bundle);
size_t bundle_get_first_fragment_min_size(struct bundle *bundle);
size_t bundle_get_mid_fragment_min_size(struct bundle *bundle);
size_t bundle_get_last_fragment_min_size(struct bundle *bundle);

struct bundle_list *bundle_list_entry_create(struct bundle *bundle);
struct bundle_list *bundle_list_entry_free(struct bundle_list *entry);

struct bundle_block *bundle_block_create(enum bundle_block_type t);
struct bundle_block_list *bundle_block_entry_create(struct bundle_block *b);
void bundle_block_free(struct bundle_block *b);
struct bundle_block_list *bundle_block_entry_free(struct bundle_block_list *e);
struct bundle_block *bundle_block_dup(struct bundle_block *b);
struct bundle_block_list *bundle_block_entry_dup(struct bundle_block_list *e);
struct bundle_block_list *bundle_block_list_dup(struct bundle_block_list *e);

/**
 * Serializes a bundle into its on-wire byte-string representation.
 */
enum upcn_result bundle_serialize(struct bundle *bundle,
	void (*write)(const void *cla_obj, const void *, const size_t),
	const void *cla_obj);

#ifdef INCLUDE_ADVANCED_COMM

void bundle_print(struct bundle *bundle);
void bundle_print_stack_conserving(struct bundle *bundle);

#endif /* INCLUDE_ADVANCED_COMM */

#endif /* BUNDLE_H_INCLUDED */
