#include <string.h>
#include <math.h>

#include "upcn/satpos.h"
#include "upcn/satpos_sgp4.h"

#include "drv/sgp4io.h"

#define SATPOS_GRAVCONST wgs72
#define SATPOS_OPMODE 'i'

static char init_success;
static struct sgp4_data satpos_data;
static enum satpos_result last_result;
static unsigned long long time;

void satpos_init(const char *const tle, const unsigned long long time_ms)
{
	last_result = satpos_sgp4_init_from_tle(
		&satpos_data, tle, SATPOS_GRAVCONST, SATPOS_OPMODE);
	time = time_ms;
	init_success = (last_result == SATPOS_SUCCESS);
}

struct vec3d satpos_get(const unsigned long long unix_ms)
{
	struct vec3d res;

	_Static_assert(
		sizeof(struct vec3d) == (sizeof(struct satpos_sgp4) / 2),
		"size of satpos_sgp4 structure wrong");
	if (!init_success)
		return (struct vec3d){NAN, NAN, NAN};
	last_result = satpos_sgp4_propagate_to(&satpos_data, unix_ms);
	memcpy(&res, &satpos_data.cur_pos.ro, sizeof(struct vec3d));
	satpos_sgp4_pos_eci2ecf((double *)&res, unix_ms);
	return res;
}

enum satpos_result satpos_last_result(void)
{
	return last_result;
}

unsigned long long satpos_get_age(const unsigned long long time_ms)
{
	return time_ms - time;
}
