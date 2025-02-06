/* 
 * Fast discrete cosine transform algorithms (C)
 * 
 * Copyright (c) 2021 Project Nayuki. (MIT License)
 * https://www.nayuki.io/page/fast-discrete-cosine-transform-algorithms
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 * - The above copyright notice and this permission notice shall be included in
 *   all copies or substantial portions of the Software.
 * - The Software is provided "as is", without warranty of any kind, express or
 *   implied, including but not limited to the warranties of merchantability,
 *   fitness for a particular purpose and noninfringement. In no event shall the
 *   authors or copyright holders be liable for any claim, damages or other
 *   liability, whether in an action of contract, tort or otherwise, arising from,
 *   out of or in connection with the Software or the use or other dealings in the
 *   Software.
 */

#include <assert.h>
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fast-dct-fft.h"

static const double EPSILON = 1e-9;

// Forward declarations
static void testFastDctFftInvertibility(void);
static void assertArrayEquals(const double expect[], const double actual[], size_t len, double epsilon);
static double *randomVector(size_t len);


int main(void)
{
    //testFastDctFftInvertibility();
    //fprintf(stderr, "Test passed\n");

    size_t len = 8;
    double *vector = randomVector(len);
    for (int i=0; i<len; i++)
        vector[i] = (i % 2) * 2;

    printf ("\nORIG  "); for (int i=0; i<len; i++) printf ("%7.4f  ", vector[i]);

    double *temp = calloc(len, sizeof(double));
    memcpy(temp, vector, len * sizeof(double));
    FastDctFft_transform(temp, len);
    printf ("\nDCT   "); for (int i=0; i<len; i++) printf ("%7.4f  ", temp[i]);
    FastDctFft_inverseTransform(temp, len);
    for (size_t i = 0; i < len; i++)
        temp[i] /= len / 2.0;
    printf ("\nIDCT  "); for (int i=0; i<len; i++) printf ("%7.4f  ", temp[i]);
    //assertArrayEquals (vector, temp, len, EPSILON);
    free(vector);
    free(temp);
    return EXIT_SUCCESS;
}

static void testFastDctFftInvertibility(void)
{
    size_t prev = 0;
    for (int i = 0; i <= 30; i++)
    {
        size_t len = (size_t)round(pow(1000000.0, i / 30.0));
        if (len <= prev)
            continue;
        prev = len;
        double *vector = randomVector(len);
        double *temp = calloc(len, sizeof(double));
        memcpy(temp, vector, len * sizeof(double));
        FastDctFft_transform(temp, len);
        FastDctFft_inverseTransform(temp, len);
        for (size_t i = 0; i < len; i++)
            temp[i] /= len / 2.0;
        assertArrayEquals(vector, temp, len, EPSILON);
        free(vector);
        free(temp);
    }
}


static void assertArrayEquals(const double expect[], const double actual[], size_t len, double epsilon)
{
    for (size_t i = 0; i < len; i++)
        assert(fabs(expect[i] - actual[i]) < epsilon);
}


static double *randomVector(size_t len)
{
    double *result = calloc(len, sizeof(double));
    for (size_t i = 0; i < len; i++)
        result[i] = (double)rand() / RAND_MAX * 2 - 1;
    return result;
}
