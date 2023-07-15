#include "common.h"
#include "cinterplot.h"

#define GET_DATA_POS_X(hist,xi) (((double) xi / (hist->w-1)) * (hist->dataRange.x1 - hist->dataRange.x0) + hist->dataRange.x0)
#define GET_DATA_POS_Y(hist,yi) (((double) yi / (hist->h-1)) * (hist->dataRange.y1 - hist->dataRange.y0) + hist->dataRange.y0)



uint64_t y_of_x (Histogram *hist, CinterGraph *graph, char plotType)
{
    int *bins  = hist->bins;
    uint32_t w = hist->w;
    uint32_t h = hist->h;
    uint32_t nBins = w * h;

    Area *dr = & hist->dataRange;

    for (uint32_t i=0; i<nBins; i++)
        bins[i] = 0;

    for (uint32_t xi=0; xi<w; xi++)
    {
        double x = GET_DATA_POS_X (hist, xi);

        double y = x*x;

        int yi = (int) ((h-1) * (y - dr->y0) / (dr->y1 - dr->y0));
//        int yi = GET_BIN_POS_Y (hist, y);

        if (yi >= 0 && yi < h)
            bins[(uint32_t) yi * w + xi] = 1023;
    }
    return 0;
}

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

            int cnt = 512 + (int) (x*y*1024);
            if (cnt < 1) cnt = 1;
            bins[yi*w+xi] = cnt;
        }
    }
    return 0;
}

int user_main (int argc, char **argv, CinterState *cs)
{
    const uint32_t nRows = 1;
    const uint32_t nCols = 2;
    uint32_t bordered = 1;
    uint32_t margin = 4;

    if (make_sub_windows (cs, nRows, nCols, bordered, margin) < 0)
        return 1;

    CinterGraph *g1 = graph_new (0);
    GraphAttacher *a1 = graph_attach (cs, g1, 0, 'p', "black white", 1024);
    attacher_set_histogram_function (a1, y_of_x);
    set_range (get_sub_window (cs, 0), -1, -1, 1, 1, 1);

    CinterGraph *g2 = graph_new (0);
    GraphAttacher *a2 = graph_attach (cs, g2, 1, 'p', "red black blue", 1024);
    attacher_set_histogram_function (a2, count_of_xy);
    set_range (get_sub_window (cs, 0), -1, -1, 1, 1, 1);

    cinterplot_redraw_async (cs);
    return 0;
}
