#include <stdlib.h>
#include <limits.h>

#include "drv/vec3d.h"
#include "drv/brent.h"

#include "upcn/rrnd.h"

static unsigned long long get_next_elevation_maximum(
	const unsigned long long start_time, const unsigned long step,
	const struct vec3d gspos,
	struct vec3d (*const get_satpos)(const unsigned long long time));

static unsigned long long get_zero_within(
	const unsigned long long start_time, const unsigned long step,
	const struct vec3d gspos,
	const struct rrnd_locator_data *const locator_data,
	struct vec3d (*const get_satpos)(const unsigned long long time));

enum rrnd_rc rrnd_predictor_predict(struct rrnd_contact_info *const result,
	const struct vec3d gspos, const unsigned long long start_time,
	const unsigned long min_duration, const unsigned long step,
	const struct rrnd_locator_data *const locator_data,
	const struct rrnd_availability_pattern *const avail_pattern,
	struct vec3d (*const get_satpos)(const unsigned long long time))
{
	int i;
	unsigned long long cur_time = start_time, max_time, zero_l, zero_r;
	double cur_elev;

	for (i = 0; i < RRND_PREDICTOR_MAX_STEPS; i++, cur_time += step) {
		max_time = get_next_elevation_maximum(
			cur_time, step, gspos, get_satpos);
		if (max_time == ULLONG_MAX)
			continue;
		cur_elev = rrnd_get_elevation(get_satpos(max_time), gspos);
		if (cur_elev <= RRND_START_ELEVATION)
			continue;
		zero_l = get_zero_within(max_time - step / 2, step / 2,
			gspos, locator_data, get_satpos);
		if (zero_l == ULLONG_MAX)
			continue;
		zero_r = get_zero_within(max_time, step / 2,
			gspos, locator_data, get_satpos);
		if (zero_r == ULLONG_MAX)
			continue;
		if (zero_r <= zero_l ||
				(unsigned long)(zero_r - zero_l) < min_duration)
			continue;
		result->start = zero_l;
		result->duration = zero_r - zero_l;
		if (avail_pattern != NULL) {
			if (rrnd_availability_pattern_intersect(
						result, avail_pattern)
					!= RRND_SUCCESS)
				continue;
			if (result->duration < min_duration)
				continue;
		}
		result->max_elevation = (float)cur_elev;
		RRND_DBG("Predicted contact @ %llu - %llu\n",
			result->start / 1000,
			(result->start + result->duration) / 1000);
		return RRND_SUCCESS;
	}
	return RRND_FAILURE;
}

/*
 * Calculates e := elevation of sat. above horizon of gs
 *
 * s := distance between center of earth and sat
 * g := distance between center of earth and gs
 * d := distance between sat and gs
 * alpha := angle between horizon and sat-gs-line (= altitude)
 *
 * sin(alpha) = e / d
 * s^2 = d^2 + g^2 - 2dg*cos(90+alpha)
 *     = d^2 + g^2 + 2dg*sin(alpha)
 *     = d^2 + g^2 + 2ge
 * e = (s^2 - d^2 - g^2) / (2g)
 */
double rrnd_get_elevation(
	const struct vec3d satpos, const struct vec3d gspos)
{
	double s = vec3d_norm(satpos);
	double g = vec3d_norm(gspos);
	double d = vec3d_norm(vec3d_diff(satpos, gspos));

	return (s * s - d * d - g * g) / (2 * g);
}

/*
 * Searches the next maximum of the elevation(time) function
 * in interval [start_time, start_time + step]
 * Step should be equal to around 1 orbital period.
 */
static unsigned long long get_next_elevation_maximum(
	const unsigned long long start_time, const unsigned long step,
	const struct vec3d gspos,
	struct vec3d (*const get_satpos)(const unsigned long long time))
{
	int status = 0;
	double a, b, cur_elev, cur_time;

	a = 0.0;
	b = (double)step;
	cur_time = brent_local_min_rc(&a, &b, &status, 0.0);
	while (status > 0) {
		cur_elev = rrnd_get_elevation(
			get_satpos((unsigned long long)cur_time + start_time),
			gspos);
		cur_time = brent_local_min_rc(&a, &b, &status, -cur_elev);
	}
	if (status != 0)
		return ULLONG_MAX;
	else
		return (unsigned long long)cur_time + start_time;
}

/*
 * Searches the root of the elevation(time) function
 * in interval [start_time, start_time + step]
 * Step should be equal to around 0.5 orbital periods.
 */
static unsigned long long get_zero_within(
	const unsigned long long start_time, const unsigned long step,
	const struct vec3d gspos,
	const struct rrnd_locator_data *const locator_data,
	struct vec3d (*const get_satpos)(const unsigned long long time))
{
	int status = 0;
	double a, b, t, cur_elev, cur_time;
	struct vec3d satpos;

	a = 0.0;
	b = (double)step;
	t = 1.49e-7; /* ~ 10 * sqrt(EPSILON) */
	brent_zero_rc(a, b, t, &cur_time, &status, 0.0);
	while (status > 0) {
		satpos = get_satpos((unsigned long long)cur_time + start_time);
		cur_elev = (
			rrnd_get_elevation(satpos, gspos) -
			rrnd_locator_estimate_start_elevation(
				locator_data, gspos, satpos));
		brent_zero_rc(a, b, t, &cur_time, &status, cur_elev);
	}
	if (status != 0)
		return ULLONG_MAX;
	else
		return (unsigned long long)cur_time + start_time;
}
