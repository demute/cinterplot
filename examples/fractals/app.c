#include "common.h"
#include "cinterplot.h"

#define GET_DATA_POS_X(hist,xi) (((double) xi / (hist->w-1)) * (hist->dataRange.x1 - hist->dataRange.x0) + hist->dataRange.x0)
#define GET_DATA_POS_Y(hist,yi) (((double) yi / (hist->h-1)) * (hist->dataRange.y1 - hist->dataRange.y0) + hist->dataRange.y0)



uint64_t count_of_xy (Histogram *hist, CinterGraph *graph, char plotType)
{
    int *bins  = hist->bins;
    uint32_t w = hist->w;
    uint32_t h = hist->h;

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
                     double a = Re;
                     double b = Im;
                     Re = a*a - b*b + x;
                     Im = 2*a*b + y;
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

            double dist = sqrt (Re*Re + Im*Im);

            int cnt = (int) (sqrt(dist)*1000);
            bins[yi*w+xi] = cnt;
        }
    }
    return 0;
}

int user_main (int argc, char **argv, CinterState *cs)
{
    const uint32_t nRows = 2;
    const uint32_t nCols = 3;
    uint32_t bordered = 0;
    uint32_t margin = 4;

    
    cinterplot_set_bg_shade (cs, 0.0);
    if (make_sub_windows (cs, nRows, nCols, bordered, margin) < 0)
        return 1;

    CinterGraph *nullGraph = graph_new (0);

    //graph_attach (cs, nullGraph, 0, count_of_xy, '4', "white yellow red black", 1000);
    uint32_t windowIndex = 0;
    graph_attach (cs, nullGraph, windowIndex++, count_of_xy, '0', "white yellow red black", 1000);
    graph_attach (cs, nullGraph, windowIndex++, count_of_xy, '1', "white yellow red black blue cyan black", 1000);
    graph_attach (cs, nullGraph, windowIndex++, count_of_xy, '2', "white yellow red black", 1000);
    graph_attach (cs, nullGraph, windowIndex++, count_of_xy, '3', "white yellow red black blue cyan black", 1000);
    graph_attach (cs, nullGraph, windowIndex++, count_of_xy, '4', "white yellow red black", 1000);
    graph_attach (cs, nullGraph, windowIndex++, count_of_xy, '5', "white white blue purple black", 1000);

    set_range (get_sub_window (cs, 0), -1.6, -1.0, 0.6,  1.0, 1);
    set_range (get_sub_window (cs, 1), -1.9, -1.2, 0.7,  1.0, 1);
    set_range (get_sub_window (cs, 2), -2.2, -2.2, 2.2,  2.2, 1);
    set_range (get_sub_window (cs, 3), -2.6, -2.2, 0.2,  2.2, 1);
    set_range (get_sub_window (cs, 4), -2.7, -1.8, 0.6,  1.8, 1);
    set_range (get_sub_window (cs, 5), -2.5,  0.7, 0.9, -2.7, 1);

    cinterplot_redraw_async (cs);
    return 0;
}
