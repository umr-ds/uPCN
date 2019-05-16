#include <math.h>
#include <stdbool.h>
#include <limits.h>

#include "upcn/rrnd.h"

/* PROBABILITIES */

static float estimate_historical_probability(
	const struct rrnd_contact_history *const history);
static float estimate_satpos_accuracy(const unsigned long pos_age);
static float estimate_gsloc_accuracy(const unsigned char elements);

struct rrnd_prob_estimation rrnd_estimator_estimate_prediction_probability(
	const struct rrnd_contact_history *const history,
	const unsigned long satpos_age_sec,
	const unsigned char gsloc_elements,
	const struct rrnd_availability_pattern *const pattern)
{
	float P_h, P_pos, P_loc, P_avail, P_d, P_pred;

	P_h = estimate_historical_probability(history);

	P_pos = estimate_satpos_accuracy(satpos_age_sec);
	P_loc = estimate_gsloc_accuracy(gsloc_elements);
	if (pattern == NULL)
		P_avail = 1.0f;
	else
		P_avail = rrnd_estimator_estimate_avail_accuracy(pattern);
	P_d = P_pos * P_loc * P_avail;

	P_pred = (
		RRND_ESTIMATOR_Apred * P_h +
		RRND_ESTIMATOR_Bpred * P_d +
		RRND_ESTIMATOR_Cpred * P_h * P_d
	);
	RRND_DBG("P_pred=%f (P_h=%f, P_d=%f, P_pos=%f, P_loc=%f, P_avail=%f)\n",
		P_pred, P_h, P_d, P_pos, P_loc, P_avail);

	if (history->contacts_in_bitmap >= RRND_CONTACT_SUCCESS_MIN_ELEMENTS) {
		return (struct rrnd_prob_estimation){
			.historical = P_h,
			.current_estimation = P_d,
			.total = MIN(1.0f, P_pred),
		};
	} else {
		return (struct rrnd_prob_estimation){
			.historical = NAN,
			.current_estimation = P_d,
			.total = NAN,
		};
	}
}

static float estimate_historical_probability(
	const struct rrnd_contact_history *const history)
{
	int i;
	float bitprob;

	if (history->contacts_in_bitmap != 0) {
#if RRND_ESTIMATOR_HIST_ALGO == REP_ALGO_RATIO
		i = sizeof(history->failed_contacts_bitmap) * 8
			- history->contacts_in_bitmap;
		bitprob = __builtin_popcountll(
			(ULLONG_MAX >> i) &
			history->failed_contacts_bitmap
		);
		bitprob = 1.0f - bitprob / history->contacts_in_bitmap;
#elif RRND_ESTIMATOR_HIST_ALGO == REP_ALGO_PROPHET
		bitprob = 0.5f;
		for (i = history->contacts_in_bitmap - 1; i >= 0; i--) {
			if (history->failed_contacts_bitmap & (1ULL << i))
				bitprob *= RRND_ESTIMATOR_PROPHET_GAMMA;
			else
				bitprob += ((1 - bitprob) *
					    RRND_ESTIMATOR_PROPHET_Pi);
		}
#elif RRND_ESTIMATOR_HIST_ALGO == REP_ALGO_MOCGR
		i = sizeof(history->failed_contacts_bitmap) * 8
			- history->contacts_in_bitmap;
		int failed_contact_count = __builtin_popcountll(
			(ULLONG_MAX >> i) &
			history->failed_contacts_bitmap
		);

		for (i = 0; i < history->contacts_in_bitmap -
				failed_contact_count; i++)
			bitprob *= (1 - RRND_ESTIMATOR_MOCGR_BCONF);
		bitprob = 1 - bitprob;
		for (i = 0; i < failed_contact_count; i++)
			bitprob *= (1 - RRND_ESTIMATOR_MOCGR_BCONF);
#elif RRND_ESTIMATOR_HIST_ALGO == REP_ALGO_EMAVG
		bitprob = 0.5f;
		for (i = history->contacts_in_bitmap - 1; i >= 0; i--) {
			if (history->failed_contacts_bitmap & (1ULL << i))
				bitprob *= 1.0f - RRND_ESTIMATOR_EMAVG_ALPHA;
			else
				bitprob = (
					RRND_ESTIMATOR_EMAVG_ALPHA +
					bitprob *
					(1.0f - RRND_ESTIMATOR_EMAVG_ALPHA)
				);
		}
#else
#error Invalid value for RRND_ESTIMATOR_HIST_ALGO
#endif // RRND_ESTIMATOR_HIST_ALGO
	} else {
		bitprob = NAN;
	}
	return bitprob;
}

static float estimate_satpos_accuracy(const unsigned long pos_age)
{
#ifdef RRND_ASSUME_SATPOS_CORRECT
	return 1.0f;
#else /* RRND_ASSUME_SATPOS_CORRECT */
	if (pos_age <= RRND_SATPOS_ACCURACY_THRESHOLD)
		return 1.0f;
	return expf(
		-(RRND_SATPOS_ACCURACY_PARAM) *
		((pos_age - RRND_SATPOS_ACCURACY_THRESHOLD) / 1000));
#endif /* RRND_ASSUME_SATPOS_CORRECT */
}

static float estimate_gsloc_accuracy(const unsigned char elements)
{
	if (elements == RRND_LOCATOR_MAX_RECORDS)
		return 1.0f;
	return (RRND_GSLOC_ACCURACY_MINIMUM +
		(1 - RRND_GSLOC_ACCURACY_MINIMUM) /
		(RRND_LOCATOR_MAX_RECORDS - 3) *
		(elements - 3));
}

float rrnd_estimator_estimate_avail_accuracy(
	const struct rrnd_availability_pattern *const pattern)
{
	unsigned char changes =
		rrnd_availability_pattern_get_sig_changes(pattern);
	unsigned char contacts = pattern->contacts_honored;

	if (contacts < 2 || changes < 1 || pattern->period == 0)
		return 1.0f;
	return MAX(RRND_AVAIL_ACCURACY_MINIMUM, 1 - changes / contacts);
}

void rrnd_estimator_update_probability_metric_after_contact(
	struct rrnd_probability_metrics *const metrics,
	const struct rrnd_contact_history *const history,
	const bool contact_successful)
{
#if RRND_ESTIMATOR_PROB_ALGO == REP_ALGO_RATIO
	int i = sizeof(history->failed_contacts_bitmap) * 8
		- history->contacts_in_bitmap;

	metrics->reliability = __builtin_popcountll(
		(ULLONG_MAX >> i) &
		history->failed_contacts_bitmap
	);
	if (history->contacts_in_bitmap != 0)
		metrics->reliability = (1.0f -
			metrics->reliability / history->contacts_in_bitmap
		);
#elif RRND_ESTIMATOR_PROB_ALGO == REP_ALGO_PROPHET
	if (!contact_successful)
		metrics->reliability *= RRND_ESTIMATOR_PROPHET_GAMMA;
	else
		metrics->reliability += (
			(1.0f - metrics->reliability) *
			RRND_ESTIMATOR_PROPHET_Pi
		);
#elif RRND_ESTIMATOR_PROB_ALGO == REP_ALGO_MOCGR
	if (!contact_successful)
		metrics->reliability *= (1.0f - RRND_ESTIMATOR_MOCGR_BCONF);
	else
		metrics->reliability = 1.0f - (
			(1.0f - metrics->reliability) *
			(1.0f - RRND_ESTIMATOR_MOCGR_BCONF)
		);
#elif RRND_ESTIMATOR_PROB_ALGO == REP_ALGO_EMAVG
	if (!contact_successful)
		metrics->reliability *= 1.0f - RRND_ESTIMATOR_EMAVG_ALPHA;
	else
		metrics->reliability = (
			RRND_ESTIMATOR_EMAVG_ALPHA +
			metrics->reliability *
			(1.0f - RRND_ESTIMATOR_EMAVG_ALPHA)
		);
#elif RRND_ESTIMATOR_PROB_ALGO == REP_ALGO_HIST
	float tmp = estimate_historical_probability(history);

	if (history->contacts_in_bitmap >= RRND_CONTACT_SUCCESS_MIN_ELEMENTS)
		metrics->reliability = (
			RRND_ESTIMATOR_AprobC * metrics->reliability +
			RRND_ESTIMATOR_BprobC * tmp
		);
#else
#error Invalid value for RRND_ESTIMATOR_PROB_ALGO
#endif // RRND_ESTIMATOR_PROB_ALGO

	// Write back value
#if RRND_ESTIMATOR_PROB_ALGO == REP_ALGO_HIST
	// NOTE: if using HIST algo, we can only use own value for std.dev.
	metrics->last_reliabilities[metrics->last_rel_index] = tmp;
#else // RRND_ESTIMATOR_PROB_ALGO == REP_ALGO_HIST
	metrics->last_reliabilities[metrics->last_rel_index] =
		metrics->reliability;
#endif // RRND_ESTIMATOR_PROB_ALGO == REP_ALGO_HIST
	metrics->last_rel_index++;
	if (metrics->last_rel_index == RRND_PROB_HISTORY_LENGTH)
		metrics->last_rel_index = 0;
	if (metrics->last_rel_count != RRND_PROB_HISTORY_LENGTH)
		metrics->last_rel_count++;
}

void rrnd_estimator_update_probability_metric(
	struct rrnd_probability_metrics *const metrics,
	const float new_metric, const float trust_estimation)
{
	// Simple weighed avg. for testing
	metrics->reliability = (
		RRND_ESTIMATOR_Aprob * metrics->reliability +
		RRND_ESTIMATOR_Bprob * new_metric +
		RRND_ESTIMATOR_Cprob * new_metric * trust_estimation +
		RRND_ESTIMATOR_Cprob * metrics->reliability *
			(1.0f - trust_estimation)
	);
}

float rrnd_estimator_get_reliability_variance(
	const struct rrnd_probability_metrics *const metrics)
{
	unsigned char i;
	float mean = 0.0f, var = 0.0f;

	for (i = 0; i < metrics->last_rel_count; i++)
		mean += metrics->last_reliabilities[i];
	mean /= metrics->last_rel_count;
	for (i = 0; i < metrics->last_rel_count; i++)
		var += (
			(metrics->last_reliabilities[i] - mean) *
			(metrics->last_reliabilities[i] - mean)
		);
	var /= metrics->last_rel_count;
	if (isnan(var))
		return 1.0f; // maximum variance
	return var;
}

/* CAPACITY */

/*
 * NOTE
 * For future optimization, see also:
 * http://math.stackexchange.com/questions/102978/
 *     incremental-computation-of-standard-deviation
 * https://en.wikipedia.org/wiki/
 *     Algorithms_for_calculating_variance#Online_algorithm
 */
struct rrnd_capacity_estimation rrnd_estimator_estimate_capacity(
	const unsigned long max_tx_bitrate,
	const struct rrnd_contact_history *const history,
	const unsigned long contact_duration)
{
	int i;
	unsigned long long brsum;
	long long br, sqsum;
	float expected_usable_duration;
	float hist_match = 1.0f;
	struct rrnd_capacity_estimation ret;

	// We tanke the interval matches of the last predictions to
	// pessimistically calculate an expected capacity.
	if (history->elements != 0) {
		hist_match = 0.0f;
		for (i = 0; i < history->elements; i++)
			hist_match += history->prediction_reliability[i];
		hist_match /= history->elements;
	}
	// in seconds
	expected_usable_duration = hist_match * contact_duration / 1000.0f;
	if (history->elements < RRND_ESTIMATOR_MIN_HISTORY_RECORDS) {
		ret.mean = (unsigned long)((float)max_tx_bitrate *
			RRND_ESTIMATOR_MEAN_BITRATE_FACTOR);
		ret.std_deviation = (float)max_tx_bitrate *
			RRND_ESTIMATOR_DEFAULT_STD_DEVIATION_FACTOR;
	} else {
		brsum = sqsum = 0;
		for (i = 0; i < history->elements; i++)
			brsum += history->tx_bitrate[i];
		ret.mean = brsum / history->elements;
		for (i = 0; i < history->elements; i++) {
			br = (long long)history->tx_bitrate[i];
			sqsum += (br - (long long)ret.mean)
				* (br - (long long)ret.mean);
		}
		ret.std_deviation = sqrtf((float)sqsum / history->elements);
	}
	/*RRND_DBG("Contact BR estimation: %lu; %f\n",*/
	/*	 ret.mean, ret.std_deviation);*/
	ret.mean *= expected_usable_duration;
	ret.std_deviation *= expected_usable_duration;
	return ret;
}
