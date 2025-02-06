#include "common.h"

#include "randlib.h"
#include "midilib.h"
#include "cinterplot.h"
#include "audio.h"

char *stateFile = "/Users/manne/.cinterplot/ambientsounds";

#ifndef randf
#define randf() ((double) rand () / ((double) RAND_MAX+1))
#endif

#define N 1000000

#define PLOTLEN 65536
#define UPDATE_INTERVAL (65536)

enum
{
    GRAPH_TD1,
    GRAPH_TD2,

    GRAPH_FD1,
    GRAPH_FD2,

    GRAPH_SIZE
};

typedef struct Kernel
{
    float term1;
    float term2;
    float re[2];
    float im[2];
    float cosTheta;
    float sinTheta;
    float amplitude;
    float peakAmplitude;
    int reltime;
} Kernel;


long audioCounter = 0;
static uint32_t bordered = 1;
static uint32_t margin = 4;
static CipGraph *graphs[GRAPH_SIZE];
static CipState *cs = NULL;
double volume = 1;
double decay = 2;
double decay2 = 0.9;

double k1 = 0.01;
double k2 = 0.5e-3;
double k3 = 1.0;
double k4 = 1.0;


#define FEATLEN 256
#define FEATURES_HIST_SIZE 4096
int featureIndex = 0;
double featureHist[FEATURES_HIST_SIZE][FEATLEN] = {0};

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

int init = 1;

#define F 1.01
int on_encoder (void *twisterDev, int encoder, int dir)
{
    double f = (dir > 0) ? F : 1.0 / F;
    switch (encoder)
    {
     case 0: volume *= f; print_debug ("volume %f", volume); break;
     case 1: decay *= f; print_debug ("decay %f", decay); break;
     case 2: if (decay2 < 1) decay2 *= f; else decay2 = 2 - (2 - decay2) / f; print_debug ("decay2 %f", decay2); break;
     case 4: k1 *= f; print_debug ("k1 %f", k1); break;
     case 5: k2 *= f; print_debug ("k2 %f", k2); break;
     case 6: k3 *= f; print_debug ("k3 %f", k3); break;
     case 7: k4 *= f; print_debug ("k4 %f", k4); break;
     case 15: init = 1; break;
     default: print_debug ("%d: %+d %f", encoder, dir, f); break;
    }
    return 1;
}

int twister_poll (void *twisterDev)
{
    static int lastDirs[16] = {0};
    static double lastTsps[16] = {0};
    uint8_t buf[16];
    int len = 16;
    while (midi_get_message (twisterDev, & buf[0], & len))
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
                 on_encoder (twisterDev, encoder, dir);
                 break;
             }
         case 0xb1:
             {
                 int encoder = buf[1];
                 int pressed = (buf[2] == 0x7f);
                 on_press (twisterDev, encoder, pressed);
                 break;
             }
         case 0xb3:
             {
                 int button = buf[1] - 8;
                 int pressed = (buf[2] == 0x7f);
                 on_button (button, pressed);
                 break;
             }
         default: printf ("%02x %02x %02x\n", buf[0], buf[1], buf[2]);
        }
    }
    return 0;
}

int on_keyboard (CipState *_cs, int key, int mod, int pressed, int repeat)
{
    if (repeat)
        return 0;

    switch (key)
    {
     default: break;
    }
    return 0;
}

#define GET_DATA_POS_X(hist,xi) (((double) xi / (hist->w-1)) * (hist->dataRange.x1 - hist->dataRange.x0) + hist->dataRange.x0)
#define GET_DATA_POS_Y(hist,yi) (((double) yi / (hist->h-1)) * (hist->dataRange.y1 - hist->dataRange.y0) + hist->dataRange.y0)

SubWindow *featWin = NULL;

#define NUM_KERNELS 20
Kernel kernels[NUM_KERNELS] = {0};

float *generate_next_samples (void)
{
    static float samples[2] = {0};
    double sums[2] = {0};

    float deltaT = 1.0f / AUDIO_FREQUENCY;
    for (int ki=0; ki<NUM_KERNELS; ki++)
    {
        Kernel *k = & kernels[ki];

        k->amplitude += k->term1;
        if (k->amplitude > k->peakAmplitude)
            k->term1 = k->term2;

        for (int i=0; i<2; i++)
        {
            sums[i] += k->amplitude * k->re[i];

            float re = k->cosTheta * k->re[i] - k->sinTheta * k->im[i];
            float im = k->sinTheta * k->re[i] + k->cosTheta * k->im[i];

            k->re[i] = re;
            k->im[i] = im;
        }

        if ((k->amplitude <= 0 && k->term1 <= 0 ) || init)
        {
            const int notesRight[] = {11,1,11,8,8,8,6,8,9,8,9,6,11,8,4,1,6,3,4,3,1,11,4,8,11,1,11,9,8,6,8,1,4,6,9,8,6,8,1,4,6,9,8,6,4,3,1,3,4,3,1,11,4,8,11,1,11,9,8,6,8,1,4,6,9,8,6,8,1,4,6,9,8,6,8,11,0,8,8,6,4,6,4,8,8,6,4,6,4,8,8,6,4,6,8,9,8,4,4,4,1,11,11,4,4,1,11,11,4,4,1,4,6,3,1,4,3,8,8,6,4,6,4,8,8,6,4,6,4,8,8,6,4,6,8,9,8,4,4,4,1,11,11,4,4,1,11,11,4,4,1,4,6,3,8,9,8,6,4,1,4,4,1,4,6,8,8,9,8,6,4,1,1,4,4,1,4,6,3,8,9,8,6,4,1,4,4,1,4,6,8,8,9,8,6,4,1,1,4,4,1,4,6,3,1,1,4,8,9,9,8,4,3,1,11,4,8,11,1,11,9,8,6,8,1,4,6,9,8,6,8,1,4,4,6,9,8,6,4,3,1,3,4,3,1,11,4,8,11,1,11,9,8,6,8,1,4,6,9,8,6,8,1,4,4,6,9,8,6,8,11,0,8,8,6,4,6,4,8,8,6,4,6,4,8,8,6,4,6,8,9,8,4,4,4,1,11,11,4,4,1,11,11,4,4,1,4,6,3,1,4,3,8,8,6,4,6,4,8,8,6,4,6,4,8,8,6,4,6,8,9,8,4,4,4,1,11,11,4,4,1,11,11,4,4,1,4,6,3,8,9,8,6,4,4,4,1,4,6,8,8,9,8,6,4,1,1,4,4,1,4,6,3,8,9,8,6,4,1,4,4,1,4,6,8,8,9,8,6,4,1,1,4,4,1,4,6,3,1,1,4,8,9,9,8};
            const int notesLeft[] = {3,8,8,8,11,11,11,4,4,4,8,8,8,4,4,4,8,8,8,4,4,4,3,3,3,8,8,8,11,11,11,4,4,4,8,8,8,4,4,4,8,8,8,4,4,4,3,8,8,11,11,11,4,4,4,3,3,3,4,4,4,11,11,11,4,4,4,3,8,8,8,8,11,11,11,4,4,4,3,3,3,4,4,4,11,11,11,4,4,4,3,8,8,11,11,4,4,3,3,8,8,11,11,4,4,3,3,8,8,11,11,4,4,3,3,8,8,11,11,4,4,3,8,8,11,11,4,4,3,3,8,8,11,11,4,4,3,8,8,8,11,11,11,4,4,4,8,8,8,4,4,4,8,8,8,4,4,4,3,3,3,8,8,8,11,11,11,4,4,4,8,8,8,4,4,4,8,8,8,4,4,4,3,8,8,11,11,11,4,4,4,3,3,3,4,4,4,11,11,11,4,4,4,3,8,8,8,11,11,11,4,4,4,3,3,3,4,4,4,11,11,11,4,4,4,3,8,8,11,11,4,4,3,3,8,8,11,11,4,4,3,3,8,8,11,11,4,4,3,3,8,8,11,11,4,4,8,8,11,11,4,4,3,3,8,8,11,11,4,4,3,8};

            static int i=0;
            static float bf[2] = {0};
            if (ki == 0)
            {
                int nL = sizeof (notesLeft) / sizeof (notesLeft[0]);
                int nR = sizeof (notesRight) / sizeof (notesRight[0]);
                bf[0] = 440.0f * powf (2.0f, (notesRight[i % nR] - 9.0f) / 12);
                bf[1] = 440.0f * powf (2.0f, (notesLeft[i % nL] - 9.0f - 24) / 12);
                print_debug ("bf: {%f,%f}", bf[0],bf[1]);
                i++;
            }

            float frequency = bf[ki % 2] * (1.0 + 0.01 * ki) * k1 + k2 * randf ();
            float omega = 2 * M_PI * frequency;
            float theta = omega * deltaT;
            k->cosTheta = cosf (theta);
            k->sinTheta = sinf (theta);

            float phase1 = randf () * (float) (2 * M_PI);
            float phase2 = randf () * (float) (2 * M_PI);

            k->re[0] = cosf (phase1);
            k->im[0] = sinf (phase1);
            k->re[1] = cosf (phase2);
            k->im[1] = sinf (phase2);

            k->peakAmplitude = 20.0f / sqrt(frequency) * (1.1 + cos (ki*0.01));
            k->amplitude = 0;

            int n1 = AUDIO_FREQUENCY * 0.02 * k3;
            int n2 = AUDIO_FREQUENCY * 0.6 * ((ki % 2) + 1) * k4;
            k->term1 =   (k->peakAmplitude) / n1;
            k->term2 = - (k->peakAmplitude) / n2;
        }
    }
    init = 0;

    samples[0] = sums[0] * (1.0 / NUM_KERNELS) * volume;
    samples[1] = sums[1] * (1.0 / NUM_KERNELS) * volume;
    audioCounter++;
    return samples;
}

int user_main (int argc, char **argv, CipState *_cs)
{
    audio_init ();
    cs = _cs;
    randlib_init (0);
    void *twisterDev = NULL;
    twisterDev = midi_init ("Midi Fighter Twister");
    cip_set_crosshair_enabled (cs, 0);

    uint32_t nRows = GRAPH_SIZE / 2;
    uint32_t nCols = 2;

    if (cip_make_sub_windows (cs, nRows, nCols, bordered, margin) < 0)
        return 1;

    //cip_set_app_keyboard_callback (cs, on_keyboard);

    for (int gi=0; gi<GRAPH_SIZE; gi++)
    {
        switch (gi)
        {
         case GRAPH_TD1:
             {
                 graphs[gi] = cip_graph_new (PLOTLEN);
                 cip_graph_attach (cs, graphs[gi], gi, NULL, 'l', "red", 4);

                 for (int j=0; j<PLOTLEN; j++)
                     cip_graph_add_point (graphs[gi], j, 0.0);

                 SubWindow *sw = cip_get_sub_window (cs, gi);
                 cip_continuous_scroll_enable (sw);
                 cip_set_x_range (sw, 0, PLOTLEN - 1, 1);
                 cip_set_y_range (sw, -1.5, 1.5, 1);
                 break;
             }
         case GRAPH_TD2:
             {
                 graphs[gi] = cip_graph_new (PLOTLEN);
                 cip_graph_attach (cs, graphs[gi], gi, NULL, 'l', "red", 4);

                 for (int j=0; j<PLOTLEN; j++)
                     cip_graph_add_point (graphs[gi], j, 0.0);

                 SubWindow *sw = cip_get_sub_window (cs, gi);
                 cip_set_x_range (sw, 0, PLOTLEN - 1, 1);
                 cip_set_y_range (sw, -0.5, 0.5, 1);
                 break;
             }
         case GRAPH_FD1:
             {
                 graphs[gi] = cip_graph_new (PLOTLEN);
                 cip_graph_attach (cs, graphs[gi], gi, NULL, 'l', "yellow", 4);

                 for (int j=0; j<PLOTLEN; j++)
                     cip_graph_add_point (graphs[gi], j, 0.0);

                 SubWindow *sw = cip_get_sub_window (cs, gi);
                 cip_set_x_range (sw, 0, PLOTLEN - 1, 1);
                 cip_set_y_range (sw, -1, 1, 1);
                 break;
             }
         case GRAPH_FD2:
             {
                 graphs[gi] = cip_graph_new (FEATLEN);
                 cip_graph_attach (cs, graphs[gi], gi, NULL, 'l', "#4444ff", 4);

                 for (int j=0; j<FEATLEN; j++)
                     cip_graph_add_point (graphs[gi], j, 0.0);

                 SubWindow *sw = cip_get_sub_window (cs, gi);
                 cip_set_x_range (sw, 0, PLOTLEN - 1, 1);
                 cip_set_y_range (sw, 0, 2 * M_PI, 1);
                 break;
             }
         default:
             exit_error ("unhandled graph id initialisation");
        }
    }

    while (cip_is_running (cs))
    {
        midi_connect (twisterDev);
        twister_poll (twisterDev);

        float *samples = generate_next_samples ();
        audio_out_samples_push (samples);
        cip_graph_add_point (graphs[GRAPH_TD1], audioCounter, samples[0]);

        if (audioCounter % (AUDIO_FREQUENCY / 16) == 0)
            cip_redraw_async (cs);
    }

    audio_close ();
    return 0;
}
