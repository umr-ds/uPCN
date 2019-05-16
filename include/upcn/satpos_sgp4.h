#ifndef SATPOS_SGP4_H_INCLUDED
#define SATPOS_SGP4_H_INCLUDED

#include "upcn/satpos.h"
#include "drv/sgp4io.h"

struct satpos_sgp4 {
	double ro[3];
	double vo[3];
};

struct sgp4_data {
	gravconsttype gravconst;
	elsetrec satrec;
	struct satpos_sgp4 cur_pos;
};

enum satpos_result satpos_sgp4_init_from_tle(
	struct sgp4_data *const data, const char *const tle,
	const gravconsttype gravconst, const char opmode);
enum satpos_result satpos_sgp4_propagate_to(
	struct sgp4_data *const data, const unsigned long long unix_ms);
void satpos_sgp4_pos_eci2ecf(
	double *const pos, const unsigned long long unix_ms);

#endif /* SATPOS_SGP4_H_INCLUDED */
