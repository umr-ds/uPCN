#include "upcn/rrnd.h"

enum rrnd_rc rrnd_availability_pattern_update(
	struct rrnd_availability_pattern *const pattern,
	const unsigned long long time, const unsigned long on_time,
	const unsigned long long last_contact_end, const unsigned char new_c)
{
	unsigned long n_on, n_period, n_offset;

	n_on = MAX(pattern->on_time, on_time);
	if (!new_c || on_time >= pattern->last_on_time) {
		pattern->last_on_time = on_time;
		if (n_on > pattern->period && pattern->period != 0)
			goto invalidate;
		else
			pattern->on_time = n_on;
		return RRND_SUCCESS;
	}
	n_offset = time - on_time;
	if (last_contact_end >= n_offset)
		goto invalidate;
	n_period = n_offset - last_contact_end + pattern->last_on_time;
	if (n_period == 0 || n_on > n_period) /* NOTE Reuse? */
		goto invalidate;
	if (pattern->period != 0 && n_period > pattern->period &&
			(n_period % pattern->period) < RRND_AP_MAX_DEVI)
		n_period = pattern->period;
	n_offset = n_offset % n_period;
	pattern->significant_changes <<= 1;
	if (pattern->valid &&
			rrnd_interval_intersect(pattern->offset,
				pattern->offset + pattern->on_time,
				n_offset, n_offset + n_on)
			< RRND_AP_SIG_THRESHOLD)
		pattern->significant_changes |= 1;
	pattern->valid = 1;
	pattern->contacts_honored++;
	pattern->on_time = n_on;
	pattern->period = n_period;
	pattern->offset = n_offset;
	pattern->last_on_time = on_time;
	return RRND_SUCCESS;
invalidate:
	RRND_DBG("Invalidating availability pattern\n");
	pattern->on_time = 0;
	pattern->contacts_honored = 0;
	pattern->valid = 0;
	return RRND_SUCCESS;
}

unsigned char rrnd_availability_pattern_get_sig_changes(
	const struct rrnd_availability_pattern *const pattern)
{
	return __builtin_popcount(pattern->significant_changes <<
		(pattern->contacts_honored -
			RRND_AVAILABILITY_PATTERN_CHANGE_HISTORY));
}

/*
 * NOTE:
 * This assumes that the patterns on-time is alsways longer
 * than the duration of the prediction.
 */
enum rrnd_rc rrnd_availability_pattern_intersect(
	struct rrnd_contact_info *const prediction,
	const struct rrnd_availability_pattern *const pattern)
{
	unsigned long long rel_start, rel_end, on_start, on_end;

	if (rrnd_estimator_estimate_avail_accuracy(pattern)
			< RRND_AP_INTER_MIN_PROBABILITY || pattern->period == 0)
		return RRND_SUCCESS; /* XXX Really success? */
	rel_start = prediction->start % pattern->period;
	rel_end = rel_start + prediction->duration;
	on_start = pattern->offset;
	on_end = pattern->offset + pattern->on_time;
	if (rel_start > on_end || rel_end < on_start)
		return RRND_FAILURE;
	if (rel_start < on_start)
		rel_start = on_start;
	if (rel_end > on_end)
		rel_end = on_end;
	if (rel_end <= rel_start)
		return RRND_FAILURE;
	prediction->start = rel_start +
		pattern->period * (prediction->start / pattern->period);
	prediction->duration = rel_end - rel_start;
	return RRND_SUCCESS;
}
