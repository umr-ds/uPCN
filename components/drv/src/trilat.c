/*
 * Trilateration algorithm, taken from
 * https://en.wikipedia.org/wiki/Talk%3ATrilateration#Example_C_program
 * by https://en.wikipedia.org/wiki/User:Nominal_animal
 */

#include <math.h>

#include "drv/trilat.h"

/*
 * No rights reserved (CC0, see http://wiki.creativecommons.org/CC0_FAQ).
 * The author has waived all copyright and related or neighboring rights
 * to this program, to the fullest extent possible under law.
 */

#define vsum vec3d_sum
#define vdiff vec3d_diff
#define vmul vec3d_mul
#define vdiv vec3d_div
#define vnorm vec3d_norm
#define dot vec3d_dot
#define cross vec3d_cross

/* Return zero if successful, negative error otherwise.
 * The last parameter is the largest nonnegative number considered zero;
 * it is somewhat analoguous to machine epsilon (but inclusive).
*/
int trilateration(struct vec3d *const result1, struct vec3d *const result2,
                  const struct vec3d p1, const double r1,
                  const struct vec3d p2, const double r2,
                  const struct vec3d p3, const double r3,
                  const double maxzero)
{
	struct vec3d ex, ey, ez, t1, t2;
	double h, i, j, x, y, z, t;

	/* h = |p2 - p1|, ex = (p2 - p1) / |p2 - p1| */
	ex = vdiff(p2, p1);
	h = vnorm(ex);
	if (h <= maxzero) {
		/* p1 and p2 are concentric. */
		return -1;
	}
	ex = vdiv(ex, h);

	/* t1 = p3 - p1, t2 = ex (ex . (p3 - p1)) */
	t1 = vdiff(p3, p1);
	i = dot(ex, t1);
	t2 = vmul(ex, i);

	/* ey = (t1 - t2), t = |t1 - t2| */
	ey = vdiff(t1, t2);
	t = vnorm(ey);
	if (t > maxzero) {
		/* ey = (t1 - t2) / |t1 - t2| */
		ey = vdiv(ey, t);

		/* j = ey . (p3 - p1) */
		j = dot(ey, t1);
	} else
		j = 0.0;

	/* Note: t <= maxzero implies j = 0.0. */
	if (fabs(j) <= maxzero) {
		/* p1, p2 and p3 are colinear. */

		/* Is point p1 + (r1 along the axis) the intersection? */
		t2 = vsum(p1, vmul(ex, r1));
		if (fabs(vnorm(vdiff(p2, t2)) - r2) <= maxzero &&
		    fabs(vnorm(vdiff(p3, t2)) - r3) <= maxzero) {
			/* Yes, t2 is the only intersection point. */
			if (result1)
				*result1 = t2;
			if (result2)
				*result2 = t2;
			return 0;
		}

		/* Is point p1 - (r1 along the axis) the intersection? */
		t2 = vsum(p1, vmul(ex, -r1));
		if (fabs(vnorm(vdiff(p2, t2)) - r2) <= maxzero &&
		    fabs(vnorm(vdiff(p3, t2)) - r3) <= maxzero) {
			/* Yes, t2 is the only intersection point. */
			if (result1)
				*result1 = t2;
			if (result2)
				*result2 = t2;
			return 0;
		}

		return -2;
	}

	/* ez = ex x ey */
	ez = cross(ex, ey);

	x = (r1*r1 - r2*r2) / (2*h) + h / 2;
	y = (r1*r1 - r3*r3 + i*i) / (2*j) + j / 2 - x * i / j;
	z = r1*r1 - x*x - y*y;
	if (z < -maxzero) {
		/* The solution is invalid. */
		return -3;
	} else
	if (z > 0.0)
		z = sqrt(z);
	else
		z = 0.0;

	/* t2 = p1 + x ex + y ey */
	t2 = vsum(p1, vmul(ex, x));
	t2 = vsum(t2, vmul(ey, y));

	/* result1 = p1 + x ex + y ey + z ez */
	if (result1)
		*result1 = vsum(t2, vmul(ez, z));

	/* result1 = p1 + x ex + y ey - z ez */
	if (result2)
		*result2 = vsum(t2, vmul(ez, -z));

	return 0;
}
