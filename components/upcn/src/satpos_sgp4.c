#include <string.h>
#include <math.h>

#include "upcn/satpos.h"
#include "upcn/satpos_sgp4.h"

#include "drv/sgp4ext.h"
#include "drv/sgp4unit.h"
#include "drv/sgp4io.h"

#define SUCCESS SATPOS_SUCCESS
#define FAILURE SATPOS_FAILURE

struct tle {
	char l1[70];
	char l2[70];
};

/* ms since 1970-01-01 to jday */
static inline double milliunixtojd(const unsigned long long unixtime_ms)
{
	return ((double)unixtime_ms / 86.4e6) + 2440587.5;
}

static enum satpos_result split_and_validate_tle(
	struct tle *const result, const char *const tle);

enum satpos_result satpos_sgp4_init_from_tle(
	struct sgp4_data *const data, const char *const tle,
	const gravconsttype gravconst, const char opmode)
{
	struct tle tle_split;
	double tmp;

	if (split_and_validate_tle(&tle_split, tle) != SUCCESS)
		return FAILURE;
	data->gravconst = gravconst;
	twoline2rv(tle_split.l1, tle_split.l2, 0.0, 0.0, opmode,
		data->gravconst, &tmp, &tmp, &data->satrec);
	sgp4(data->gravconst, &data->satrec,  0.0,
		data->cur_pos.ro, data->cur_pos.vo);
	return (data->satrec.error > 0) ? FAILURE : SUCCESS;
}

/* Be aware that this returns ECI coordinates! */
enum satpos_result satpos_sgp4_propagate_to(
	struct sgp4_data *const data, const unsigned long long unix_ms)
{
	double jday, mfe;

	jday = milliunixtojd(unix_ms);
	mfe = (jday - data->satrec.jdsatepoch) * 1440.0;
	if (!isnormal(mfe))
		return FAILURE;
	sgp4(data->gravconst, &data->satrec, mfe,
		data->cur_pos.ro, data->cur_pos.vo);
	return (data->satrec.error > 0) ? FAILURE : SUCCESS;
}

static enum satpos_result split_and_validate_tle(
	struct tle *const result, const char *const tle)
{
	int i, c;
	const char *cur;
	char *t[2];

	t[0] = result->l1;
	t[1] = result->l2;
	cur = tle;
	for (i = 0; i < 2; i++) {
		c = 0;
		while (*cur != '\n' && *cur != '\0' && c != 69) {
			*(t[i]++) = *(cur++);
			c++;
		}
		*t[i] = '\0';
		if (c != 69)
			return FAILURE;
		cur++;
	}
	return SUCCESS;
}

/*
 * Matrix multiplication. See formulae (25) and (26):
 * http://web.archive.org/web/20111121073253/ \
 *     http://www.cdeagle.com/omnum/pdf/csystems.pdf
 */
void satpos_sgp4_pos_eci2ecf(
	double *const pos, const unsigned long long unix_ms)
{
	double theta = gstime(milliunixtojd(unix_ms));
	double s = sin(theta);
	double c = cos(theta);
	double x = 0 + c * pos[0] + s * pos[1] + 0 * pos[2];
	double y = 0 - s * pos[0] + c * pos[1] + 0 * pos[2];
	double z = 0 + 0 * pos[0] + 0 * pos[1] + 1 * pos[2];

	pos[0] = x;
	pos[1] = y;
	pos[2] = z;
}
