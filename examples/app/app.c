#include "common.h"
#include "randlib.h"
#include "midilib.h"
#include "cinterplot.h"

#ifndef randf
#define randf() ((double) rand () / ((double) RAND_MAX+1))
#endif

int on_press (void *twisterDev, int encoder, int pressed)
{
    print_debug ("encoder: %d, pressed: %d", encoder, pressed);
    return 0;
}

int on_button (int button, int pressed)
{
    print_debug ("button: %d pressed: %d", button, pressed);
    return 1;
}

int on_encoder (void *twisterDev, int encoder, int dir)
{
    print_debug ("en: %d dir: %d", encoder, dir);
    return 0;
}

int twister_poll (void *twisterDev)
{
    static int lastDirs[16] = {0};
    static double lastTsps[16] = {0};
    uint8_t buf[16];
    int len = 16;
    if (midi_get_message (twisterDev, & buf[0], & len))
    {
        switch (buf[0])
        {
         case 0xb0:
             {
                 int encoder = buf[1];
                 int dir     = ((buf[2] == 0x3f) ? -1 : 1);
                 double tsp  = get_time ();
                 double elapsed = tsp - lastTsps[encoder];
                 lastTsps[encoder] = tsp;
                 if (lastDirs[encoder] != dir || elapsed > 4.0)
                 {
                     lastDirs[encoder] = dir;
                     break;
                 }
                 return on_encoder (twisterDev, encoder, dir);
             }
         case 0xb1:
             {
                 int encoder = buf[1];
                 int pressed = (buf[2] == 0x7f);
                 return on_press (twisterDev, encoder, pressed);
             }
         case 0xb3:
             {
                 int button = buf[1] - 8;
                 int pressed = (buf[2] == 0x7f);
                 return on_button (button, pressed);
             }
         default: printf ("%02x %02x %02x\n", buf[0], buf[1], buf[2]);
        }
    }
    else
    {
        usleep (100);
    }
    return 0;
}

int user_main (int argc, char **argv, CinterState *cs)
{
    randlib_init (0);
    //void *twisterDev = NULL;
    //twisterDev = midi_init ("Midi Fighter Twister");
    //midi_connect (twisterDev);

    //cinterplot_continuous_scroll_enable (cs);
    const uint32_t nRows = 2;
    const uint32_t nCols = 3;
    const uint32_t n = nRows * nCols;
    uint32_t bordered = 1;
    uint32_t margin = 4;

    char *colorSchemes[6] =
    {
        "red orange white",
        "purple blue white",
        "blue yellow white",
        "brown turquoise white",
        "violet indigo white",
        "chocolate brown red yellow"
    };

    if (make_sub_windows (cs, nRows, nCols, bordered, margin) < 0)
        return 1;

    CinterGraph *sineGraph[n];
    char plotType[6] = {'p','l','s','p','l','s'};
    for (int i=0; i<n; i++)
    {
        sineGraph[i] = graph_new (1000000, 1);
        graph_attach (cs, sineGraph[i], (uint32_t) i, plotType[i], colorSchemes[i % 6]);
    }

    double v = 0;
    uint64_t t = 0;
    double a[n] = {0};
    while (cinterplot_is_running (cs))
    {
        usleep (0);

        for (int i=0; i<n; i++)
        {
            v += (1.0 / 1024.0) * 1.3;
            if (v > 5)
                v -= 5;

            double f = 0.99;
            double *r = heavy_tail_ncube (n);
            a[i] = f * a[i] + (1-f) * r[i] * 0.1;
            double y = a[i] * sin (2 * M_PI * v * a[i] * (i+1));
            double x = a[i] * cos (2 * M_PI * v * a[i] * (i+1));
            //double x = t;
            graph_add_point (sineGraph[i], x, y);
        }
        t++;
        cinterplot_redraw_async (cs);
    }

    return 0;
}
