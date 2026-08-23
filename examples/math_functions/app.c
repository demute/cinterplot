#include "cinterplot_common.h"
#include "cinterplot.h"

void canvas_fun_math_functions (WorldTransform *world, void *_points, size_t len, int firstUnusedIndex, uint32_t logMode, CipCanvas *canvas)
{
    //double (*restrict points)[2] = _points;
    //double *restrict wz          = canvas->wz;
    int    *restrict bins        = canvas->bins;

    int w = canvas->w;
    int h = canvas->h;

    memset (canvas->bins, 0x00, w*h*sizeof (canvas->bins[0]));
    memset (canvas->wz,   0x00, w*h*sizeof (canvas->wz[0]));

    char plotType = '5';

    for (uint32_t yi=0; yi<h; yi++)
    {
        for (uint32_t xi=0; xi<w; xi++)
        {
            double xyz[3];
            world_transform_bin_to_datapos (world, w, h, xi, yi, 0, xyz);

            double x = xyz[0];
            double y = xyz[1];
            double z = xyz[2];

            switch (plotType)
            {
             case '0': z = 1; break;
             case '1': z = x; break;
             case '2': z = sqrt(x); break;
             case '3': z = x*x*x; break;
             case '4': z = x*x*x*x; break;
             case '5': z = 0.5+0.5*sin(x*cos(y)*cos(x+y*y)); break;
             default:  z = 0;
            }

            double datapos[3] = {x,y,z};
            double wzVal;
            int binx, biny;
            world_transform_datapos_to_bin (world, datapos, w, h, & binx, & biny, & wzVal);

            if ((unsigned)binx < (unsigned)w && (unsigned)biny < (unsigned)h)
                bins[(uint32_t) biny * w + binx] = wzVal * 1024;
        }
    }
}

int user_main (int argc, char **argv, CipState *cs)
{
    const uint32_t nRows = 2;
    const uint32_t nCols = 4;
    uint32_t bordered = 1;
    uint32_t margin = 4;

    if (cip_make_sub_windows (cs, nRows, nCols, bordered, margin) < 0)
        return 1;

    for (int i=0; i<256; i++)
        cip_register_canvas_fun (1, (char) i, canvas_fun_math_functions);

    CipGraph *nullGraph = cip_graph_new (1, 10);

    uint32_t windowIndex = 0;
    cip_graph_attach (cs, nullGraph, windowIndex++, '1', "blue white", 1024);
    cip_graph_attach (cs, nullGraph, windowIndex++, '2', "blue white", 1024);
    cip_graph_attach (cs, nullGraph, windowIndex++, '3', "blue white", 1024);
    cip_graph_attach (cs, nullGraph, windowIndex++, '4', "blue white", 1024);
    cip_graph_attach (cs, nullGraph, windowIndex++, '1', "red black blue", 1024);
    cip_graph_attach (cs, nullGraph, windowIndex++, '2', "red black blue", 1024);
    cip_graph_attach (cs, nullGraph, windowIndex++, '3', "1/10 2/10 3/10 4/10 5/10 6/10 7/10 8/10 9/10 10/10 black", 1024);
    cip_graph_attach (cs, nullGraph, windowIndex++, '4', "red black blue", 1024);

    double tiltAngle = -M_PI * 0.75;
    while (cip_is_running (cs))
    {
        cip_graph_add_1d_point (nullGraph, 1.0);
        int si[] = {0,1,2,3,4,5,6,7};
        int nn = sizeof (si) / sizeof (si[0]);
        for (int i=0; i<nn; i++)
        {
            double w[3] = {0};
            double datapos[3];
            CipSubWindow *sw = cip_get_sub_window (cs, si[i]);
            world_transform_worldpos_to_datapos (& sw->world, w, datapos);
            world_transform_rotate_data  (& sw->world, datapos, 1, 0.01);
        }

        cip_force_refresh (cs);
        cip_redraw_async (cs);
        usleep (10000);
    }
    return 0;
}
