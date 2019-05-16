#include "drv/sgp4ext.h"
#include "drv/sgp4unit.h"
#include "drv/sgp4io.h"

#include "drv/mini-printf.h"

#include "upcn/upcn.h"

#ifdef TEST_SGP4_PERFORMANCE

void sgp4testtask(void *task_params)
{
	const int RUNS = 5000;

	double ro[3];
	double vo[3];

	double sec,  jd, tsince, jdstart, jdstop, startmfe, stopmfe, deltamin;
	int  year; int mon; int day; int hr; int min;
	char longstr1[130];
	char longstr2[130];
	int ctr = 0;
	uint64_t time_m, cyc;
	elsetrec satrec;

	hal_debug_printf("SGP4 version: %s\n", SGP4Version);

	hal_task_suspend_scheduler();
	time_m = hal_time_get_timestamp_us();
	(*((uint32_t *)0xE0001000)) = 0x40000001UL;
	cyc = (*((uint32_t *)0xE0001004));

	/* SOMP data from 14/11/2015 */
	mini_snprintf(longstr1, 130, "1 39134U 13015E   15318.19902117" \
		"  .00007320  00000-0  41168-3 0  9993");
	mini_snprintf(longstr2, 130, "2 39134  64.8724 297.0097 0021111" \
		" 236.0616 123.8502 15.13791064141393");
	/* duration = 1 day */
	jday(2015, 11, 14, 12, 11, 11, &jdstart);
	jday(2015, 11, 15, 12, 11, 11, &jdstop);
	deltamin = 1.0;

	/* "improved" op mode */
	twoline2rv(longstr1, longstr2, jdstart, jdstop, 'i', wgs72,
		&startmfe, &stopmfe, &satrec);

	/*hal_debug_printf("Sat. no. %d\n", satrec.satnum);*/

	sgp4(wgs72, &satrec,  0.0, ro,  vo);

	tsince = startmfe;

	/* check so the first value isn't written twice */
	if (fabs(tsince) > 1.0e-8)
		tsince = tsince - deltamin;

	/* Record count */
	stopmfe = startmfe + (deltamin * (RUNS - 1));

	/* Propagate the orbit */
	while ((tsince < stopmfe) && (satrec.error == 0)) {
		tsince = tsince + deltamin;

		if (tsince > stopmfe)
			tsince = stopmfe;

		sgp4(wgs72, &satrec, tsince, ro,  vo);

		if (satrec.error > 0) {
			/*
			 * hal_debug_printf(
			 * "# *** error: t:= %f *** code = %3d\n",
			 * satrec.t, satrec.error);
			 */
		} else {
			jd = satrec.jdsatepoch + tsince/1440.0;
			invjday(jd, &year, &mon, &day, &hr, &min, &sec);
		}
		ctr++;
	}

	cyc = (*((uint32_t *)0xE0001004)) - cyc;
	time_m = hal_time_get_timestamp_us() - time_m;
	hal_task_resume_scheduler();
	hal_debug_printf("Processed %d in %d us, cyc: %d\n",
		ctr, (int)time_m, (int)cyc);

	for (;;)
		;
}

#endif /* TEST_SGP4_PERFORMANCE */
