#ifndef _RANDLIB_H_
#define _RANDLIB_H_

#ifdef __cplusplus
extern "C" {
#endif


#include <stdarg.h>
#include <gsl/gsl_math.h>
#include <gsl/gsl_randist.h>
#include <gsl/gsl_rng.h>
#include <gsl/gsl_test.h>
#include <gsl/gsl_ieee_utils.h>
#include <gsl/gsl_integration.h>

extern gsl_rng* r_global;

void randlib_init (int seed);

#define rnorm(sigma) gsl_ran_gaussian_ziggurat (r_global, sigma)
#define runif(a,b) gsl_ran_flat (r_global, a, b)
#define rlomax(shape,scale) gsl_ran_pareto (r_global, shape, scale) - scale
#define randf() runif (0.0, 1.0)
//#define rlomax(shape,scale) pow (scale / runif(0.0,1.0), (1.0 / shape)) - scale

// uniform distribution for distance from origin
#define hyper_unif(dst,len) va_hyper_rand (dst, len, va_runif, 0)
double va_runif (va_list valist);

// unifrom distribution for area
#define hyper_disc(dst,len) va_hyper_rand (dst, len, va_rdisc, 0)
double va_rdisc (va_list valist);

// lomax distribution for distance from origin
#define hyper_lomax(dst,len,shape,scale) va_hyper_rand (dst, len, va_rlomax, 2, shape, scale)
double va_rlomax (va_list valist);

double* heavy_tail_ncube (int dim);

#ifdef __cplusplus
} /* end extern C */
#endif

#endif /* _RANDLIB_H_ */

