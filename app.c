#include "common.h"
#include "stream_buffer.h"
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

    uint32_t nRows = 2;
    uint32_t nCols = 3;
    uint32_t bordered = 1;
    uint32_t margin = 2;

    if (make_sub_windows (cs, nRows, nCols, bordered, margin) < 0)
        return 1;

    CinterGraph *sineGraph[6];
    for (int i=0; i<6; i++)
       sineGraph[i] = graph_new (500000, 1);

    graph_attach (cs, sineGraph[0], 0, 0, 'p', "red yellow green");
    graph_attach (cs, sineGraph[1], 0, 1, 'p', "white black white");
    graph_attach (cs, sineGraph[2], 0, 2, 'p', "black blue yellow");
    graph_attach (cs, sineGraph[3], 1, 0, 'p', "magenta purple");
    graph_attach (cs, sineGraph[4], 1, 1, 'p', "black red white");
    graph_attach (cs, sineGraph[5], 1, 2, 'p', "chocolate brown red");

    double v = 0;
    uint64_t t = 0;
    double a[6] = {0};
    while (cs->running)
    {
        usleep (1);
        while (cs->paused && cs->running)
            usleep (10000);

        for (int i=0; i<6; i++)
        {
            v += (1.0 / 1024.0) * 1.3;
            if (v > 1)
                v -= 1;

            double f = 0.99;
            double *r = heavy_tail_ncube (6);
            a[i] = f * a[i] + (1-f) * r[i] * 0.1;
            double y = a[i] * sin (2 * M_PI * v);
            //double x = cos (2 * M_PI * v);
            double x = t;
            graph_add_point (sineGraph[i], x, y);
        }
        t++;
        cs->redraw = 1;
    }

    return 0;
}


