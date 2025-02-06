#include "cinterplot_common.h"
#include "randlib.h"
#include "cinterplot.h"

#ifndef randf
#define randf() ((double) rand () / ((double) RAND_MAX+1))
#endif

int user_main (int argc, char **argv, CipState *cs)
{
    randlib_init (0);

    const uint32_t nRows = 2;
    const uint32_t nCols = 3;
    const uint32_t n = nRows * nCols;
    uint32_t bordered = 1;
    uint32_t margin = 4;

    char *colorSchemes[6] =
    {
        "red orange white",
        "purple blue white",
        "blue yellow white",
        "brown turquoise white",
        "violet indigo white",
        "chocolate brown red yellow"
    };

    if (cip_make_sub_windows (cs, nRows, nCols, bordered, margin) < 0)
        return 1;

    CipGraph *sineGraph[n];
    char plotType[6] = {'p','l','s','p','l','s'};
    for (int i=0; i<n; i++)
    {
        cip_continuous_scroll_enable (cs, i);
        sineGraph[i] = cip_graph_new (1000000);
        cip_graph_attach (cs, sineGraph[i], (uint32_t) i, NULL, plotType[i], colorSchemes[i % 6], 8);
    }

    double v = 0;
    uint64_t t = 0;
    double a[n];
    bzero (a, sizeof (a));

    while (cip_is_running (cs))
    {
        usleep (0);

        for (int i=0; i<n; i++)
        {
            v += (1.0 / 1024.0) * 1.3;
            if (v > 5)
                v -= 5;

            double f = 0.99;
            double *r = heavy_tail_ncube (n);
            a[i] = f * a[i] + (1-f) * r[i] * 0.1;
            double y = a[i] * sin (2 * M_PI * v * a[i] * (i+1));
            double x = (double) t;
            cip_graph_add_point (sineGraph[i], x, y);
        }
        t++;
        cip_redraw_async (cs);
    }

    return 0;
}
