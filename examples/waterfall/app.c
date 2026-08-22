#include "cinterplot_common.h"
#include "cinterplot.h"

#ifndef randf
#define randf() ((double) rand () / ((double) RAND_MAX+1))
#endif

static uint32_t bordered = 1;
static uint32_t margin = 4;
static CipState *cs = NULL;

int user_main (int argc, char **argv, CipState *_cs)
{
    cs = _cs;
    uint32_t nRows = 1;
    uint32_t nCols = 1;

    int nPoints = 1000;
    if (cip_make_sub_windows (cs, nRows, nCols, bordered, margin) < 0)
        return 1;

    CipGraph *graph = cip_graph_new (2, 1000000);
    cip_graph_attach (cs, graph, 0, 'w', "black red yellow white", 1000);

    double theta = 0;
    double y = 0;
    while (cip_is_running (cs))
    {
        for (int ni=0; ni<nPoints; ni++)
        {
            double x = ni * (1.0 / nPoints);
            double z = sin (cos (x * M_PI) * 2*M_PI) + sin ((theta) + x * 10 + theta);
            y = 0.5 * y + 0.5 * z + y * (randf () * 2 - 1) * 0.1;
            cip_graph_add_2d_point (graph, x, y);
        }
        theta += 0.0040303303;
        if (theta  > 2*M_PI)
            theta -= 2*M_PI;

        cip_graph_add_2d_point (graph, NaN, NaN);
        cip_redraw_async (cs);
        usleep (1000);
    }

    return 0;
}
