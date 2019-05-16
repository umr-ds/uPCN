/*
 * Trilateration algorithm, taken from
 * https://en.wikipedia.org/wiki/Talk%3ATrilateration#Example_C_program
 * by https://en.wikipedia.org/wiki/User:Nominal_animal
 */

#ifndef TRILAT_H_INCLUDED
#define TRILAT_H_INCLUDED

#include "drv/vec3d.h"

/* Return zero if successful, negative error otherwise.
 * The last parameter is the largest nonnegative number considered zero;
 * it is somewhat analoguous to machine epsilon (but inclusive).
*/
int trilateration(struct vec3d *const result1, struct vec3d *const result2,
                  const struct vec3d p1, const double r1,
                  const struct vec3d p2, const double r2,
                  const struct vec3d p3, const double r3,
                  const double maxzero);

#endif /* TRILAT_H_INCLUDED */
