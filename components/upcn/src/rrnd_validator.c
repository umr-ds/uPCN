#include "drv/vec3d.h"
#include "upcn/rrnd.h"

static enum rrnd_rc validate_elevation(
	const struct vec3d satpos, const struct vec3d gspos);

float rrnd_validator_calculate_time_match(
	const unsigned long long predicted_start,
	const unsigned long long predicted_end,
	const unsigned long long time)
{
	unsigned long long duration, diff;

	if (predicted_end <= predicted_start)
		return 0.0f;
	if (time >= predicted_start && time <= predicted_end)
		return 1.0f;
	duration = predicted_end - predicted_start;
	if (time < predicted_start)
		diff = predicted_start - time;
	else
		diff = time - predicted_end;
	if (diff >= duration)
		return 0.0f;
	return 1.0f - (float)diff / (float)duration;
}

struct rrnd_validation_result rrnd_validator_validate_contact_time(
	const unsigned long long time, const struct vec3d gspos,
	const struct rrnd_predictions *const predictions,
	struct vec3d (*const get_satpos)(const unsigned long long time))
{
	unsigned char i, c;
	float match;
	const struct rrnd_contact_info *p;
	struct rrnd_validation_result res = { .prediction = -1 };

	/* Check if predictions match */
	for (i = predictions->start, c = 0; c <= predictions->count;
	     i = (i + 1) % RRND_CONTACT_PREDICTION_LENGTH, c++) {
		p = &predictions->contacts[i];
		if (p->probability < RRND_VALIDATOR_MIN_PREDICTION_ACCURACY)
			continue;
		match = rrnd_validator_calculate_time_match(
			p->start, p->start + p->duration, time);
		if (match > RRND_VALIDATOR_MIN_VALIDATION_MATCH) {
			res.match = match;
			res.prediction = i;
			res.success = 1;
			return res;
		}
		if (p->start > time && match < 1e-3)
			break;
	}
	/* Check location if no prediction matches */
	/* TODO: Calculate match for statistic */
	res.success = (validate_elevation(get_satpos(time), gspos)
		== RRND_SUCCESS);
	return res;
}

struct rrnd_validation_result rrnd_validator_validate_contact(
	const unsigned long long start, const unsigned long long end,
	const struct vec3d gspos,
	const struct rrnd_predictions *const predictions,
	struct vec3d (*const get_satpos)(const unsigned long long time))
{
	unsigned char i, c;
	float match;
	const struct rrnd_contact_info *p;
	struct rrnd_validation_result res = { .prediction = -1 };

	/* Check if predictions match */
	for (i = predictions->start, c = 0; c <= predictions->count;
	     i = (i + 1) % RRND_CONTACT_PREDICTION_LENGTH, c++) {
		p = &predictions->contacts[i];
		if (p->probability < RRND_VALIDATOR_MIN_PREDICTION_ACCURACY) {
			RRND_DBG("Not considering contact %llu - %llu\n",
				 p->start, p->start + p->duration);
			continue;
		}
		match = rrnd_interval_intersect(
			p->start, p->start + p->duration, start, end);
		if (match > RRND_VALIDATOR_MIN_VALIDATION_MATCH) {
			res.match = match;
			res.prediction = i;
			res.success = 1;
			return res;
		}
		if (p->start > end && match < 1e-3)
			break;
	}
	/* Check location if no prediction matches */
	/* TODO: Calculate match for statistic */
	res.success =
		((validate_elevation(get_satpos(start), gspos) ==
			RRND_SUCCESS) &&
		(validate_elevation(get_satpos(end), gspos) ==
			RRND_SUCCESS));
	return res;
}

static enum rrnd_rc validate_elevation(
	const struct vec3d satpos, const struct vec3d gspos)
{
	double elev = rrnd_get_elevation(satpos, gspos);
	int res = elev > RRND_VALIDATOR_MIN_VALIDATION_ELEVATION;

	if (!res)
		RRND_DBG("Validating elevation failed: e=%f\n", elev);
	return res ? RRND_SUCCESS : RRND_FAILURE;
}
