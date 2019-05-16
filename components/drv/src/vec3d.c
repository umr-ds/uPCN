/*
 * Felix Walter, 2016, BSD3-licensed (see uPCN)
 *
 * Partly taken from public domain source:
 * https://en.wikipedia.org/wiki/Talk%3ATrilateration#Example_C_program
 * by https://en.wikipedia.org/wiki/User:Nominal_animal
 */

#include <math.h>

#include "drv/vec3d.h"

/* Return the difference of two vectors, (vector1 - vector2). */
struct vec3d vec3d_diff(const struct vec3d vector1, const struct vec3d vector2)
{
	struct vec3d v;
	v.x = vector1.x - vector2.x;
	v.y = vector1.y - vector2.y;
	v.z = vector1.z - vector2.z;
	return v;
}

/* Return the sum of two vectors. */
struct vec3d vec3d_sum(const struct vec3d vector1, const struct vec3d vector2)
{
	struct vec3d v;
	v.x = vector1.x + vector2.x;
	v.y = vector1.y + vector2.y;
	v.z = vector1.z + vector2.z;
	return v;
}

/* Multiply vector by a number. */
struct vec3d vec3d_mul(const struct vec3d vector, const double n)
{
	struct vec3d v;
	v.x = vector.x * n;
	v.y = vector.y * n;
	v.z = vector.z * n;
	return v;
}

/* Divide vector by a number. */
struct vec3d vec3d_div(const struct vec3d vector, const double n)
{
	struct vec3d v;
	v.x = vector.x / n;
	v.y = vector.y / n;
	v.z = vector.z / n;
	return v;
}

/* Return the Euclidean norm. */
double vec3d_norm(const struct vec3d vector)
{
	return sqrt(
		vector.x * vector.x +
		vector.y * vector.y +
		vector.z * vector.z);
}

/* Return the dot product of two vectors. */
double vec3d_dot(const struct vec3d vector1, const struct vec3d vector2)
{
	return (vector1.x * vector2.x +
		vector1.y * vector2.y +
		vector1.z * vector2.z);
}

/* Replace vector with its cross product with another vector. */
struct vec3d vec3d_cross(
	const struct vec3d vector1, const struct vec3d vector2)
{
	struct vec3d v;
	v.x = vector1.y * vector2.z - vector1.z * vector2.y;
	v.y = vector1.z * vector2.x - vector1.x * vector2.z;
	v.z = vector1.x * vector2.y - vector1.y * vector2.x;
	return v;
}
