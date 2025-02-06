#include "cinterplot_common.h"
#include "randlib.h"
#include "cinterplot.h"

int user_main (int argc, char **argv, CipState *cs)
{
    randlib_init (0);
    const uint32_t nRows = 3;
    const uint32_t nCols = 2;
    uint32_t bordered = 1;
    uint32_t margin = 4;

    cip_set_crosshair_enabled (cs, 0);

    if (cip_make_sub_windows (cs, nRows, nCols, bordered, margin) < 0)
        return 1;

    CipGraph *graph1 = cip_graph_new (3, 0);
    CipGraph *graph2 = cip_graph_new (3, 0);
    CipGraph *graph3 = cip_graph_new (3, 0);
    CipGraph *graph4 = cip_graph_new (3, 0);

    cip_graph_attach (cs, graph1, 0, NULL, 'p', "red yellow white", 32);
    cip_graph_attach (cs, graph2, 0, NULL, 'p', "maroon indigo beige", 32);
    cip_graph_attach (cs, graph3, 0, NULL, 'p', "navy cyan white", 32);
    cip_graph_attach (cs, graph4, 0, NULL, 'p', "gold white", 32);

    cip_graph_attach (cs, graph2, 1, NULL, 'p', "red yellow white", 32);
    cip_graph_attach (cs, graph3, 2, NULL, 'p', "red yellow white", 32);
    cip_graph_attach (cs, graph4, 3, NULL, 'p', "red yellow white", 32);

    CipGraph *graph2d = cip_graph_new (2, 500000);
    cip_continuous_scroll_enable (cs, 4);
    cip_graph_attach (cs, graph2d, 4, NULL, 'p', "red yellow white", 32);

    int nIter = 400000;
    for (int i=0; i<nIter; i++)
    {
        {
            double *r = heavy_tail_ncube (3);
            cip_graph_add_3d_point (graph1, r[0], r[1], r[2]);
        }

        {
            double s = (double) i / nIter;
            double x = cos (s * 2 * M_PI);
            double y = sin (s * 4 * M_PI);
            double z = sin (s* 8*2000 * M_PI) * y * x;
            cip_graph_add_3d_point (graph2, x + 2,y,z);
        }

        {
            double s = (double) i / nIter;
            double x = 0.5*((i % 100) / 50.0 - 1.0);
            double y = sin (s * 2 * M_PI);
            double z = cos (s * 1 * M_PI);
            cip_graph_add_3d_point (graph3, x - 2,y,z);
        }

        {
            //double s = (double) i / nIter;
            //double z = cos (s * M_PI);
            //double r = sin (s * M_PI) * z * z;
            //double x = r * cos (s * 100 * M_PI);
            //double y = r * sin (s * 100 * M_PI);
            double s = (double) i / nIter;
            double z = cos (s * M_PI * 1.2);
            double r = atan (s * M_PI * 3) * (1 + (2*s-1)* (2*s-1));
            double x = r * cos (s * 100 * M_PI);
            double y = r * sin (s * 100 * M_PI);
            x*=0.5;
            y*=0.5;
            z*=0.75;
            cip_graph_add_3d_point (graph4, z,x,y);
        }

    }


    int t = 0;
    double y = 0;
    while (cip_is_running (cs))
    {
        int n = 1000;
        double *r = heavy_tail_ncube (n);
        for (int i=0; i<n; i++)
        {
            double f = 0.9999;
            y = f * y + (1-f) * atan (r[i] * 100);
            double x = (t++) * 1e-6;
            cip_graph_add_2d_point (graph2d, x, y);
        }

        cip_force_refresh (cs);
        cip_redraw_async (cs);
        usleep (10000);
    }

    return 0;
}
