/*
 * A C-port derived from the JS "geodesy" library
 * https://github.com/chrisveness/geodesy/blob/master/latlon-ellipsoidal.js
 *
 * Original implementation Copyright (c) 2014 Chris Veness
 * License: MIT License
 * https://github.com/chrisveness/geodesy/blob/master/LICENSE
 *
 * Port by Felix Walter (c) 2016
 */

#ifndef ELLIPSOIDAL_COORDS_H_INCLUDED
#define ELLIPSOIDAL_COORDS_H_INCLUDED

struct vec3d coord_ecef2llh(const struct vec3d ecef);
struct vec3d coord_llh2enu(const struct vec3d llh, const struct vec3d unit_vec);
struct vec3d coord_ecef2ead(
    const struct vec3d sat_ecef, const struct vec3d gs_ecef);
double coord_earth_radius_at(const struct vec3d ecef);

#endif /* ELLIPSOIDAL_COORDS_H_INCLUDED */
