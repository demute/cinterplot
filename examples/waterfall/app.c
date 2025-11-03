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
    uint32_t nRows = 2;
    uint32_t nCols = 1;

    if (cip_make_sub_windows (cs, nRows, nCols, bordered, margin) < 0)
        return 1;

    const int nPoints = 1000;
    int windowIndex = 0;

    CipGraph *graphs[2] = {0};

    graphs[0] = cip_graph_new (2, nPoints);
    cip_graph_attach (cs, graphs[0], windowIndex++, NULL, 'p', "#ff4444 yellow", 4);

    graphs[1] = cip_graph_new (2, 1000000);
    //cip_graph_attach (cs, graphs[1], windowIndex++, NULL, 'w', "gold red black #4444ff cyan", 1000);
    cip_graph_attach (cs, graphs[1], windowIndex++, NULL, 'w', "black red yellow white", 1000);

    cip_set_x_range (cs, 0, 0, 1, 1);
    cip_set_y_range (cs, 0, -3, 3, 1);

    double theta = 0;
    double y = 0;
    while (cip_is_running (cs))
    {
        for (int ni=0; ni<nPoints; ni++)
        {
            double x = ni * (1.0 / nPoints);
            double z = sin (cos (x * M_PI) * 2*M_PI) + sin ((theta) + x * 10 + theta);
            y = 0.5 * y + 0.5 * z + y * (randf () * 2 - 1) * 0.1;

            cip_graph_add_2d_point (graphs[0], x, y);
            cip_graph_add_2d_point (graphs[1], x, y);
        }
        theta += 0.0040303303;
        if (theta  > 2*M_PI)
            theta -= 2*M_PI;

        cip_graph_add_2d_point (graphs[0], NaN, NaN);
        cip_graph_add_2d_point (graphs[1], NaN, NaN);
        CipSubWindow *sw0 = cip_get_sub_window (cs, 0);
        CipSubWindow *sw1 = cip_get_sub_window (cs, 1);
        sw1->logMode = sw0->logMode;
        memcpy (& sw1->dataRange, & sw0->dataRange, sizeof (sw1->dataRange));

        cip_redraw_async (cs);
        usleep (1000);
    }

    return 0;
}
