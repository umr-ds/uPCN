#include <string.h>
#include <math.h>
#include <float.h>

#include "upcn/rrnd.h"
#include "upcn/satpos.h"
#include "drv/vec3d.h"
#include "drv/trilat.h"
#include "drv/ellipsoidal_coords.h"
#include "drv/mpfit.h"

static double calculate_dist(struct vec3d satpos,
	const double earth_radius, const double start_elevation);

static enum rrnd_rc calculate_math_trilat(struct vec3d *const result,
	const struct vec3d p1, const double d1,
	const struct vec3d p2, const double d2,
	const struct vec3d p3, const double d3,
	const double earth_radius);

static struct vec3d vec_with_length(
	const struct vec3d a, const struct vec3d b, const double length);

#ifdef RRND_TRILATERATION_MPFIT

static enum rrnd_rc locate_mpfit(struct vec3d *const result,
	struct vec3d *const pos, double *const dist, const unsigned char count);

static enum rrnd_rc mpfit_point(
	struct vec3d *const result,
	struct vec3d *const pos, double *const dist,
	const unsigned char elements, const double earth_radius);
#define FIT_POINT mpfit_point
#define USE_MPFIT 1

#else /* RRND_TRILATERATION_MPFIT */

static enum rrnd_rc trilaterate_point(
	struct vec3d *const result,
	const struct vec3d *const pos, const double *const dist,
	const unsigned char elements, const double earth_radius);
#define FIT_POINT trilaterate_point
#define USE_MPFIT 0

#endif /* RRND_TRILATERATION_MPFIT */

enum rrnd_rc rrnd_locator_add(
	struct rrnd_locator_data *const data, const struct vec3d satpos)
{
	data->crit_satpos[data->index++] = vec3d2f(satpos);
	if (data->index == RRND_LOCATOR_MAX_RECORDS)
		data->index = 0;
	if (data->elements < RRND_LOCATOR_MAX_RECORDS)
		data->elements++;
	data->location_up2date = 0;
	/*RRND_DBG("Added time %llu to locator\n", unix_ms / 1000);*/
	return RRND_SUCCESS;
}

#ifdef RRND_ADVANCED_ELEVATION_TRACKING

struct elevation_map_index {
	float lower_fraction, upper_fraction;
	unsigned char lower, upper;
};

static struct elevation_map_index get_elevation_map_index(
	const struct vec3d satpos, const struct vec3d gspos,
	float *const elevation_out)
{
	struct vec3d ead = coord_ecef2ead(satpos, gspos);
	float modindex;
	struct elevation_map_index result;

	if (elevation_out != NULL)
		*elevation_out = (float)ead.x;
	modindex = fmodf(
		ead.y / (2 * M_PI) * RRND_ADVANCED_ELEVATION_TRACKING + 0.5,
		RRND_ADVANCED_ELEVATION_TRACKING);
	result.lower_fraction = floorf(modindex);
	result.upper_fraction = ceilf(modindex);
	result.lower = (unsigned char)result.lower_fraction %
		RRND_ADVANCED_ELEVATION_TRACKING;
	result.upper = (unsigned char)result.upper_fraction %
		RRND_ADVANCED_ELEVATION_TRACKING;
	result.lower_fraction = 1.0f - (modindex - result.lower_fraction);
	result.upper_fraction = 1.0f - (result.upper_fraction - modindex);
	return result;
}

enum rrnd_rc rrnd_locator_update_elevation_map(
	struct rrnd_locator_data *const data,
	const struct vec3d satpos, const struct vec3d gspos)
{
	float elev;
	struct elevation_map_index ei =
		get_elevation_map_index(satpos, gspos, &elev);

	if (elev == 0.0f)
		elev = FLT_MIN;
	data->elevation_map[ei.lower] = elev * ei.lower_fraction +
		data->elevation_map[ei.lower] * (1.0f - ei.lower_fraction);
	data->elevation_map[ei.upper] = elev * ei.upper_fraction +
		data->elevation_map[ei.upper] * (1.0f - ei.upper_fraction);
	return RRND_SUCCESS;
}

#endif /* RRND_ADVANCED_ELEVATION_TRACKING */

enum rrnd_rc rrnd_locator_locate(
	struct vec3d *const result, const unsigned char valid_in,
	struct rrnd_locator_data *const data,
	const double earth_radius, const double start_elevation)
{
	int i;
	struct vec3d pos[RRND_LOCATOR_MAX_RECORDS];
	double dist[RRND_LOCATOR_MAX_RECORDS];

	if (valid_in && data->location_up2date)
		return RRND_SUCCESS;
	if (data->elements < 3)
		return RRND_FAILURE;
	/* If valid_in is specified, it will serve as initial "guess" */
	if (USE_MPFIT && !valid_in)
		*result = vec3d_null();
	for (i = 0; i < data->elements; i++) {
		/* NOTE: We might do this incrementally in rrnd_locator_add! */
		pos[i] = vec3f2d(data->crit_satpos[i]);
		dist[i] = calculate_dist(pos[i], earth_radius, start_elevation);
		/* 90 degree case */
		/*dist[i] = vec3d_norm(pos[i]) * vec3d_norm(pos[i]);*/
		/*dist[i] = sqrt(dist[i] - earth_radius * earth_radius);*/
		if (!isnormal(dist[i]))
			return RRND_FAILURE;
		if (USE_MPFIT && !valid_in)
			*result = vec3d_sum(*result, pos[i]);
	}
	/* NOTE *result := initial "guess" which is the mean of pos[] */
	if (USE_MPFIT && !valid_in)
		*result = vec3d_div(*result, data->elements);
	if (FIT_POINT(result, pos, dist, data->elements, earth_radius)
			!= RRND_SUCCESS)
		return RRND_FAILURE;
	/* Earth radius correction */
	*result = vec3d_mul(*result, earth_radius / vec3d_norm(*result));
	data->location_up2date = 1;
	RRND_DBG("Found location: %f; %f; %f; NORM = %f\n",
		result->x, result->y, result->z, vec3d_norm(*result));
	return RRND_SUCCESS;
}

#ifdef RRND_PREDICTION_ESTIMATE_START_ELEVATION
static double get_mean_start_elevation(
	const struct rrnd_locator_data *const data,
	const struct vec3d gspos, const int count)
{
	int i;
	double elev_mean;

	if (data->elements < count)
		return NAN;
	if (count <= 0)
		return RRND_START_ELEVATION;
	i = ((int)data->index - count) % RRND_LOCATOR_MAX_RECORDS;
	if (i < 0) /* positive modulo */
		i += RRND_LOCATOR_MAX_RECORDS;
	elev_mean = 0.0;
	while (i != data->index) {
		elev_mean += rrnd_get_elevation(
			vec3f2d(data->crit_satpos[i]), gspos);
		i++;
		if (i == RRND_LOCATOR_MAX_RECORDS)
			i = 0;
	}
	return elev_mean / count;
}
#endif /* RRND_PREDICTION_ESTIMATE_START_ELEVATION */

#ifdef RRND_ADVANCED_ELEVATION_TRACKING
static double get_adv_tracked_start_elevation(
	const struct rrnd_locator_data *const data,
	const struct vec3d gspos, const struct vec3d satpos)
{
	float lower, upper;
	struct elevation_map_index ei =
		get_elevation_map_index(satpos, gspos, NULL);

	lower = data->elevation_map[ei.lower];
	upper = data->elevation_map[ei.upper];
	/* NOTE: 0.0 is uninitialized, FLT_MIN is init. zero */
	if (lower == 0.0f || upper == 0.0f)
		return NAN; /* TODO: Or do we want to estimate otherwise? */
	return lower * ei.lower_fraction + upper * ei.upper_fraction;
}
#endif /*RRND_ADVANCED_ELEVATION_TRACKING*/

double rrnd_locator_estimate_start_elevation(
	const struct rrnd_locator_data *const data,
	const struct vec3d gspos, const struct vec3d satpos)
{
	double elev;

#ifdef RRND_ADVANCED_ELEVATION_TRACKING
	if (!vec3d_isnull(satpos) && !vec3d_isnull(gspos)) {
		elev = get_adv_tracked_start_elevation(data, satpos, gspos);
		if (!isnan(elev))
			return elev;
	}
#endif /*RRND_ADVANCED_ELEVATION_TRACKING*/
#ifdef RRND_PREDICTION_ESTIMATE_START_ELEVATION
	if (!vec3d_isnull(gspos)) {
		elev = get_mean_start_elevation(
			data, gspos, RRND_PREDICTION_ESTIMATE_START_ELEVATION);
		if (!isnan(elev) &&
				elev >= RRND_VALIDATOR_MIN_VALIDATION_ELEVATION)
			return elev;
	}
#endif /* RRND_PREDICTION_ESTIMATE_START_ELEVATION */
	(void)elev;
	return RRND_START_ELEVATION;
}

static double calculate_dist(const struct vec3d satpos,
	const double earth_radius, const double start_elevation)
{
	double sat_height = vec3d_norm(satpos) - earth_radius;

	return sqrt(
		sat_height * sat_height +
		2.0 * earth_radius * (sat_height - start_elevation));
}

static enum rrnd_rc calculate_math_trilat(struct vec3d *const result,
	const struct vec3d p1, const double d1,
	const struct vec3d p2, const double d2,
	const struct vec3d p3, const double d3,
	const double earth_radius)
{
	struct vec3d r1, r2;
	double de1, de2, maxzero;

	maxzero = MIN(d1, MIN(d2, d3)) * 1e-5; /* 1e-5 should lead to < 10m */
	if (trilateration(&r1, &r2, p1, d1, p2, d2, p3, d3, maxzero) != 0)
		return RRND_FAILURE;
	de1 = vec3d_norm(r1) - earth_radius;
	de2 = vec3d_norm(r2) - earth_radius;
	/*RRND_DBG("R1 [%f, %f, %f]\n", r1.x, r1.y, r1.z);*/
	/*RRND_DBG("R2 [%f, %f, %f]\n", r2.x, r2.y, r2.z);*/
	if (SIGN(de1) != SIGN(de2))
		*result = vec_with_length(r1, r2, earth_radius);
	else if (ABS(de1) < ABS(de2))
		*result = r1;
	else
		*result = r2;
	/*RRND_DBG("Found vec [%f, %f, %f] LEN = %f\n",*/
	/*	result->x, result->y, result->z, vec3d_norm(*result));*/
	/*RRND_DBG("D1: %f, D2: %f, MID: %f\n", de1, de2, ABS(de1 + de2));*/
	return RRND_SUCCESS;
}

/* TODO: This is a bit dirty */
/* Get the vector with length "length" between a and b */
static struct vec3d vec_with_length(
	const struct vec3d a, const struct vec3d b, const double length)
{
	struct vec3d b_m_a = vec3d_diff(b, a), res;
	double l_a = vec3d_norm(a);
	double l_b_m_a = vec3d_norm(b_m_a);
	double cos_alpha, p, q, t;

	b_m_a = vec3d_div(b_m_a, l_b_m_a);
	cos_alpha = vec3d_dot(b_m_a, vec3d_div(a, l_a));
	p = -2.0 * l_a * cos_alpha;
	q = l_a * l_a - length * length;
	t = -(p / 2.0) - sqrt((p * p / 4.0) - q);
	res = vec3d_sum(a, vec3d_mul(b_m_a, ABS(t)));
	if (ABS(res.x - a.x) > ABS(res.x))
		res = vec3d_mul(res, -1.0);
	return res;
}

#ifdef RRND_TRILATERATION_MPFIT

struct privdata {
	struct vec3d *pos;
	double *dist;
};

static int fit_func(int m, int n, double *x, double *fvec,
	double **dvec, void *private_data)
{
	struct vec3d *pos = ((struct privdata *)private_data)->pos;
	double *dist = ((struct privdata *)private_data)->dist;
	int i;

	for (i = 0; i < m; i++) {
		fvec[i] = dist[i] - sqrt(
			(pos[i].x - x[0]) * (pos[i].x - x[0]) +
			(pos[i].y - x[1]) * (pos[i].y - x[1]) +
			(pos[i].z - x[2]) * (pos[i].z - x[2]));
	}
	return 0;
}

/*
 * See also
 * --------
 * SE:  http://gis.stackexchange.com/questions/40660/
 *      trilateration-algorithm-for-n-amount-of-points
 * Lib: http://www.physics.wisc.edu/~craigm/idl/cmpfit.html
 * Ex.: http://josephmeiring.github.io/jsfit/#/examples/5
 * Pp.: http://www.mines.edu/~whereman/papers/
 *      Murphy-Hereman-Trilateration-1995.pdf
 */
static enum rrnd_rc locate_mpfit(struct vec3d *const result,
	struct vec3d *const pos, double *const dist, const unsigned char count)
{
	struct privdata pd = {pos, dist};

	/* TODO: This returns an accuracy value. Use it for prob.det.! */
	return (mpfit(fit_func, count, 3, (double *)result,
		NULL, NULL, &pd, NULL) > 0) ? RRND_SUCCESS : RRND_FAILURE;
}

static enum rrnd_rc mpfit_point(
	struct vec3d *const result,
	struct vec3d *const pos, double *const dist,
	const unsigned char elements, const double earth_radius)
{
	if (elements == 3) {
		if (calculate_math_trilat(result,
				pos[0], dist[0], pos[1], dist[1],
				pos[2], dist[2], earth_radius)
					!= RRND_SUCCESS)
			return RRND_FAILURE;
	} else if (locate_mpfit(result, pos, dist, elements)
			!= RRND_SUCCESS) {
		return RRND_FAILURE;
	}
	return RRND_SUCCESS;
}

#else /* RRND_TRILATERATION_MPFIT */

static enum rrnd_rc trilaterate_point(
	struct vec3d *const result,
	const struct vec3d *const pos, const double *const dist,
	const unsigned char elements, const double earth_radius)
{
	int i, c = 0;
	struct vec3d rcur, rsum = vec3d_null();

	for (i = 0; i < data->elements - 2; i++) {
		if (calculate_math_trilat(&rcur,
				pos[i], dist[i], pos[i + 1], dist[i + 1],
				pos[i + 2], dist[i + 2], earth_radius)
					!= RRND_SUCCESS)
			continue;
		rsum = vec3d_sum(rsum, rcur);
		c++;
	}
	if (c == 0)
		return RRND_FAILURE;
	*result = vec3d_div(rsum, c);
	return RRND_SUCCESS;
}

#endif /* RRND_TRILATERATION_MPFIT */
