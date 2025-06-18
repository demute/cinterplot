#include "cinterplot_common.h"
#include "randlib.h"
#include "cinterplot.h"

void rotate_x (double mtx[3][3], double theta, int order);
void rotate_y (double mtx[3][3], double theta, int order);
void rotate_z (double mtx[3][3], double theta, int order);

typedef struct State
{
    CipGraph *graph;
    double x;
    double y;
    double z;
    double dx;
    double dy;
    double dz;
} State;

int user_main (int argc, char **argv, CipState *cs)
{
    randlib_init (0);
    const uint32_t nRows = 1;
    const uint32_t nCols = 1;
    uint32_t bordered = 1;
    uint32_t margin = 4;

    int nColors = 8;
    int numPerColor = 48;
    int n = nColors * numPerColor;
    int masklen = 200;
    State *states = malloc (sizeof (states[0]) * n);
    assert (states);

    cip_set_crosshair_enabled (cs, 0);

    if (cip_make_sub_windows (cs, nRows, nCols, bordered, margin) < 0)
        return 1;

    for (int si=0; si<n; si++)
    {
        states[si].graph = NULL;
        states[si].x  = 0;
        states[si].y  = 0;
        states[si].z  = 0;
        states[si].dx = runif (0.5,2);
        states[si].dy = 0;
        states[si].dz = 0;

        if (si < nColors)
        {
            int col[9];
            for (int i=0; i<9; i++)
                col[i] = (rand () & 0xff) | (0x22 << (i%3));
            char colorStr[64];
            sprintf (colorStr, "#%02x%02x%02x #%02x%02x%02x #%02x%02x%02x",
                     col[0], col[1], col[2], col[3], col[4], col[5], col[6], col[7], col[8]);

            print_debug ("color: %s", colorStr);
            states[si].graph = cip_graph_new (3, numPerColor * masklen);
            cip_graph_attach (cs, states[si].graph, 0, NULL, 'p', colorStr, 5);
        }
    }

    while (cip_is_running (cs))
    {
        for (int si=0; si<n; si++)
        {
            CipGraph *graph = states[si % nColors].graph;
            State *s = & states[si];
            s->x += 1e-3 * s->dx;
            s->y += 1e-3 * s->dy;
            s->z += 1e-3 * s->dz;
            cip_graph_add_3d_point (graph, s->x, s->y, s->z);
            double d2 = sqrt (s->x * s->x + s->y * s->y + s->z * s->z);
            if (0)
            {
                s->dx += 0.1*runif (-1-s->x,1-s->x);
                s->dy += 0.1*runif (-1-s->y,1-s->y);
                s->dz += 0.1*runif (-1-s->z,1-s->z);
            }
            else
            {
                if (runif (0,1) < 0.02 * d2)
                {
                    double r = runif (0,2);
                    double a = fabs (s->dx + s->dy + s->dz);
                    if (s->dx != 0)
                    {
                        s->dx = 0;
                        s->dy = (r < 1) ? a : 0;
                        s->dz = (r < 1) ? 0 : a;
                    }
                    else if (s->dy != 0)
                    {
                        s->dx = (r < 1) ? a : 0;
                        s->dy = 0;
                        s->dz = (r < 1) ? 0 : a;
                    }
                    else
                    {
                        s->dx = (r < 1) ? a : 0;
                        s->dy = (r < 1) ? 0 : a;
                        s->dz = 0;
                    }

                    if (s->x > 0)
                        s->dx *= -1;

                    if (s->y > 0)
                        s->dy *= -1;

                    if (s->z > 0)
                        s->dz *= -1;
                }
            }
        }

        CipSubWindow *sw = cip_get_sub_window (cs, 0);
        rotate_y (sw->rotMatrix, 0.0003,  0);
        //rotate_y (sw->rotMatrix, 0.00019, 1);
        //rotate_z (sw->rotMatrix, 0.0001003, 0);
        //rotate_y (sw->rotMatrix, 0.00012,   1);

        usleep (2000);
        cip_redraw_async (cs);
    }

    return 0;
}
