/*
 * BRENT implementation, taken from
 * https://people.sc.fsu.edu/~jburkardt/c_src/brent/brent.html
 *
 * Licensed under LGPL
 * https://people.sc.fsu.edu/~jburkardt/txt/gnu_lgpl.txt
 *
 * Modified to remove dependencies on time, stdlib and stdio
 * and add "brent_" prefixes; by Felix Walter, Jan. 2016
 */

#ifndef BRENT_H_INCLUDED
#define BRENT_H_INCLUDED

double brent_global_min ( double a, double b, double c, double m, double machep,
  double e, double t, double f ( double x ), double *x );
double brent_local_min ( double a, double b, double eps, double t,
  double f ( double x ), double *x );
double brent_local_min_rc ( double *a, double *b, int *status, double value );
double brent_zero ( double a, double b, double machep, double t,
  double f ( double x ) );
void brent_zero_rc ( double a, double b, double t, double *arg, int *status,
  double value );

#endif /* BRENT_H_INCLUDED */
