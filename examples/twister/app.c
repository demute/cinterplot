#include "common.h"
#include "randlib.h"
#include "midilib.h"
#include "cinterplot.h"

#ifndef randf
#define randf() ((double) rand () / ((double) RAND_MAX+1))
#endif

#define N 1000000

static const uint32_t nRows = 2;
static const uint32_t nCols = 3;
static const uint32_t n = nRows * nCols;
static uint32_t bordered = 1;
static uint32_t margin = 4;
static CinterGraph **sineGraph;
static CinterState *cs = NULL;

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
    static double speed = 1.0;
    double f = (dir > 0) ? 1.01 : 1.0 / 1.01;
    speed *= f;

    sineGraph = safe_calloc (n, sizeof (*sineGraph));
    graph_remove_points (sineGraph[0]);
    for (int i=0; i<N; i++)
    {
        double x = i * 1e-3;
        double y = sin (x * 2 * M_PI * speed);
        graph_add_point (sineGraph[0], x, y);
    }
    print_debug ("en: %d dir: %d", encoder, dir);
    return 1;
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

int user_main (int argc, char **argv, CinterState *_cs)
{
    cs = _cs;
    randlib_init (0);
    void *twisterDev = NULL;
    twisterDev = midi_init ("Midi Fighter Twister");

    char *colorSchemes[6] =
    {
        "white blue black",
        "white",
        "white",
        "white",
        "white",
        "white",
    };

    if (make_sub_windows (cs, nRows, nCols, bordered, margin) < 0)
        return 1;

    char plotType[6] = {'p','l','s','p','l','s'};
    for (int i=0; i<n; i++)
    {
        sineGraph[i] = graph_new (N);
        graph_attach (cs, sineGraph[i], (uint32_t) i, NULL, plotType[i], colorSchemes[i % 6], 4);
    }

    while (cinterplot_is_running (cs))
    {
        midi_connect (twisterDev);
        if (twister_poll (twisterDev))
            cinterplot_redraw_async (cs);
        else
            usleep (1000);
    }

    return 0;
}
