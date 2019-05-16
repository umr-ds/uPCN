/*
 * Felix Walter, 2016, BSD3-licensed (see uPCN)
 *
 * Partly taken from public domain source:
 * https://en.wikipedia.org/wiki/Talk%3ATrilateration#Example_C_program
 * by https://en.wikipedia.org/wiki/User:Nominal_animal
 */

#ifndef VEC3D_H_INCLUDED
#define VEC3D_H_INCLUDED

struct vec3d {
	double x, y, z;
};

struct vec3f {
	float x, y, z;
};

#define vec3f2d(vec) ((struct vec3d){ (vec).x, (vec).y, (vec).z })
#define vec3d2f(vec) ((struct vec3f){ \
	(float)(vec).x, (float)(vec).y, (float)(vec).z })

/* Return the difference of two vectors, (vector1 - vector2). */
struct vec3d vec3d_diff(const struct vec3d vector1, const struct vec3d vector2);

/* Return the sum of two vectors. */
struct vec3d vec3d_sum(const struct vec3d vector1, const struct vec3d vector2);

/* Multiply vector by a number. */
struct vec3d vec3d_mul(const struct vec3d vector, const double n);

/* Divide vector by a number. */
struct vec3d vec3d_div(const struct vec3d vector, const double n);

/* Return the Euclidean norm. */
double vec3d_norm(const struct vec3d vector);

/* Return the dot product of two vectors. */
double vec3d_dot(const struct vec3d vector1, const struct vec3d vector2);

/* Replace vector with its cross product with another vector. */
struct vec3d vec3d_cross(
	const struct vec3d vector1, const struct vec3d vector2);

/* Get (0, 0, 0) vector */
static inline struct vec3d vec3d_null(void)
{
	static struct vec3d null;
	return null;
}

/* val == (0, 0, 0) ? */
static inline int vec3d_isnull(const struct vec3d val)
{
	return (val.x == 0.0 && val.y == 0.0 && val.z == 0.0);
}

#endif /* VEC3D_H_INCLUDED */
