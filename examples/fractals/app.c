#include "cinterplot_common.h"
#include "cinterplot.h"

void canvas_fun_fractals (WorldTransform *world, void *_points, size_t len, int firstUnusedIndex, uint32_t logMode, CipCanvas *canvas)
{
    int *bins  = canvas->bins;
    uint32_t w = canvas->w;
    uint32_t h = canvas->h;
    char plotType = '1';

    for (uint32_t yi=0; yi<h; yi++)
    {
        for (uint32_t xi=0; xi<w; xi++)
        {
            double xyz[3];
            world_transform_bin_to_datapos (world, w, h, xi, yi, 0, xyz);

            double x = xyz[0];
            double y = xyz[1];
            double z = xyz[2];

            int T = 100;
            double Re = 0;
            double Im = 0;
            switch (plotType)
            {
             case '0':
                 for (int j=0; j<T; j++)
                 {
                     double r = hypot (Re, Im);
                     double t = atan2 (Im, Re);

                     double rr = pow (r, 2);
                     double tt = pow(2,t);

                     Re = rr * cos (tt) + x;
                     Im = rr * sin (tt) + y;
                 }
                 break;
             case '1':
                 // mandelbrot
                 for (int j=0; j<T; j++)
                 {
                     double a = Re;
                     double b = Im;
                     Re = a*a - b*b - z*z + x;
                     Im = 2*a*b + a*b*z + y;
                 }
                 break;
             case '2':
                 // headphone man
                 for (int j=0; j<T; j++)
                 {
                     double a = Re;
                     double b = Im;
                     Re = cos(a*a - b*b) + x;
                     Im = sin(2*a*b) + y;
                 }
                 break;
             case '3':
                 // fly
                 for (int j=0; j<T; j++)
                 {
                     double a = Re;
                     double b = Im;
                     Re = exp(a*a - b*b) + x;
                     Im = sin(2*a*b) + y;
                 }
                 break;
             case '4':
                 // spaceship
                 for (int j=0; j<T; j++)
                 {
                     double a = Re;
                     double b = Im;
                     Re = log(1+a*a + b*b) + x;
                     Im = sin(2*a*b) + y;
                 }
                 break;
             case '5':
                 // sentinel
                 for (int j=0; j<T; j++)
                 {
                     double a = Re;
                     double b = Im;
                     Re = log(1+a*a + b*b) + x;
                     Im = 1-(2*a*b) + y;
                 }
                 break;
             default:
                 break;
            }

            int cnt = (int) (1000 * (1.1-fabs(atan2 (Im, Re) - atan2 (y, x)) / (2 * M_PI)));
            bins[yi*w+xi] = cnt;
        }
    }
}

int user_main (int argc, char **argv, CipState *cs)
{
    const uint32_t nRows = 2;
    const uint32_t nCols = 3;
    uint32_t bordered = 1;
    uint32_t margin = 4;


    cip_set_bg_shade (cs, 0.0);
    cip_set_crosshair_enabled (cs, 0);
    cip_set_statusline_enabled (cs, 0);

    for (int i=0; i<256; i++)
        cip_register_canvas_fun (1, (char) i, canvas_fun_fractals);

    if (cip_make_sub_windows (cs, nRows, nCols, bordered, margin) < 0)
        return 1;

    for (int i=0; i<nRows*nCols; i++)
        cip_set_grid_mode (cs, i, 0);

    CipGraph *nullGraph = cip_graph_new (1, 0);
    cip_graph_add_1d_point (nullGraph, 1.0);
    cip_graph_attach (cs, nullGraph, 0, '1', "white yellow red black blue cyan black", 1000);
    cip_set_range (cip_get_sub_window (cs, 1), -1.9, -1.2, 0.7,  1.0, 1);
    cip_redraw_async (cs);
    return 0;
}
