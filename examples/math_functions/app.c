#include "cinterplot_common.h"
#include "cinterplot.h"

#define GET_DATA_POS_X(hist,xi) (((double) xi / (hist->w-1)) * (hist->dataRange.x1 - hist->dataRange.x0) + hist->dataRange.x0)
#define GET_DATA_POS_Y(hist,yi) (((double) yi / (hist->h-1)) * (hist->dataRange.y1 - hist->dataRange.y0) + hist->dataRange.y0)


uint64_t y_of_x (CipHistogram *hist, CipGraph *graph, uint32_t logMode, char plotType, uint64_t lastGraphCounter)
{
    int *bins  = hist->bins;
    uint32_t w = hist->w;
    uint32_t h = hist->h;
    uint32_t nBins = w * h;

    CipArea *dr = & hist->dataRange;

    for (uint32_t i=0; i<nBins; i++)
        bins[i] = 0;

    for (uint32_t xi=0; xi<w; xi++)
    {
        double x = GET_DATA_POS_X (hist, xi);
        double y;

        switch (plotType)
        {
         case '0': y = 1; break;
         case '1': y = x; break;
         case '2': y = sqrt(x); break;
         case '3': y = x*x*x; break;
         case '4': y = x*x*x*x; break;
         case '5': y = x*x*x*x*x; break;
         default:  y = 0;
        }

        int yi = (int) ((h-1) * (y - dr->y0) / (dr->y1 - dr->y0));
//        int yi = GET_BIN_POS_Y (hist, y);

        if (yi >= 0 && yi < h)
            bins[(uint32_t) yi * w + xi] = 1023;
    }
    return 0;
}

uint64_t count_of_xy (CipHistogram *hist, CipGraph *graph, uint32_t logMode, char plotType, uint64_t lastGraphCounter)
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

            int cnt;
            switch (plotType)
            {
             case '1': cnt = 512 + (int) (x*y*1024); break;
             case '2': cnt = (int) (512+sin(x)*sin(y)*512); break;
             case '3': cnt = (int) (1024*(x*x+y*y)); break;
             case '4': cnt = (int) (512+sin(x)*tan(y)*512); break;
             default:  cnt = 0;
            }
            if (cnt < 1) cnt = 1;
            bins[yi*w+xi] = cnt;
        }
    }
    return 0;
}

int user_main (int argc, char **argv, CipState *cs)
{
    const uint32_t nRows = 2;
    const uint32_t nCols = 4;
    uint32_t bordered = 1;
    uint32_t margin = 4;

    if (cip_make_sub_windows (cs, nRows, nCols, bordered, margin) < 0)
        return 1;

    CipGraph *nullGraph = cip_graph_new (2, 0);

    uint32_t windowIndex = 0;
    cip_graph_attach (cs, nullGraph, windowIndex++, y_of_x, '1', "black white", 1024);
    cip_graph_attach (cs, nullGraph, windowIndex++, y_of_x, '2', "black white", 1024);
    cip_graph_attach (cs, nullGraph, windowIndex++, y_of_x, '3', "black white", 1024);
    cip_graph_attach (cs, nullGraph, windowIndex++, y_of_x, '4', "black white", 1024);
    cip_graph_attach (cs, nullGraph, windowIndex++, count_of_xy, '1', "red black blue", 1024);
    cip_graph_attach (cs, nullGraph, windowIndex++, count_of_xy, '2', "red black blue", 1024);
    cip_graph_attach (cs, nullGraph, windowIndex++, count_of_xy, '3', "1/10 2/10 3/10 4/10 5/10 6/10 7/10 8/10 9/10 10/10 black", 1024);
    cip_graph_attach (cs, nullGraph, windowIndex++, count_of_xy, '4', "red black blue", 1024);

    cip_redraw_async (cs);
    return 0;
}
