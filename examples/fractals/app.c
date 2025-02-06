#include "cinterplot_common.h"
#include "cinterplot.h"

#define GET_DATA_POS_X(hist,xi) (((double) xi / (hist->w-1)) * (hist->dataRange.x1 - hist->dataRange.x0) + hist->dataRange.x0)
#define GET_DATA_POS_Y(hist,yi) (((double) yi / (hist->h-1)) * (hist->dataRange.y1 - hist->dataRange.y0) + hist->dataRange.y0)



uint64_t count_of_xy (CipHistogram *hist, CipGraph *graph, uint32_t logMode, char plotType)
{
    int *bins  = hist->bins;
    uint32_t w = hist->w;
    uint32_t h = hist->h;
    static double s = 1.00 - 0.65;
    s += 0.05;
    //s = -0.95;
    print_debug ("s: %f", s);

    for (uint32_t yi=0; yi<h; yi++)
    {
        for (uint32_t xi=0; xi<w; xi++)
        {
            double x = GET_DATA_POS_X (hist, xi);
            double y = GET_DATA_POS_Y (hist, yi);

            int T = 100;
            double Re = 0;
            double Im = 0;
            switch (plotType)
            {
             case '0':
                 // mandelbrot
                 for (int j=0; j<T; j++)
                 {
                     double r = hypot (Re, Im);
                     double t = atan2 (Im, Re);

                     double rr = pow (r, 2);
                     //double tt = pow(s,t);
                     double tt = pow(2,t);

                     Re = rr * cos (tt) + x;
                     Im = rr * sin (tt) + y;



                     //double a = Re;
                     //double b = Im;
                     //Re = a*a - b*b + x;
                     //Im = 2*a*b + y;
                 }
                 break;
             case '1':
                 // morphed mandelbrot
                 for (int j=0; j<T; j++)
                 {
                     double a = Re;
                     double b = Im;
                     Re = a*a - b*b + x;
                     Im = 2*a*b + y - a*a*b*b;
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

            //double dist = sqrt (Re*Re + Im*Im);

            //int cnt = (int) (sqrt(dist)*1000);

            int cnt = (int) (1000 * (1.1-fabs(atan2 (Im, Re) - atan2 (y, x)) / (2 * M_PI)));
            //int cnt = (int) (1000 * ((atan2 (Im, Re) + M_PI) / (2 * M_PI)));
            bins[yi*w+xi] = cnt;
        }
    }
    return 0;
}

int user_main (int argc, char **argv, CipState *cs)
{
    const uint32_t nRows = 2;
    const uint32_t nCols = 3;
    uint32_t bordered = 0;
    uint32_t margin = 4;

    
    cip_set_bg_shade (cs, 0.0);
    cip_set_crosshair_enabled (cs, 0);
    cip_set_statusline_enabled (cs, 0);
    cip_set_grid_mode (cs, 0);

    if (cip_make_sub_windows (cs, nRows, nCols, bordered, margin) < 0)
        return 1;

    CipGraph *nullGraph = cip_graph_new (0);

    //graph_attach (cs, nullGraph, 0, count_of_xy, '4', "white yellow red black", 1000);
    uint32_t windowIndex = 0;
    cip_graph_attach (cs, nullGraph, windowIndex++, count_of_xy, '0', "white yellow red black", 1000);
    cip_graph_attach (cs, nullGraph, windowIndex++, count_of_xy, '1', "white yellow red black blue cyan black", 1000);
    cip_graph_attach (cs, nullGraph, windowIndex++, count_of_xy, '2', "white yellow red black", 1000);
    cip_graph_attach (cs, nullGraph, windowIndex++, count_of_xy, '3', "white yellow red black blue cyan black", 1000);
    cip_graph_attach (cs, nullGraph, windowIndex++, count_of_xy, '4', "white yellow red black", 1000);
    cip_graph_attach (cs, nullGraph, windowIndex++, count_of_xy, '5', "white white blue purple black", 1000);

    cip_set_range (cip_get_sub_window (cs, 0), -1.6, -1.0, 0.6,  1.0, 1);
    cip_set_range (cip_get_sub_window (cs, 1), -1.9, -1.2, 0.7,  1.0, 1);
    cip_set_range (cip_get_sub_window (cs, 2), -2.2, -2.2, 2.2,  2.2, 1);
    cip_set_range (cip_get_sub_window (cs, 3), -2.6, -2.2, 0.2,  2.2, 1);
    cip_set_range (cip_get_sub_window (cs, 4), -2.7, -1.8, 0.6,  1.8, 1);
    cip_set_range (cip_get_sub_window (cs, 5), -2.5,  0.7, 0.9, -2.7, 1);

    cip_redraw_async (cs);
    return 0;
}
