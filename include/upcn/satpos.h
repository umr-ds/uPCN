#ifndef SATPOS_H_INCLUDED
#define SATPOS_H_INCLUDED

#include "drv/vec3d.h"

enum satpos_result {
	SATPOS_SUCCESS = 0,
	SATPOS_FAILURE = 1
};

void satpos_init(const char *const tle, const unsigned long long time_ms);
struct vec3d satpos_get(const unsigned long long unix_ms);
enum satpos_result satpos_last_result(void);
unsigned long long satpos_get_age(const unsigned long long time_ms);

#endif /* SATPOS_H_INCLUDED */
