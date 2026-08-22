#include "cinterplot.h"
#include "cinterplot_common.h"
#include "canvas_functions.h"
#include "benchmark.h"

static void cip_canvas_line (CipCanvas *canvas, int x0, int y0, int x1, int y1, double wzVal)
{
    int    *bins = canvas->bins;
    double *wz   = canvas->wz;

    int w = (int) canvas->w;
    int h = (int) canvas->h;

    if ((x0 < 0 && x1 < 0) ||
        (y0 < 0 && y1 < 0) ||
        (x0 > w && x1 > w) ||
        (y0 > h && y1 > h)
       )
        return;

    int xabs = (x1 > x0) ? x1 - x0 : x0 - x1;
    int yabs = (y1 > y0) ? y1 - y0 : y0 - y1;

    if (xabs > yabs)
    {
        int xstart = ((x0 < x1) ? x0 : x1);
        int xstop  = xstart + xabs;

        if (xstart <   0) xstart = 0;
        if (xstart > w-1) xstart = w-1;
        if (xstop <    0) xstop  = 0;
        if (xstop >  w-1) xstop  = w-1;

        for (int x=xstart; x<=xstop; x++)
        {
            int y = (int) (y0 + ((double) (x - x0) / (x1 - x0) * (y1 - y0) + 0.5));
            if (x>=0 && y>=0 && x<w && y<h)
            {
                bins[y*w+x]++;
                wz[y*w+x] = wzVal;
            }
        }
    }
    else if (yabs >= xabs && y0 != y1)
    {
        int ystart = ((y0 < y1) ? y0 : y1);
        int ystop  = ystart + yabs;

        if (ystart <   0) ystart = 0;
        if (ystart > h-1) ystart = h-1;
        if (ystop <    0) ystop  = 0;
        if (ystop >  h-1) ystop  = h-1;

        for (int y=ystart; y<=ystop; y++)
        {
            int x = (int) (x0 + ((double) (y - y0) / (y1 - y0) * (x1 - x0) + 0.5));
            if (x>=0 && y>=1 && x<w && y<h)
            {
                bins[y*w+x]++;
                wz[y*w+x] = wzVal;
            }
        }
    }
    else
    {
        int x = x0;
        int y = y0;
        if (x>=0 && y>=0 && x<w && y<h)
        {
            bins[y*w+x]++;
            wz[y*w+x] = wzVal;
        }
    }
}

static void canvas_fun_1d_histogram (WorldTransform *world, void *_points, size_t len, uint32_t logMode, CipCanvas *canvas)
{
    BENCHMARK_ADD_CHECKPOINT ("1d_histogram");
    double *points = _points;
    double *sums   = canvas->sums;
    int    w       = canvas->w;
    int    h       = canvas->h;

    double sx   = world->scaleMtx[0][0];
    double xmin = world->centerPos[0] - 1.0/sx;
    double xmax = world->centerPos[0] + 1.0/sx;

    for (int xi=0; xi<w; xi++)
        sums[xi] = 0;

    for (size_t i=0; i<len; i++)
    {
        double x = points[i];
        if (logMode & 1) x = LOGFUN (x);
        if (isnan (x) || isinf (x))
            continue;

        int xi = (int) (w * (x - xmin) / (xmax - xmin));
        if (xi >= 0 && xi < w)
            sums[xi] += 1.0;
    }


    int lastXi = -1;
    int lastYi = -1;

    for (int xi=0; xi<w; xi++)
    {
        double y = sums[xi];
        double x = xmin + (xi * (1.0 / w)) * (xmax - xmin);
        if (logMode & 2) y = LOGFUN (y);

        int binx, biny;
        double datapos[3] = {x,y,0};
        double wzVal;
        if (world_transform_datapos_to_bin (world, datapos, w, h, & binx, & biny, & wzVal) != 0)
        {
            lastXi = -1;
            lastYi = -1;
            continue;
        }

        if (lastXi >= 0 && lastYi >= 0)
        {
            cip_canvas_line (canvas, lastXi, lastYi, binx, biny, wzVal);
        }
        lastXi = binx;
        lastYi = biny;
    }
}

static void canvas_fun_2d_histogram_point (WorldTransform *world, void *_points, size_t len, uint32_t logMode, CipCanvas *canvas)
{
    BENCHMARK_ADD_CHECKPOINT ("2d_histogram_point");
    double (*restrict points)[2] = _points;
    int    *restrict bins        = canvas->bins;
    double *restrict wz          = canvas->wz;

    int w = canvas->w;
    int h = canvas->h;

    WORLD_TRANSFORM_DATAPOS_TO_BIN_HOT_LOOP_INIT (world);

    for (size_t i=0; i<len; i++)
    {
        double pt[3] = {points[i][0], points[i][1], 0};

        if (logMode & 1) pt[0] = LOGFUN (pt[0]);
        if (logMode & 2) pt[1] = LOGFUN (pt[1]);
        if (isnan (pt[0]) || isnan (pt[1]) || isinf (pt[0]) || isinf (pt[1]))
            continue;

        WORLD_TRANSFORM_DATAPOS_TO_BIN_HOT_LOOP_COMPUTE (pt, xi, yi, wzVal);

        if ((unsigned)xi < (unsigned)w && (unsigned)yi < (unsigned)h)
        {
            const int idx = yi * w + xi;
            ++bins[idx];
            if (wz[idx] < wzVal)
                wz[idx] = wzVal;
        }
    }
}

static void canvas_fun_2d_histogram_plus (WorldTransform *world, void *_points, size_t len, uint32_t logMode, CipCanvas *canvas)
{
    BENCHMARK_ADD_CHECKPOINT ("2d_histogram_plus");
    double (*restrict points)[2] = _points;
    int    *restrict bins        = canvas->bins;
    double *restrict wz          = canvas->wz;

    int w = canvas->w;
    int h = canvas->h;

    WORLD_TRANSFORM_DATAPOS_TO_BIN_HOT_LOOP_INIT (world);

    for (size_t i=0; i<len; i++)
    {
        double pt[3] = {points[i][0], points[i][1], 0};

        if (logMode & 1) pt[0] = LOGFUN (pt[0]);
        if (logMode & 2) pt[1] = LOGFUN (pt[1]);
        if (isnan (pt[0]) || isnan (pt[1]) || isinf (pt[0]) || isinf (pt[1]))
            continue;

        WORLD_TRANSFORM_DATAPOS_TO_BIN_HOT_LOOP_COMPUTE (pt, xi, yi, wzVal);

        int xx[9] = { 0,  0, -2, -1, 0, 1, 2, 0, 0};
        int yy[9] = {-2, -1,  0,  0, 0, 0, 0, 1, 2};

        for (int j=0; j<9; j++)
        {
            int xp = xi+xx[j];
            int yp = yi+yy[j];
            if ((unsigned)xp < (unsigned)w && (unsigned)yp < (unsigned)h)
            {
                const int idx = yp * w + xp;
                ++bins[idx];
                if (wz[idx] < wzVal)
                    wz[idx] = wzVal;
            }
        }
    }
}

static void canvas_fun_2d_line (WorldTransform *world, void *_points, size_t len, uint32_t logMode, CipCanvas *canvas)
{
    BENCHMARK_ADD_CHECKPOINT ("2d_line");
    double (*restrict points)[2] = _points;

    const int w = canvas->w;
    const int h = canvas->h;

    int lastXi = -1;
    int lastYi = -1;

    WORLD_TRANSFORM_DATAPOS_TO_BIN_HOT_LOOP_INIT (world);

    for (size_t i=0; i<len; i++)
    {
        double pt[3] = {points[i][0], points[i][1], 0};

        if (logMode & 1) pt[0] = LOGFUN (pt[0]);
        if (logMode & 2) pt[1] = LOGFUN (pt[1]);
        if (isnan (pt[0]) || isnan (pt[1]) || isinf (pt[0]) || isinf (pt[1]))
            continue;

        WORLD_TRANSFORM_DATAPOS_TO_BIN_HOT_LOOP_COMPUTE (pt, xi, yi, wzVal);

        if ((unsigned)xi < (unsigned)w && (unsigned)yi < (unsigned)h)
        {
            if (lastXi >= 0)
                cip_canvas_line (canvas, lastXi, lastYi, xi, yi, wzVal);
            lastXi = xi;
            lastYi = yi;
        }
        else
        {
            lastXi = -1;
            lastYi = -1;
        }
    }
}

static void canvas_fun_2d_stair (WorldTransform *world, void *_points, size_t len, uint32_t logMode, CipCanvas *canvas)
{
    BENCHMARK_ADD_CHECKPOINT ("2d_line");
    double (*restrict points)[2] = _points;

    const int w = canvas->w;
    const int h = canvas->h;

    int lastXi = -1;
    int lastYi = -1;

    WORLD_TRANSFORM_DATAPOS_TO_BIN_HOT_LOOP_INIT (world);

    for (size_t i=0; i<len; i++)
    {
        double pt[3] = {points[i][0], points[i][1], 0};

        if (logMode & 1) pt[0] = LOGFUN (pt[0]);
        if (logMode & 2) pt[1] = LOGFUN (pt[1]);
        if (isnan (pt[0]) || isnan (pt[1]) || isinf (pt[0]) || isinf (pt[1]))
            continue;

        WORLD_TRANSFORM_DATAPOS_TO_BIN_HOT_LOOP_COMPUTE (pt, xi, yi, wzVal);

        if ((unsigned)xi < (unsigned)w && (unsigned)yi < (unsigned)h)
        {
            if (lastXi >= 0)
            {
                int xi0 = lastXi;
                int yi0 = lastYi;
                int xi1 = xi;
                int yi1 = yi;
                cip_canvas_line (canvas, xi0, yi0, xi1, yi0, wzVal);
                cip_canvas_line (canvas, xi1, yi0, xi1, yi1, wzVal);
            }
            lastXi = xi;
            lastYi = yi;
        }
        else
        {
            lastXi = -1;
            lastYi = -1;
        }
    }
}

static void canvas_fun_2d_thick_line (WorldTransform *world, void *_points, size_t len, uint32_t logMode, CipCanvas *canvas)
{
    BENCHMARK_ADD_CHECKPOINT ("2d_line");
    double (*restrict points)[2] = _points;

    const int w = canvas->w;
    const int h = canvas->h;

    int lastXi = -1;
    int lastYi = -1;

    WORLD_TRANSFORM_DATAPOS_TO_BIN_HOT_LOOP_INIT (world);

    for (size_t i=0; i<len; i++)
    {
        double pt[3] = {points[i][0], points[i][1], 0};

        if (logMode & 1) pt[0] = LOGFUN (pt[0]);
        if (logMode & 2) pt[1] = LOGFUN (pt[1]);
        if (isnan (pt[0]) || isnan (pt[1]) || isinf (pt[0]) || isinf (pt[1]))
            continue;

        WORLD_TRANSFORM_DATAPOS_TO_BIN_HOT_LOOP_COMPUTE (pt, xi, yi, wzVal);

        if ((unsigned)xi < (unsigned)w && (unsigned)yi < (unsigned)h)
        {
            if (lastXi >= 0)
            {
                cip_canvas_line (canvas, lastXi,   lastYi,   xi,   yi,   wzVal);
                cip_canvas_line (canvas, lastXi+1, lastYi,   xi+1, yi,   wzVal);
                cip_canvas_line (canvas, lastXi-1, lastYi,   xi-1, yi,   wzVal);
                cip_canvas_line (canvas, lastXi,   lastYi+1, xi,   yi+1, wzVal);
                cip_canvas_line (canvas, lastXi,   lastYi-1, xi,   yi-1, wzVal);
            }
            lastXi = xi;
            lastYi = yi;
        }
        else
        {
            lastXi = -1;
            lastYi = -1;
        }
    }
}

static void canvas_fun_3d_histogram (WorldTransform *world, void *_points, size_t len, uint32_t logMode, CipCanvas *canvas)
{
    BENCHMARK_ADD_CHECKPOINT ("3d_histogram");
    double (*restrict points)[3] = _points;
    int     *restrict bins       = canvas->bins;
    double  *restrict wz         = canvas->wz;

    const int w = canvas->w;
    const int h = canvas->h;

    WORLD_TRANSFORM_DATAPOS_TO_BIN_HOT_LOOP_INIT (world);

    BENCHMARK_ADD_CHECKPOINT ("for loop");
    for (size_t i = 0; i < len; ++i)
    {
        WORLD_TRANSFORM_DATAPOS_TO_BIN_HOT_LOOP_COMPUTE (points[i], xi, yi, wzVal);

        if ((unsigned)xi < (unsigned)w && (unsigned)yi < (unsigned)h)
        {
            const int idx = yi * w + xi;
            ++bins[idx];
            wz[idx] = wzVal;
        }
    }
}

static void canvas_fun_3d_line (WorldTransform *world, void *_points, size_t len, uint32_t logMode, CipCanvas *canvas)
{
    BENCHMARK_ADD_CHECKPOINT ("3d_line");
    double (*restrict points)[3] = _points;

    const int w = canvas->w;
    const int h = canvas->h;

    int lastXi = -1;
    int lastYi = -1;

    WORLD_TRANSFORM_DATAPOS_TO_BIN_HOT_LOOP_INIT (world);

    for (size_t i=0; i<len; i++)
    {
        WORLD_TRANSFORM_DATAPOS_TO_BIN_HOT_LOOP_COMPUTE (points[i], xi, yi, wzVal);

        if ((unsigned)xi < (unsigned)w && (unsigned)yi < (unsigned)h)
        {
            if (lastXi >= 0)
                cip_canvas_line (canvas, lastXi, lastYi, xi, yi, wzVal);
            lastXi = xi;
            lastYi = yi;
        }
        else
        {
            lastXi = -1;
            lastYi = -1;
        }
    }
}

//    // waterfall
//    if (i0 < 0)
//    {
//        print_warning ("truncating");
//        i0 = 0;
//    }
//    if (isnan (xy[0]) || isnan (xy[1]))
//    {
//        // flush row
//        for (uint32_t yi=h-1; yi>0; yi--)
//            for (uint32_t xi=0; xi<w; xi++)
//                bins[yi*w + xi] = bins[(yi-1) * w + xi];

//        // construct new row
//        int lastNonZeroXi = -1;
//        for (uint32_t xi=0; xi<w; xi++)
//        {
//            if (canvas->counts[xi] > 1e-5)
//            {
//                double avg = sums[xi] / counts[xi];
//                double s = canvas->world.scaleMtx[1][1];
//                double ymin = canvas->world.centerPos[1] - s;
//                double ymax = canvas->world.centerPos[1] + s;
//                double w = (avg - ymin) / (ymax - ymin);

//                if (lastNonZeroXi < 0)
//                    lastNonZeroXi = xi-1;
//                for (int xik=lastNonZeroXi+1; xik<=xi; xik++)
//                    bins[xik] = w * 1000; // FIXME: 1000 is the resolution of the color scheme

//                //print_debug ("sums[xi]: %f counts[xi]: %f ymin: %f, ymax: %f avg: %f => w: %f => bins[%d]: %d",
//                //sums[xi], counts[xi], ymin, ymax, avg, w, xi, bins[xi]);
//                sums[xi]   = 0.0;
//                counts[xi] = 0.0;
//                lastNonZeroXi = xi;
//            }
//        }

void canvas_functions_register (void)
{
    cip_register_canvas_fun (1, 'h', canvas_fun_1d_histogram);
    cip_register_canvas_fun (2, 'p', canvas_fun_2d_histogram_point);
    cip_register_canvas_fun (2, '+', canvas_fun_2d_histogram_plus);
    cip_register_canvas_fun (2, 'l', canvas_fun_2d_line);
    cip_register_canvas_fun (2, 't', canvas_fun_2d_thick_line);
    cip_register_canvas_fun (2, 's', canvas_fun_2d_stair);
    cip_register_canvas_fun (3, 'h', canvas_fun_3d_histogram);
    cip_register_canvas_fun (3, 'l', canvas_fun_3d_line);
}
