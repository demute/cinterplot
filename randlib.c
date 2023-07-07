#include "common.h"
#include <gsl/gsl_math.h>
#include <gsl/gsl_randist.h>
#include <gsl/gsl_rng.h>
#include <gsl/gsl_test.h>
#include <gsl/gsl_ieee_utils.h>
#include <gsl/gsl_integration.h>

#include "randlib.h"

gsl_rng* r_global = NULL;

void randlib_init (uint64_t seed)
{
    if (!seed)
        seed = (uint64_t) time (NULL);

    const gsl_rng_type* rngType;
    gsl_rng_env_setup ();
    gsl_rng_default_seed = seed;
    rngType = gsl_rng_default;
    r_global = gsl_rng_alloc (rngType);
}

double va_runif (va_list valist) { return drand48 (); }
double va_rdisc (va_list valist) { return sqrt (drand48 ()); }
double va_rlomax (va_list valist)
{
    double shape = va_arg (valist, double);
    double scale = va_arg (valist, double);
    return rlomax (shape, scale);
}

void va_hyper_rand (double* r, int len, double (*dist) (va_list valist), int nargs, ...)
{
    va_list valist;
    va_start (valist, nargs);

    double length = 0.0;
    for (int j=0; j<len; j++)
    {
        r[j] = rnorm (1.0);
        length += r[j] * r[j];
    }
    length = sqrt (length);

    double radius = dist (valist);
    double scale  = radius / length;
    for (int j=0; j<len; j++)
        r[j] *= scale;
}

static inline double ctrans (double unif)
{
    double val = tan (M_PI * (unif)) * 0.05;
    return val;
}

static inline double cauchy ()
{
    return ctrans (drand48 () - 0.5);
}

double* heavy_tail_ncube (uint32_t dim)
{
    static double* coords = NULL;
    static uint32_t maxDim = 0;

    if (maxDim < dim)
    {
        if (coords)
            free (coords);
        maxDim = dim;
        coords = (double *) malloc (sizeof (double) * maxDim);
    }

    double length = cauchy ();

    double sum = 0.0;
    for (uint32_t i=0; i<dim; i++)
    {
        coords[i] = drand48 () - 0.5;
        sum += coords[i] * coords[i];
    }
    double radius = sqrt (sum);

    for (uint32_t i=0; i<dim; i++)
        coords[i] = length * (coords[i] / radius);

    return coords;
}


