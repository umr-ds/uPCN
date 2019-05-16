#ifndef CONFIG_H_INCLUDED
#define CONFIG_H_INCLUDED

/*
 * [GENERAL] general configuration options
 */
/* The device we're communicating with should generally be faster */
#define COMM_RX_QUEUE_LENGTH 3072
#define COMM_TX_QUEUE_LENGTH 1024
#define COMM_TX_TIMEOUT -1 /* -1 == infinite */
/* If this is defined, comm_* waits for the PC to fetch outgoing data */
#define COMM_BLOCK_ON_DISCONNECT
/* OK for up to 64 bits */
#define MAX_SDNV_SIZE 10
/* 01/01/2000 00:00:00 UTC */
#define DTN_TIMESTAMP_OFFSET 946684800
#define UPCN_SCHEME "dtn"
#define UPCN_SSP "//ops-sat.dtn"
/* global transmission bitrate */
#define TRANSMISSION_BITRATE 512
#define ROUTER_QUEUE_LENGTH 30
#define BUNDLE_QUEUE_LENGTH 10
#define FS_QUEUE_LENGTH 10
/* Contact dropping / failed forwarding policy */
enum failed_forwarding_policy {
	POLICY_DROP,
	POLICY_DROP_IF_NO_CUSTODY,
	POLICY_TRY_RE_SCHEDULE
};
/* Policy for re-scheduling bundles on dropped contacts, etc. */
#define FAILED_FORWARD_POLICY POLICY_DROP_IF_NO_CUSTODY
/* If policy decides to re-schedule, but processing times out => drop */
/* Set to 0 for infinite delay which means possible deadlock */
#define FAILED_FORWARD_TIMEOUT 5000



/*
 * [LOG] configuration related to logging and output
 */
/* Calls = 512 B, must be < 256 */
#define LOG_MAX_CALLS 128
/* Events = 8192 B */
#define LOG_MAX_EVENTS 1024



/*
 * [BUNDLE] configuration of Bundle releated
 * processing
 */
#ifdef PLATFORM_POSIX
/* Bundles requiring more space will be dropped immediately */
#ifdef THROUGHPUT_TEST
#define BUNDLE_QUOTA 100000000
#else /* THROUGHPUT_TEST */
#define BUNDLE_QUOTA 250000
#endif /* THROUGHPUT_TEST */
#elif defined PLATFORM_STM32
/* Bundles requiring more space will be dropped immediately */
#define BUNDLE_QUOTA 24576
#else
/* FALLBACK value
 * Bundles requiring more space will be dropped immediately
 */
#define BUNDLE_QUOTA 24576
#endif

/* The maximum count of bundles for which we have custody at a time */
#define CUSTODY_MAX_BUNDLE_COUNT 16
/* The maximum size of a bundle for which custody will be accepted */
#define CUSTODY_MAX_BUNDLE_SIZE 1024

/*
 * [GROUNDSTATION] ground station releated configuration
 * options
 */
#define CONTACT_TX_TASK_QUEUE_LENGTH 3
/*
 * [ROUTING] configuration options related to the routing
 * algorithm
 */
#define ROUTER_MAX_FRAGMENTS 4
#define ROUTER_MAX_CONTACTS 4
/* Default values */
#define FRAGMENT_MIN_PAYLOAD 8
#define MIN_PROBABILITY 0.90f
#define MIN_GS_CONFIDENCE_OPPORTUNISTIC 0.2f
/* This value has to be greater than or equal to MIN_PROBABILITY */
#define MIN_GS_CONFIDENCE_DETERMINISTIC 0.90f
#define GS_TRUSTWORTHINESS_WEIGHT 0.0f
#define GS_RELIABILITY_WEIGHT 1.0f
/* The minimum available time for an optimization to be triggered */
#define OPTIMIZATION_MIN_TIME 2
#define OPTIMIZATION_MAX_BUNDLES 3
#define OPTIMIZATION_MAX_PRE_BUNDLES 9
#define OPTIMIZATION_MAX_PRE_BUNDLES_CONTACT 3
#define ROUTER_OPTIMIZER_DELAY 50
/* Number of slots in the node hash table */
#define NODE_HTAB_SLOT_COUNT 128
/* Below this, NBFs will be consulted */
#define ROUTER_MIN_CONTACTS_HTAB 10
/* The "reliability" of nodes discovered via NBF lookup */
#define ROUTER_NBF_BASE_RELIABILITY 0.6f
/* Below this, a default route will be used */
#define ROUTER_MIN_CONTACTS_NBF 2
/* The "reliability" of the default route */
/* This makes sure to use the default route everytime and not fail */
#define ROUTER_DEF_BASE_RELIABILITY MIN_PROBABILITY
/* Maximum number of GSs discovered through lookups */
#define ROUTER_MAX_REQ_GS 5



/*
 * [CONTACT] contact related configuration
 */
/* Maximum period (in ms) for checking */
#define CONTACT_CHECKING_MAX_PERIOD 600000
/* Number of contacts generated for discovered ground stations */
#define MINIMUM_GENERATED_CONTACTS 1
/* Define if contact predictions triggered by dbg command should be added */
/*#define INTEGRATE_DEMANDED_CONTACTS*/



/*
 * [NDP] configuration for NDP beacon content
 * and neighbor discovery
 */
//#define ND_USE_COOKIES
/* Disable own beacons? You should not define ND_USE_COOKIES, then. */
#define ND_DISABLE_BEACONS
#define BEACON_PERIOD 10000
#define BEACON_QUEUE_LENGTH 5
#define BEACON_PROC_DELAY 100
#define TLV_MAX_DEPTH 32
#define DEFAULT_TX_BITRATE 500
#define DEFAULT_RX_BITRATE 500
#define MAX_EID_LENGTH 1024
#define MAX_SVC_COUNT 128
#define MAX_TLV_VALUE_LENGTH 256
#define BEACON_MAX_COOKIE_ENTRIES 10
/* HMAC */
#define UPCN_HASH_LENGTH 32
#define BEACON_COOKIE_HASH_LENGTH UPCN_HASH_LENGTH
/* Cookie stuff for incoming beacons */
#define BEACON_BLOOM_TABLE_SZ 16
#define BEACON_BLOOM_SALT_CNT 4
/* Own bf = NBF in outgoing beacons */
#define BEACON_OWN_BLOOM_TABLE_SZ 16
#define BEACON_OWN_BLOOM_SALT_CNT 6
#define BEACON_OWN_BLOOM_REFRESH_INTERVAL 30
#ifdef DEBUG
#define BEACON_COOKIE_DISCOVERY_TIME 10
#else /* DEBUG */
#define BEACON_COOKIE_DISCOVERY_TIME 5000
#endif /* DEBUG */
#define BEACON_PRIVATE_KEY_LENGTH 64
#define DISCOVERY_NODE_LIST_INITIAL_P 0.5f
#define DISCOVERY_NBF_DEFAULT_HASHES 10

/*
 * [OTHER] further tests and config parameters
 */
/* Test the performance of the SGP4 implementation */
/* #define TEST_SGP4_PERFORMANCE */

#endif /* CONFIG_H_INCLUDED */
