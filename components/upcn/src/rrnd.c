#include <string.h>
#include <stdbool.h>
#include <limits.h>
#include <math.h>

#include "upcn/upcn.h"
#include "upcn/rrnd.h"
#include "drv/vec3d.h"
#include "drv/ellipsoidal_coords.h"

// Calculates a time correction for the start and/or end of contacts with
// the specified station.
#define T_CORR(station) ( \
	RRND_CONTACT_TIME_CORRECTION(((station)->beacon_period == 0) \
		? RRND_DEFAULT_BEACON_PERIOD : (station)->beacon_period * 10))

// Gets the end time of the specified contact
#define CONTACT_END(c) ((c).start + (c).duration)
// Gets the last element of a ring buffer with the specified parameters.
#define RINGBUF_LAST(start, count, capacity) \
	(((start) + (count) - 1) % (capacity))

static void rrnd_try_locate(struct rrnd_gs_info *const station);

static enum rrnd_status cleanup_predictions(
	struct rrnd_predictions *const pred,
	struct rrnd_contact_history *const history,
	struct rrnd_probability_metrics *const metrics,
	const unsigned long long time,
	const bool last_contact_successful);

static enum rrnd_status append_prediction(
	struct rrnd_predictions *const pred,
	const struct rrnd_contact_info p);

static enum rrnd_status rrnd_update_contact_history_and_predictions(
	struct rrnd_gs_info *const station,
	const unsigned long long cur_time,
	struct vec3d (*const get_satpos)(const unsigned long long time));

static int out_of_last_contact(
	struct rrnd_gs_info *const station, const unsigned long long time);

static void update_station_from_beacon(
	struct rrnd_gs_info *const station,
	const struct rrnd_beacon *const beacon);

static double get_earth_radius(const struct rrnd_gs_info *const station);

static void append_to_history(struct rrnd_contact_history *const h,
	const float match, const unsigned long br);

static void update_bitrate_estimation(
	struct rrnd_gs_info *const station,
	const unsigned long our_estimation, const unsigned long gs_estimation);

/*
 * Starts a new contact by (re)initializing the relevant data structures.
 */
static inline enum rrnd_status rrnd_start_contact(
	struct rrnd_gs_info *const station, const struct rrnd_beacon beacon,
	const unsigned long long time, const unsigned long br_estimation,
	struct vec3d (*const get_satpos)(const unsigned long long time))
{
	struct vec3d satpos;
	enum rrnd_status rc = RRND_STATUS_UNCHANGED;

	station->flags |= RRND_GS_FLAG_IN_CONTACT;
	if (HAS_FLAG(station->flags, RRND_GS_FLAG_LOCATION_VALID) &&
	    rrnd_validator_validate_contact_time(
			time, station->location, &station->predictions,
			get_satpos).success == 0)
		return RRND_STATUS_FAILED | RRND_FAILURE_VALIDATOR;
	update_station_from_beacon(station, &beacon);
	rc |= RRND_STATUS_UPDATED;
	if (br_estimation != 0 && br_estimation != ULONG_MAX)
		station->last_mean_tx_bitrate = br_estimation;
	satpos = get_satpos(time - T_CORR(station));
	rrnd_locator_add(&station->locator_data, satpos);
	rc |= RRND_UPDATE_LOCATOR_DATA;
	rrnd_try_locate(station);
	if (HAS_FLAG(station->flags, RRND_GS_FLAG_LOCATION_VALID))
		rrnd_locator_update_elevation_map(
			&station->locator_data, satpos, station->location);
	if (HAS_FLAG(station->flags,
			RRND_GS_FLAG_INTERMITTENT_AVAILABILITY)) {
		rrnd_availability_pattern_update(
			&station->availability_pattern, time,
			beacon.availability_duration,
			station->last_contact_end_time, 1);
		rc |= RRND_UPDATE_AVAILABILITY;
	}
	station->beacon_count = 0;
	station->last_contact_start_time = time;
	return rc;
}

/*
 * Determine whether a contact has passed and, if that is the case,
 * finish it and update resulting data.
 */
enum rrnd_status rrnd_check_and_finalize(
	struct rrnd_gs_info *const station, const unsigned long long cur_time,
	struct vec3d (*const get_satpos)(const unsigned long long time))
{
	struct vec3d satpos;
	enum rrnd_status rc = RRND_STATUS_UNCHANGED;

	if (out_of_last_contact(station, cur_time) &&
		HAS_FLAG(station->flags, RRND_GS_FLAG_IN_CONTACT)
	) {
		satpos = get_satpos(
			station->last_contact_end_time + T_CORR(station));
		rrnd_locator_add(&station->locator_data, satpos);
		station->flags &= ~RRND_GS_FLAG_IN_CONTACT;
		rc |= RRND_STATUS_UPDATED | RRND_UPDATE_LOCATOR_DATA;
		rrnd_try_locate(station);
		if (HAS_FLAG(station->flags, RRND_GS_FLAG_LOCATION_VALID))
			rrnd_locator_update_elevation_map(
				&station->locator_data, satpos,
				station->location);
		rc |= rrnd_update_contact_history_and_predictions(
			station, cur_time, get_satpos);
	}
	return rc;
}

/*
 * Processes the specified beacon for the specified GS. Has to be called for
 * each received beacon. The contained information is used to automatically
 * determine the bounds of contacts and build up a history to infer further
 * values in the future.
 */
enum rrnd_status rrnd_process(
	struct rrnd_gs_info *const station, const struct rrnd_beacon beacon,
	const unsigned long long time, const unsigned long br_estimation,
	struct vec3d (*const get_satpos)(const unsigned long long time))
{
	enum rrnd_status rc = RRND_STATUS_SUCCESS;

	rc |= rrnd_check_and_finalize(station, time, get_satpos);
	if (HAS_FLAG(rc, RRND_STATUS_FAILED))
		return rc;
	if (!HAS_FLAG(station->flags, RRND_GS_FLAG_IN_CONTACT)) {
		rc |= rrnd_start_contact(
			station, beacon, time, br_estimation, get_satpos);
		if (HAS_FLAG(rc, RRND_STATUS_FAILED))
			return rc;
	} else if (HAS_FLAG(station->flags,
			RRND_GS_FLAG_INTERMITTENT_AVAILABILITY)) {
		rrnd_availability_pattern_update(
			&station->availability_pattern, time,
			beacon.availability_duration,
			station->last_contact_end_time, 0);
		rc |= RRND_UPDATE_AVAILABILITY;
	}
	update_bitrate_estimation(station, br_estimation, beacon.rx_bitrate);
	station->beacon_count++;
	station->last_contact_end_time = time;
	rc |= RRND_STATUS_UPDATED;
	return rc;
}

/*
 * Requests a contact prediction for the specified station and returns a
 * status indicating whether it succeeded or not. The prediction is stored
 * within the GS record.
 */
enum rrnd_status rrnd_infer_contact(
	struct rrnd_gs_info *const station, const unsigned long long cur_time,
	struct vec3d (*const get_satpos)(const unsigned long long time),
	const unsigned long satpos_age_sec)
{
	enum rrnd_status rc = RRND_STATUS_SUCCESS;
	unsigned long long start_time;
	struct rrnd_contact_info result;
	struct rrnd_availability_pattern *ap = NULL;
	unsigned char last_prediction_index;
	float tmp_metric;

	rc |= rrnd_check_and_finalize(station, cur_time, get_satpos);
	if (HAS_FLAG(rc, RRND_STATUS_FAILED))
		return rc;
	rrnd_try_locate(station);
	if (!HAS_FLAG(station->flags, RRND_GS_FLAG_LOCATION_VALID))
		return rc | RRND_STATUS_FAILED | RRND_FAILURE_LOCATOR;
	if (station->predictions.count != 0) {
		last_prediction_index = RINGBUF_LAST(
			station->predictions.start,
			station->predictions.count,
			RRND_CONTACT_PREDICTION_LENGTH
		);
		start_time = (1 + CONTACT_END(
			station->predictions.contacts[last_prediction_index]
		));
	} else {
		start_time = station->last_contact_end_time;
	}
	start_time = MAX(start_time, cur_time);
	if (HAS_FLAG(station->flags, RRND_GS_FLAG_INTERMITTENT_AVAILABILITY))
		ap = &station->availability_pattern;
	if (rrnd_predictor_predict(
			&result, station->location, start_time,
			RRND_CONTACT_MIN_DURATION, RRND_PREDICTION_TIME_STEP,
			&station->locator_data, ap, get_satpos) != RRND_SUCCESS)
		return rc | RRND_STATUS_FAILED | RRND_FAILURE_PREDICTOR;
	result.capacity = rrnd_estimator_estimate_capacity(
		station->tx_bitrate, &station->contact_history,
		result.duration);
	tmp_metric = rrnd_estimator_estimate_prediction_probability(
		&station->contact_history, satpos_age_sec,
		station->locator_data.elements, ap).total;
	result.probability = (isnan(tmp_metric)
		? station->prob_metrics.reliability
		: tmp_metric);
	// TODO: evaluate whether to update station probability here
	rc |= append_prediction(&station->predictions, result);
	return rc;
}

/*
 * Integrates the specified probability metrics record (as e.g. received from
 * another node) into the metric store associated with the GS.
 */
enum rrnd_status rrnd_integrate_metrics(
	struct rrnd_gs_info *const station,
	const struct rrnd_gs_info *const source,
	const struct rrnd_probability_metrics metrics)
{
	if (metrics.reliability < 0.0f || metrics.reliability > 1.0f)
		return RRND_STATUS_FAILED;
	rrnd_estimator_update_probability_metric(
		&station->prob_metrics,
		metrics.reliability,
		// TODO: Trust estimation + storing in "source"
		0.5f
	);
	return RRND_STATUS_SUCCESS | RRND_STATUS_UPDATED;
	// TODO: Capacity / Bitrate!
}

/*
 * Tries to find a location of the specified ground station via rrnd_locator.
 */
static void rrnd_try_locate(struct rrnd_gs_info *const station)
{
	if (rrnd_locator_locate(&station->location,
		HAS_FLAG(station->flags, RRND_GS_FLAG_LOCATION_VALID),
		&station->locator_data, get_earth_radius(station),
		RRND_START_ELEVATION)
			== RRND_SUCCESS
	) {
		station->flags |= RRND_GS_FLAG_LOCATION_VALID;
	} else {
		station->flags &= ~RRND_GS_FLAG_LOCATION_VALID;
		station->location = vec3d_null();
	}
}

/*
 * Removes old predictions from the contact history ring buffer and marks them
 * as failed with the exception of the last one: This can be marked as either
 * successful or failed by setting the last_contact_successful parameter.
 */
static enum rrnd_status cleanup_predictions(
	struct rrnd_predictions *const pred,
	struct rrnd_contact_history *const history,
	struct rrnd_probability_metrics *const metrics,
	const unsigned long long time,
	const bool last_contact_successful)
{
	unsigned char i, count;
	unsigned long long new_bitmap;

	if (pred->count == 0)
		return RRND_STATUS_UNCHANGED;
	for (i = pred->start, count = 0; count < pred->count;
	     i = (i + 1) % RRND_CONTACT_PREDICTION_LENGTH, count++) {
		if (CONTACT_END(pred->contacts[i]) > time)
			break;
	}
	if (count != 0) {
		if (count > 63)
			count = 63;
		history->failed_contacts_bitmap <<= count;
		new_bitmap = ULLONG_MAX >> (64 - count);
		if (last_contact_successful)
			new_bitmap ^= 1ULL; // Set the "newest" bit to 0
		history->failed_contacts_bitmap |= new_bitmap;
		history->contacts_in_bitmap += count;
		if (history->contacts_in_bitmap > RRND_CONTACT_SUCCESS_HISTORY)
			history->contacts_in_bitmap =
				RRND_CONTACT_SUCCESS_HISTORY;
		pred->start = (
			(pred->start + count) % RRND_CONTACT_PREDICTION_LENGTH
		);
		pred->count -= count;
		if (last_contact_successful)
			count--;
		for (i = 0; i < count; i++)
			rrnd_estimator_update_probability_metric_after_contact(
				metrics,
				history,
				false
			);
		if (last_contact_successful)
			rrnd_estimator_update_probability_metric_after_contact(
				metrics,
				history,
				true
			);
		return RRND_STATUS_UPDATED | RRND_UPDATE_CONTACTS;
	}
	return RRND_STATUS_UNCHANGED;
}

/*
 * Adds the specified prediction to the contact history associated with
 * the ground station.
 */
static enum rrnd_status append_prediction(
	struct rrnd_predictions *const pred,
	const struct rrnd_contact_info p)
{
	pred->last = p;
	if (pred->count >= RRND_CONTACT_PREDICTION_LENGTH)
		return RRND_STATUS_UPDATED | RRND_FAILURE_OVERFLOW;
	pred->count++;
	pred->contacts[RINGBUF_LAST(pred->start, pred->count,
				    RRND_CONTACT_PREDICTION_LENGTH)] = p;
	return RRND_STATUS_UPDATED | RRND_UPDATE_CONTACTS;
}

/*
 * Validates whether the contact which occurred was legit and cleans up
 * old, now invalid, predictions.
 * NOTE: This function should only be called once and only after the end
 *       of a contact.
 */
static enum rrnd_status rrnd_update_contact_history_and_predictions(
	struct rrnd_gs_info *const station,
	const unsigned long long cur_time,
	struct vec3d (*const get_satpos)(const unsigned long long time))
{
	enum rrnd_status rc = RRND_STATUS_UNCHANGED;
	struct rrnd_validation_result vr;
	struct rrnd_contact_history *h;
	bool last_contact_successful = false;

	if (!HAS_FLAG(station->flags, RRND_GS_FLAG_LOCATION_VALID))
		return RRND_STATUS_UNCHANGED;
	vr = rrnd_validator_validate_contact(
		station->last_contact_start_time,
		station->last_contact_end_time,
		station->location,
		&station->predictions,
		get_satpos
	);
	h = &station->contact_history;
	/* NOTE We do not fail here as it was the _last_ contact which failed */
	if (vr.prediction != -1) {
		/* Append match to history */
		append_to_history(h, vr.match, station->last_mean_tx_bitrate);
		/* NOTE: Removing prediction from list is done by cleanup */
		rc |= RRND_STATUS_UPDATED | RRND_UPDATE_CONTACTS;
		last_contact_successful = true;
	}
	rc |= cleanup_predictions(
		&station->predictions,
		&station->contact_history,
		&station->prob_metrics,
		cur_time,
		last_contact_successful
	);
	if (vr.prediction == -1) {
		// There was no matching prediction (e.g. not requested).
		if (vr.success) {
			// If the contact was validated successfully, add the
			// validation result to the end of the history.
			h->failed_contacts_bitmap <<= 1;
			h->failed_contacts_bitmap |= 0;
			if (h->contacts_in_bitmap <
			    RRND_CONTACT_SUCCESS_HISTORY)
				h->contacts_in_bitmap++;
			rrnd_estimator_update_probability_metric_after_contact(
				&station->prob_metrics,
				&station->contact_history,
				true
			);
			rc |= RRND_STATUS_UPDATED;
		} else {
			// If the contact was not expected, the station may be
			// moving or have a wrong location.
			// TODO: Decrease trustworthiness or increase an
			// "invalid contacts" counter!
		}
	}
	return rc;
}

/* Helpers */

/*
 * Check whether the "currently running" contact has ended (via beacon timeout)
 * and we're dealing with a new one now.
 */
static int out_of_last_contact(
	struct rrnd_gs_info *const station, const unsigned long long time)
{
	return (
		time > station->last_contact_end_time &&
		(time - station->last_contact_end_time)
			> RRND_BEACON_MAX_WAIT
	);
}

/*
 * Reads available information from the specified beacon and populates the
 * GS data structure with it.
 */
static void update_station_from_beacon(
	struct rrnd_gs_info *const station,
	const struct rrnd_beacon *const beacon)
{
	station->flags |= (beacon->flags & 0x0F);
	station->tx_bitrate = beacon->tx_bitrate;
	station->rx_bitrate = beacon->rx_bitrate;
	station->last_mean_tx_bitrate = beacon->rx_bitrate;
	station->beacon_period = beacon->period;
}

/*
 * Returns an approximation for the Earth radius at the location of the
 * specified ground station. If its location is valid, an elliptical model
 * is used. Else, the default radius is returned.
 */
static double get_earth_radius(const struct rrnd_gs_info *const station)
{
	double Rearth = RRND_EARTH_RADIUS;

	if (HAS_FLAG(station->flags, RRND_GS_FLAG_LOCATION_VALID)) {
		Rearth = coord_earth_radius_at(station->location);
		/* max diff. from mean radius is around 15 km */
		if (ABS(Rearth - RRND_EARTH_RADIUS) > 16)
			Rearth = RRND_EARTH_RADIUS;
	}
	return Rearth;
}

/*
 * Appends the grade of "matching" between contacts and corresponding
 * predictions to the history to allow determining the reliability of
 * predictions later.
 * NOTE: Only matches greater than RRND_VALIDATOR_MIN_VALIDATION_MATCH
 *       will be added here!
 */
static void append_to_history(struct rrnd_contact_history *const h,
	const float match, const unsigned long br)
{
	if (h->elements < RRND_CONTACT_HISTORY_LENGTH)
		h->elements++;
	// TODO: if this is used for capacity calculation in the estimator,
	// it should probably relfect only a ratio of the duration of the
	// prediction in respect to the duration of the observed contact.
	h->prediction_reliability[h->index] = match;
	h->tx_bitrate[h->index] = br;
	h->index++;
	if (h->index == RRND_CONTACT_HISTORY_LENGTH)
		h->index = 0;
}

/*
 * Updates the TX (mean) and RX (max) bit rate estimations stored with the GS.
 */
static void update_bitrate_estimation(
	struct rrnd_gs_info *const station,
	const unsigned long our_estimation, const unsigned long gs_estimation)
{
	unsigned long est = our_estimation, br;
	/* Should never get 0 */
	unsigned short n_beacon = station->beacon_count + 1;

	if (our_estimation == 0 || our_estimation == ULONG_MAX)
		est = gs_estimation;
	/* Online Mean Value */
	br = station->last_mean_tx_bitrate;
	br = (est + (n_beacon - 1) * br) / n_beacon;
	station->last_mean_tx_bitrate = br;
	/* Max RX estimation from GS */
	station->rx_bitrate = MAX(station->rx_bitrate, gs_estimation);
}

/*
 * Calculates the "grade" of intersection between two intervals.
 * If start and end times are completely identical, this returns 1.0.
 * Conversely, if there is no intersection of intervals, 0.0 is returned.
 */
float rrnd_interval_intersect(
	const unsigned long long orig_start, const unsigned long long orig_end,
	const unsigned long long new_start, const unsigned long long new_end)
{
	unsigned long long diff;

	if (
		orig_start >= new_end ||
		new_start >= orig_end ||
		orig_end <= orig_start ||
		new_end <= new_start
	) {
		return 0.0f;
	}
	if (orig_start <= new_start) {
		if (new_end >= orig_end)
			diff = orig_end - new_start;
		else
			diff = new_end - new_start;
	} else {
		if (orig_end >= new_end)
			diff = new_end - orig_start;
		else
			diff = orig_end - orig_start;
	}
	return (float)diff / (float)(orig_end - orig_start);
}
