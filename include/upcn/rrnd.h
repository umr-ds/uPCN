#ifndef RRND_H_INCLUDED
#define RRND_H_INCLUDED

#include <stddef.h>
#include <stdbool.h>

#include "drv/vec3d.h"

/* Max entries in location history (crit. times) */
#define RRND_LOCATOR_MAX_RECORDS 10
/* Max EIDs in a beacon */
#define RRND_BEACON_LIST_MAX_NEIGHBORS 3
/* Max entries in contact history */
#define RRND_CONTACT_HISTORY_LENGTH 20
/* Max number of predictions that can be stored */
#define RRND_CONTACT_PREDICTION_LENGTH 20
/* Max bits that are considered as AP history, must be <= 32 */
#define RRND_AVAILABILITY_PATTERN_CHANGE_HISTORY 32
/* Max bits that are considered for failed contacts, must be <= 64 */
#define RRND_CONTACT_SUCCESS_HISTORY 63
/* Min. success bits to be used for reliability calculation */
#define RRND_CONTACT_SUCCESS_MIN_ELEMENTS 5

/* The value that is used if no period is known previously */
#define RRND_DEFAULT_BEACON_PERIOD 10000 /* 10 s */
/* The default (initial) Earth radius for location determination */
#define RRND_EARTH_RADIUS 6371.0 /* km */
/* The fixed height over the horizon where a contact is considered probable */
#define RRND_START_ELEVATION 0.0 /* km */
/* The step in which the predictor searches for elevation maxima */
#define RRND_PREDICTION_TIME_STEP 1500000 /* 25 min in msec (prev = 95) */
/* If this step count is exceeded, the prediction has failed */
#define RRND_PREDICTOR_MAX_STEPS (int)(24 * 60 / 25) /* before: 45, 15 */
/* The minimum duration of a contact to be considered valid */
#define RRND_CONTACT_MIN_DURATION 5000 /* 5 sec in msec */
/* The maximum time between beacons for a contact to be considered ongoing */
#define RRND_BEACON_MAX_WAIT 120000 /* 120 sec in msec */

/* A correction to be applied to the detected crit. times on recording */
#define RRND_CONTACT_TIME_CORRECTION(beac_period) (0) /* ((beac_period) / 2) */
/* Use non-linear least quares (MPFIT) instead average for trilateration */
#define RRND_TRILATERATION_MPFIT
/* Estimate the start elevation before each prediction using the last contact */
#define RRND_PREDICTION_ESTIMATE_START_ELEVATION 2
/* Defines if RRND should use a record on start elevations in predictions */
// #define RRND_ADVANCED_ELEVATION_TRACKING 12

/* If the intersection of new and old AP are lower, it's a significant change */
#define RRND_AP_SIG_THRESHOLD 0.5
/* The max. deviation of the new AP period for which smaller periods are kept */
#define RRND_AP_MAX_DEVI 600000 /* 10 min */
/* If the accuracy of the AP falls below this value, it is not used */
#define RRND_AP_INTER_MIN_PROBABILITY 0.55f /* See _AVAIL_ACCURACY_MINIMUM */

/* Weights for probability associated with contact predictions */
/* Estimator history weight */
#define RRND_ESTIMATOR_Apred 1.0f
/* Estimator cur-data-weight */
#define RRND_ESTIMATOR_Bpred 0.0f
/* Estimator history * cur-data product weight */
#define RRND_ESTIMATOR_Cpred 0.0f
/* History prob if not enough data was collected */
#define RRND_ESTIMATOR_HISTORICAL_INITIAL_PROBABILITY 0.5f
/* Should we assume SATPOS data to be always correct? */
#define RRND_ASSUME_SATPOS_CORRECT
/* Max SATPOS age for which 1.0 is returned for SATPOS accuracy */
#define RRND_SATPOS_ACCURACY_THRESHOLD 259200000 /* 72h */
/* See estimate_satpos_accuracy, exp() param for delta_t > THRESH */
#define RRND_SATPOS_ACCURACY_PARAM 0.0000005f /* 0.96, 0.92, 0.88, 0.84, 0.81 */
/* Minimum accuracy returned for location (max = 1.0 = full locator history) */
#define RRND_GSLOC_ACCURACY_MINIMUM 0.5f
/* Minimum accuracy for AP / if it changes on evry contact */
#define RRND_AVAIL_ACCURACY_MINIMUM 0.5f
/* Min history for capacity estimation */
#define RRND_ESTIMATOR_MIN_HISTORY_RECORDS 1
/* If too few records, use max_br * this factor as mean bitrate */
#define RRND_ESTIMATOR_MEAN_BITRATE_FACTOR 1.0f
/* If too few records, use max_br * this factor as std. dev. */
#define RRND_ESTIMATOR_DEFAULT_STD_DEVIATION_FACTOR 0.0f

/* Use a ratio of successful / predicted contacts for reliability */
#define REP_ALGO_RATIO 1
/* Modify probability for each contact like PRoPHET does */
#define REP_ALGO_PROPHET 2
/* OCGR-like modification of probability */
#define REP_ALGO_MOCGR 3
/* Use an exponential moving average formula to modify probability */
#define REP_ALGO_EMAVG 4
/* Use HIST_ALGO and call rrnd_estimator_update_probability_metric w/ result */
#define REP_ALGO_HIST 5

/* Algorithm for modifying station probability after each contact */
#define RRND_ESTIMATOR_PROB_ALGO REP_ALGO_HIST
/* Algorithm for historical prob. in prediction reliability calculation */
#define RRND_ESTIMATOR_HIST_ALGO REP_ALGO_PROPHET

/* PRoPHET algo: Constant for multiplying the reliability on failed contacts */
#define RRND_ESTIMATOR_PROPHET_GAMMA 0.9f
/* PRoPHET algo: Constant for growing the reliability on successful contacts */
#define RRND_ESTIMATOR_PROPHET_Pi 0.1f
/* OCGR algo: Weight for modifying the reliability */
#define RRND_ESTIMATOR_MOCGR_BCONF 0.1f
/* Exponential moving average algo: Weight for modifying the reliability */
#define RRND_ESTIMATOR_EMAVG_ALPHA 0.1f

/* Weights for rrnd_estimator_update_probability_metric */
/* Weight on: old value */
#define RRND_ESTIMATOR_Aprob 0.8f // 0.9f
/* Weight on: new value */
#define RRND_ESTIMATOR_Bprob 0.2f // 0.1f
/* Weight on: new value * trust + old value * (1 - trust) */
#define RRND_ESTIMATOR_Cprob 0.0f // 0.0f

/* Weights for historical update in after-contact update (REP_ALGO_HIST) */
/* Weight on: old value */
#define RRND_ESTIMATOR_AprobC 0.8f // 0.9f
/* Weight on: new value */
#define RRND_ESTIMATOR_BprobC 0.2f // 0.1f

/* For prob history variance calculation */
#define RRND_PROB_HISTORY_LENGTH 10

/* Min match of a prediction interval to be valid */
#define RRND_VALIDATOR_MIN_VALIDATION_MATCH 0.05f
/* Min probability of a prediction to be considered */
#define RRND_VALIDATOR_MIN_PREDICTION_ACCURACY 0.0f
/* Min elevation (in km) to declare a given time as valid */
#define RRND_VALIDATOR_MIN_VALIDATION_ELEVATION -2000.0

/* >>> Common defines */

#ifndef MIN
#define MIN(a, b) ({ \
	__typeof__(a) _a = (a); \
	__typeof__(b) _b = (b); \
	_b < _a ? _b : _a; \
})
#endif /* MIN */

#ifndef MAX
#define MAX(a, b) ({ \
	__typeof__(a) _a = (a); \
	__typeof__(b) _b = (b); \
	_b > _a ? _b : _a; \
})
#endif /* MAX */

#ifndef ABS
#define ABS(a) ({ \
	__typeof__(a) _a = (a); \
	_a < 0 ? -_a : _a; \
})
#endif /* ABS */

#ifndef SIGN
#define SIGN(a) ((a < 0) ? -1 : 1)
#endif /* ABS */

#ifdef DEBUG

#define _RRND_PREFIX "tools/libupcn/"
#define RRND_DBG(f, ...) rrnd_io_debug_printf( \
	"[%s:%d] " f, __FILE__ + sizeof(_RRND_PREFIX) - 1, \
	__LINE__, ##__VA_ARGS__)

int rrnd_io_debug_printf(const char *format, ...);
void rrnd_io_debug_set_enabled(const bool enabled);

#else /* DEBUG */

#define RRND_DBG(f, ...) (void)0
#define rrnd_io_debug_set_enabled(a) (void)(a)

#endif /* DEBUG */

#ifndef DTN_TS_OFFSET
/* 01/01/2000 00:00:00 UTC */
#define DTN_TS_OFFSET 946684800ULL
#endif /* DTN_TS_OFFSET */

/* <<< Common defines */

enum rrnd_rc {
	RRND_SUCCESS = 0,
	RRND_FAILURE = 1
};

/* Bytes the below enum requires, used e.g. in uPCN for status transmission */
#define RRND_STATUS_SIZEOF 2

enum rrnd_status {
	RRND_STATUS_UNCHANGED     = 0x0000, /* Successful, GS data unchanged */
	RRND_STATUS_SUCCESS       = 0x0000, /* Successful, GS data unchanged */
	RRND_STATUS_UPDATED       = 0x0001, /* GS data has been updated */
	RRND_STATUS_DELETE        = 0x0002, /* GS should be deleted */
	/* What has been updated */
	RRND_UPDATE_CONTACTS      = 0x0010, /* The predicted contacts */
	RRND_UPDATE_LOCATOR_DATA  = 0x0020, /* The locator struct */
	RRND_UPDATE_NEIGHBOR_INFO = 0x0040, /* Neighbor list or NBF */
	RRND_UPDATE_AVAILABILITY  = 0x0080, /* Availability Pattern */
	/* FAILURE */
	RRND_STATUS_FAILED        = 0x0008, /* Combined with flags below */
	/* What has failed */
	RRND_FAILURE_PREDICTOR    = 0x1000,
	RRND_FAILURE_LOCATOR      = 0x2000,
	RRND_FAILURE_VALIDATOR    = 0x0400,
	/* Special status code, valid without RRND_STATUS_FAILED */
	RRND_FAILURE_OVERFLOW     = 0x0800
};

enum rrnd_beacon_flags {
	RRND_FLAG_NONE                      = 0x00,
	RRND_FLAG_INTERNET_ACCESS           = 0x01,
	RRND_FLAG_MOBILE_NODE               = 0x02,
	RRND_FLAG_INTERMITTENT_AVAILABILITY = 0x04
	/* NOTE: Don't use most sig. Nibble; merged into rrnd_gs_flags */
};

struct rrnd_beacon {
	unsigned long tx_bitrate; /* bit / s */
	unsigned long rx_bitrate; /* bit / s */
	unsigned long availability_duration; /* ms */
	unsigned short sequence_number;
	unsigned short period; /* centisecs */
	enum rrnd_beacon_flags flags;
};

struct rrnd_prob_estimation {
	float historical;
	float current_estimation;
	float total;
};

struct rrnd_capacity_estimation { /* bit */
	unsigned long mean;
	float std_deviation;
};

struct rrnd_contact_info {
	float probability;
	unsigned long long start; /* ms */
	unsigned long duration; /* ms */
	float max_elevation; /* km */
	struct rrnd_capacity_estimation capacity;
};

struct rrnd_locator_data {
	struct vec3f crit_satpos[RRND_LOCATOR_MAX_RECORDS];
	unsigned char elements;
	unsigned char index;
	unsigned char location_up2date;
#ifdef RRND_ADVANCED_ELEVATION_TRACKING
	float elevation_map[RRND_ADVANCED_ELEVATION_TRACKING];
#endif /* RRND_ADVANCED_ELEVATION_TRACKING */
};

/* TODO:
 * - record + use missed beacons (seqnum?), compare with estimated b. count
 * - delete GS after specific count of missed contacts
 */
struct rrnd_contact_history {
	unsigned long long failed_contacts_bitmap;
	float prediction_reliability[RRND_CONTACT_HISTORY_LENGTH];
	unsigned long tx_bitrate[RRND_CONTACT_HISTORY_LENGTH]; /* bit / s */
	unsigned char elements, index;
	unsigned char contacts_in_bitmap;
};

struct rrnd_predictions {
	struct rrnd_contact_info last;
	/* NOTE: This is only kind of a "cache" for metric calculation, etc. */
	struct rrnd_contact_info contacts[RRND_CONTACT_PREDICTION_LENGTH];
	unsigned char start, count;
};

struct rrnd_availability_pattern {
	unsigned long offset; /* mod */
	unsigned long on_time;
	unsigned long period;
	unsigned long significant_changes;
	unsigned long last_on_time;
	unsigned char contacts_honored;
	unsigned char valid;
};

struct rrnd_probability_metrics {
	float reliability;
	unsigned long mean_rx_bitrate; /* NOTE: cur. unused */
	unsigned long mean_tx_bitrate;
	float last_reliabilities[RRND_PROB_HISTORY_LENGTH];
	unsigned char last_rel_index, last_rel_count;
};

enum rrnd_gs_flags {
	RRND_GS_FLAG_NONE                      = 0x00,
	/* Merged beacon flags (most recent) */
	RRND_GS_FLAG_INTERNET_ACCESS           = 0x01,
	RRND_GS_FLAG_MOBILE_NODE               = 0x02,
	RRND_GS_FLAG_INTERMITTENT_AVAILABILITY = 0x04,
	/* GS-specific flags */
	RRND_GS_FLAG_INITIALIZED               = 0x10, /* NOTE: cur. unused */
	RRND_GS_FLAG_IN_CONTACT                = 0x20,
	RRND_GS_FLAG_LOCATION_VALID            = 0x40
};

struct rrnd_gs_info {
	void *gs_reference;
	unsigned long rx_bitrate;
	unsigned long tx_bitrate;
	unsigned long last_mean_tx_bitrate;
	struct rrnd_probability_metrics prob_metrics;
	unsigned long long last_contact_start_time;
	unsigned long long last_contact_end_time;
	unsigned short beacon_period;
	unsigned short beacon_count; /* TODO: use */
	enum rrnd_gs_flags flags;
	struct vec3d location;
	struct rrnd_locator_data locator_data;
	struct rrnd_availability_pattern availability_pattern;
	struct rrnd_contact_history contact_history;
	struct rrnd_predictions predictions;
};

struct rrnd_validation_result {
	float match;
	signed char prediction;
	unsigned char success;
};

/* Location Determination */

enum rrnd_rc rrnd_locator_add(
	struct rrnd_locator_data *const data, const struct vec3d satpos);

#ifdef RRND_ADVANCED_ELEVATION_TRACKING
enum rrnd_rc rrnd_locator_update_elevation_map(
	struct rrnd_locator_data *const data,
	const struct vec3d satpos, const struct vec3d gspos);
#else
#define rrnd_locator_update_elevation_map(a, b, c) ((void)0)
#endif /* RRND_ADVANCED_ELEVATION_TRACKING */

enum rrnd_rc rrnd_locator_locate(
	struct vec3d *const result, const unsigned char valid_in,
	struct rrnd_locator_data *const data,
	const double earth_radius, const double start_elevation);

double rrnd_locator_estimate_start_elevation(
	const struct rrnd_locator_data *const data,
	const struct vec3d gspos, const struct vec3d satpos);

/* Contact Prediction */

enum rrnd_rc rrnd_predictor_predict(struct rrnd_contact_info *const result,
	const struct vec3d gspos, const unsigned long long start_time,
	const unsigned long min_duration, const unsigned long step,
	const struct rrnd_locator_data *const locator_data,
	const struct rrnd_availability_pattern *const avail_pattern,
	struct vec3d (*const get_satpos)(const unsigned long long time));

double rrnd_get_elevation(
	const struct vec3d satpos, const struct vec3d gspos);

/* Capacity and Probability Estimation */

struct rrnd_prob_estimation rrnd_estimator_estimate_prediction_probability(
	const struct rrnd_contact_history *const history,
	const unsigned long satpos_age_sec,
	const unsigned char gsloc_elements,
	const struct rrnd_availability_pattern *const pattern);

float rrnd_estimator_estimate_avail_accuracy(
	const struct rrnd_availability_pattern *const pattern);

void rrnd_estimator_update_probability_metric_after_contact(
	struct rrnd_probability_metrics *const metrics,
	const struct rrnd_contact_history *const history,
	const bool contact_successful);

void rrnd_estimator_update_probability_metric(
	struct rrnd_probability_metrics *const metrics,
	const float new_metric, const float trust_estimation);

struct rrnd_capacity_estimation rrnd_estimator_estimate_capacity(
	const unsigned long max_tx_bitrate,
	const struct rrnd_contact_history *const history,
	const unsigned long contact_duration);

float rrnd_estimator_get_reliability_variance(
	const struct rrnd_probability_metrics *const metrics);

/* Validation */

struct rrnd_validation_result rrnd_validator_validate_contact_time(
	const unsigned long long time, const struct vec3d gspos,
	const struct rrnd_predictions *const predictions,
	struct vec3d (*const get_satpos)(const unsigned long long time));

struct rrnd_validation_result rrnd_validator_validate_contact(
	const unsigned long long start, const unsigned long long end,
	const struct vec3d gspos,
	const struct rrnd_predictions *const predictions,
	struct vec3d (*const get_satpos)(const unsigned long long time));

/* Availability Pattern */

enum rrnd_rc rrnd_availability_pattern_update(
	struct rrnd_availability_pattern *const pattern,
	const unsigned long long time, const unsigned long on_time,
	const unsigned long long last_contact_end, const unsigned char new_c);

unsigned char rrnd_availability_pattern_get_sig_changes(
	const struct rrnd_availability_pattern *const pattern);

enum rrnd_rc rrnd_availability_pattern_intersect(
	struct rrnd_contact_info *const prediction,
	const struct rrnd_availability_pattern *const pattern);

/* Helpers */

float rrnd_interval_intersect(
	const unsigned long long orig_start, const unsigned long long orig_end,
	const unsigned long long new_start, const unsigned long long new_end);

/* Public API */

enum rrnd_status rrnd_check_and_finalize(
	struct rrnd_gs_info *const station, const unsigned long long cur_time,
	struct vec3d (*const get_satpos)(const unsigned long long time));

enum rrnd_status rrnd_process(
	struct rrnd_gs_info *const station, const struct rrnd_beacon beacon,
	const unsigned long long time, const unsigned long br_estimation,
	struct vec3d (*const get_satpos)(const unsigned long long time));

enum rrnd_status rrnd_infer_contact(
	struct rrnd_gs_info *const station, const unsigned long long cur_time,
	struct vec3d (*const get_satpos)(const unsigned long long time),
	const unsigned long satpos_age_sec);

size_t rrnd_print_gs_info(
	char *buf, const size_t bufsize, const char *const eid,
	const struct rrnd_gs_info *const gs);

enum rrnd_status rrnd_integrate_metrics(
	struct rrnd_gs_info *const station,
	const struct rrnd_gs_info *const source,
	const struct rrnd_probability_metrics metrics);

#endif /* RRND_H_INCLUDED */
