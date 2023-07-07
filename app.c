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
    //randlib_init (0);
    //void *twisterDev = NULL;
    //twisterDev = midi_init ("Midi Fighter Twister");
    //midi_connect (twisterDev);

    uint32_t nRows = 2;
    uint32_t nCols = 2;
    uint32_t bordered = 1;
    uint32_t margin = 2;

    if (make_sub_windows (cs, nRows, nCols, bordered, margin) < 0)
        return 1;

    CinterGraph *sineGraph = graph_new (0, 0);
    double v = 0;
    for (int i=0; i<1024; i++)
    {
        v += (1.0 / 1024.0) * 0.1;
        double y = sin (2 * M_PI * v);
        double x = cos (2 * M_PI * v);
        graph_add_point (sineGraph, x, y);
    }

    graph_attach (cs, sineGraph, 0, 0, 'p', "red");

    while (cs->running)
    {
        print_debug ("do things here");
        sleep (1);
    }

    return 0;
}


