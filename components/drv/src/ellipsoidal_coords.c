/*
 * coord_ecef2llh is a C-port derived from the JS "geodesy" library
 * https://github.com/chrisveness/geodesy/blob/master/latlon-ellipsoidal.js
 *
 * Original implementation Copyright (c) 2014 Chris Veness
 * License: MIT License
 * https://github.com/chrisveness/geodesy/blob/master/LICENSE
 *
 * Port by Felix Walter (c) 2016
 *
 * All other functions and adaptations are part of uPCN and licensed under
 * the corresponding 3-clause BSD license.
 */

#include <math.h>
#include "drv/vec3d.h"

/* WGS-84 */
#define a 6378.137
#define b 6356.75231425

/* 1st eccentricity squared */
static const double e2 = (a * a - b * b) / (a * a);
/* 2nd eccentricity squared */
static const double eps2 = (a * a - b * b) / (b * b);

static inline double latitude(const double p, const double z)
{
	double R = sqrt(p * p + z * z); /* polar radius */

	/* parametric latitude */
	/* (Bowring eqn 17, replacing tan_beta = z·a / p·b) */
	double tan_beta = (b * z) / (a * p) * (1 + eps2 * b / R);
	double sin_beta = tan_beta / sqrt(1 + tan_beta * tan_beta);
	double cos_beta = sin_beta / tan_beta;

	/* geodetic latitude (Bowring eqn 18) */
	return atan2(z + eps2 * b * sin_beta * sin_beta * sin_beta,
		p - e2 * a * cos_beta * cos_beta * cos_beta);
}

struct vec3d coord_ecef2llh(const struct vec3d ecef)
{
	double x = ecef.x;
	double y = ecef.y;
	double z = ecef.z;

	/* distance from minor axis */
	double p = sqrt(x * x + y * y);
	double phi = latitude(p, z);

	/* longitude */
	double lamb = atan2(y, x);

	/* height above ellipsoid (Bowring eqn 7) */
	double sin_phi = sin(phi);
	double cos_phi = cos(phi);
	/* length of the normal terminated by the minor axis */
	double length = a / sqrt(1 - e2 * sin_phi * sin_phi);
	double h = p * cos_phi + z * sin_phi - (a * a / length);
	
	/* LLH is Lat, Lon, Height */
	return (struct vec3d){ phi, lamb, h };
}

struct vec3d coord_llh2enu(const struct vec3d llh, const struct vec3d unit_vec)
{
	/* ENU is East, North, Vertical */
	return (struct vec3d){
		-sin(llh.y) * unit_vec.x + cos(llh.y) * unit_vec.y,
		-cos(llh.y) * sin(llh.x) * unit_vec.x
			- sin(llh.y) * sin(llh.x) * unit_vec.y
			+ cos(llh.x) * unit_vec.z,
		cos(llh.y) * cos(llh.x) * unit_vec.x
			+ sin(llh.y) * cos(llh.x) * unit_vec.y
			+ sin(llh.x) * unit_vec.z
	};
}

/*
 * See also:
 * http://www.navipedia.net/index.php/ \
 *   Transformations_between_ECEF_and_ENU_coordinates
 */
struct vec3d coord_ecef2ead(
	const struct vec3d sat_ecef, const struct vec3d gs_ecef)
{
	struct vec3d v_gs_sat, gs_enu;
	double distance;

	/* Normalized vector from GS to Sat. */
	v_gs_sat = vec3d_diff(sat_ecef, gs_ecef);
	distance = vec3d_norm(v_gs_sat);
	v_gs_sat = vec3d_div(v_gs_sat, distance);
	/* East-North-Up coordinates from GS */
	gs_enu = coord_llh2enu(coord_ecef2llh(gs_ecef), v_gs_sat);
	/* EAD is Elevation (angular), Azimuth (angular), Distance */
	return (struct vec3d){
		M_PI / 2 - acos(gs_enu.z),
		atan2(gs_enu.x, gs_enu.y),
		distance
	};
}

double coord_earth_radius_at(const struct vec3d ecef)
{
	double x = ecef.x;
	double y = ecef.y;
	double z = ecef.z;

	/* distance from minor axis */
	double p = sqrt(x * x + y * y);
	double phi = latitude(p, z);

	/* height of ellipsoid */
	double sin_phi = sin(phi);
	double length = a / sqrt(1 - e2 * sin_phi * sin_phi);

	return a * a / length;
}
