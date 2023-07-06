#include "common.h"
#include "stream_buffer.h"
#include "randlib.h"
#include "midilib.h"
#include "plotlib.h"
#include "oklab.h"

#ifndef randf
#define randf() ((double) rand () / ((double) RAND_MAX+1))
#endif

int running = 1;
char *executable = NULL;

void hsv2rgb (HSV *hsv, RGB *rgb)
{
    float h = hsv->h;
    float s = hsv->s;
    float v = hsv->v;

    float hh = 6 * h;
    int i = (int) hh;
    float ff = hh - i;
    float p = v * (1.0f - s);
    float q = v * (1.0f - (s * ff));
    float t = v * (1.0f - (s * (1.0f - ff)));

    float r,g,b;
    switch(i) {
     case 0: r = v; g = t; b = p; break;
     case 1: r = q; g = v; b = p; break;
     case 2: r = p; g = v; b = t; break;
     case 3: r = p; g = q; b = v; break;
     case 4: r = t; g = p; b = v; break;
     case 5: r = v; g = p; b = q; break;
     default: exit_error ("bug");
    }

    rgb->r = r;
    rgb->g = g;
    rgb->b = b;
}

uint32_t rgb2color (RGB *rgb)
{
    float r = rgb->r;
    float g = rgb->g;
    float b = rgb->b;

    if (r < 0) r = 0;
    if (g < 0) g = 0;
    if (b < 0) b = 0;

    if (r > 1) r = 1;
    if (g > 1) g = 1;
    if (b > 1) b = 1;

    int ri = (int) (255 * r);
    int gi = (int) (255 * g);
    int bi = (int) (255 * b);

    return MAKE_COLOR (ri,gi,bi);
}

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


static int on_mouse_pressed (struct plotlib_config_t *config, int button)
{
    return 0;
}

static int on_mouse_released (struct plotlib_config_t *config)
{
    return 0;
}

static int on_mouse_motion (struct plotlib_config_t *config, int xi, int yi)
{
    return 0;
}

void plot_callback (PlotlibConfig *config, uint32_t *pixels, int w, int h)
{
}

int keyboard_callback (PlotlibConfig *config, int key, int pressed, int repeat)
{
    print_debug ("key: %c, pressed: %d, repeat: %d", key, pressed, repeat);
    switch (key)
    {
     case 'q': running = 0; break;
    }
    return 0;
}

void signal_handler (int sig)
{
    running = 0;
}

PlotlibConfig *ui_init (void)
{
    static PlotlibConfig config = {0};
    config.windowWidth = 600;
    config.windowHeight = 400;
    config.on_mouse_pressed  = on_mouse_pressed;
    config.on_mouse_released = on_mouse_released;
    config.on_mouse_motion   = on_mouse_motion;
    config.on_keyboard       = keyboard_callback;
    config.callback          = plot_callback;

    plotlib_init (& config);

    signal (SIGINT, signal_handler);

    return & config;
}

int main (int argc, char **argv)
{
    executable = argv[0];
    randlib_init (0);
    PlotlibConfig *plotlibConfig = ui_init ();

    double fps = 30;
    double periodTime = 1.0 / fps;
    double lastFrameTsp = 0;

    void *twisterDev = NULL;
    twisterDev = midi_init ("Midi Fighter Twister");
    midi_connect (twisterDev);

    while (running)
    {
        int didNothing = 1;
        while (twister_poll (twisterDev))
        {
            didNothing = 0;
        }

        double tsp = get_time ();
        if (tsp - lastFrameTsp > periodTime)
        {
            lastFrameTsp = tsp;
            plotlib_main_loop (plotlibConfig, 1);
            didNothing = 0;
        }

        if (didNothing)
            usleep (1000);
    }
}
