#include "common.h"

#include "randlib.h"
#include "midilib.h"
#include "cinterplot.h"
#include "audio.h"
#include "kiss_fft.h"
#include "_kiss_fft_guts.h"

char *stateFile = "/Users/manne/.cinterplot/ambientsounds";

#ifndef randf
#define randf() ((double) rand () / ((double) RAND_MAX+1))
#endif

#define N 1000000

#define DCTLEN 16384
#define UPDATE_INTERVAL (1024)

enum
{
    GRAPH_TD,
    GRAPH_NOISE_TD,

    GRAPH_FD,
    GRAPH_NOISE_FD,

    GRAPH_FEAT,
    GRAPH_NOISE_FLT_FD,

    GRAPH_FEATFALL,
    GRAPH_NOISE_FLT_TD,

    GRAPH_SIZE
};

static uint32_t bordered = 1;
static uint32_t margin = 4;
static CipGraph *graphs[GRAPH_SIZE];
static CipState *cs = NULL;
double volume = 1;
double decay = 2;
double decay2 = 0.9;

kiss_fft_cpx *cx_in1  = NULL;
kiss_fft_cpx *cx_in2  = NULL;
kiss_fft_cpx *cx_out1 = NULL;
kiss_fft_cpx *cx_out2 = NULL;
kiss_fft_cpx *flt_in  = NULL;
kiss_fft_cpx *flt_out = NULL;

#define FEATLEN 256
#define FEATURES_HIST_SIZE 4096
kiss_fft_cfg melCfg;
kiss_fft_cpx *mel_in   = NULL;
kiss_fft_cpx *mel_out  = NULL;
int featureIndex = 0;
double featureHist[FEATURES_HIST_SIZE][FEATLEN] = {0};

static void kiss_fct (kiss_fft_cfg cfg, kiss_fft_cpx *fin, kiss_fft_cpx *fout)
{
    // assuming all imaginary numbers are zero

    int len = cfg->nfft;
    int halflen = len / 2;

    for (int i=0; i<len; i++)
    {
        fin[i].i = 0;
        fout[i].r = fin[i].r;
    }

    for (int i=0; i<halflen; i++)
    {
        fin[i].r = fout[i * 2].r;
        fin[len - 1 - i].r = fout[i * 2 + 1].r;
    }

    kiss_fft (cfg, fin, fout);

    for (int i=0; i<len; i++)
    {
        double theta = i * M_PI / (2 * len);
        double cosTheta = cos (theta);
        double sinTheta = sin (theta);
        double re = fout[i].r * cosTheta + fout[i].i * sinTheta;
        fout[i].r = re;
        fout[i].i = 0;
    }
}

static void kiss_ifct (kiss_fft_cfg cfg, kiss_fft_cpx *fin, kiss_fft_cpx *fout)
{
    int len = cfg->nfft;
    int halflen = len / 2;

    if (len > 0)
        fin[0].r /= 2;

    for (int i=0; i<len; i++)
    {
        double theta = i * M_PI / (2 * len);
        fin[i].i = -fin[i].r * sin (theta);
        fin[i].r =  fin[i].r * cos (theta);
    }

    kiss_fft (cfg, fin, fout);

    for (int i=0; i<halflen; i++)
    {
        fout[i * 2 + 0].i = fout[i].r;
        fout[i * 2 + 1].i = fout[len - 1 - i].r;
    }

    for (int i=0; i<len; i++)
        fout[i].r = fout[i].i / halflen;
}

void apply_features (void)
{
    int idx = (featureIndex + FEATURES_HIST_SIZE - 1) % FEATURES_HIST_SIZE;
    double *features = featureHist[idx];
    for (int i=0; i<FEATLEN; i++)
    {
        mel_in[i].r = features[i];
        mel_in[i].i = 0;
    }

    kiss_ifct (melCfg, mel_in, mel_out);

    for (int i=0; i<DCTLEN; i++)
    {
        double f = i * (((double) AUDIO_FREQUENCY) / DCTLEN);
        double fMin = 0;
        double fMax = AUDIO_FREQUENCY;
        double bucket = (f - fMin) / (fMax - fMin) * FEATLEN;
        int b0 = (int) floor (bucket);
        int b1 = b0 + 1;
        double w0 = 1.0 - (bucket - b0);
        double w1 = 1.0 - w0;

        float mag = 0;
        if (b0 < FEATLEN && b1 < FEATLEN)
            mag = w0 * mel_out[b0].r + w1 * mel_out[b1].r;
        else if (b0 < FEATLEN)
            mag = mel_out[b0].r;
        float coeff = (mag);
        //if (coeff < 0)
        //    coeff = 0;
        //coeff = sqrtf (coeff);
        //coeff = cx_out1[i].r;
        //coeff = 1;
        //float coeff = mag;

        flt_in[i].r = coeff * (cx_out2[i].r);
        flt_in[i].i = coeff * (cx_out2[i].i);
    }
}

void compute_features ()
{
    double buckets[FEATLEN] = {0};
    double weights[FEATLEN] = {0};
    for (int i=1; i<DCTLEN; i++)
    {
        //double mag = log1p (cx_out1[i].r * cx_out1[i].r);
        double x = cx_out1[i].r;
        //double sx = SIGN (x);
        double ax = ABS (x);
        double mag = log1p (ax);
        //mag = angle;

        double f = i * (((double) AUDIO_FREQUENCY) / DCTLEN);
        double fMin = 0;
        double fMax = AUDIO_FREQUENCY / 2;
        double bucket = (f - fMin) / (fMax - fMin) * FEATLEN;
        int b0 = (int) floor (bucket);
        int b1 = b0 + 1;
        double w0 = 1.0 - (bucket - b0);
        double w1 = 1.0 - w0;

        if (b0 > FEATLEN - 1)
        {
            print_debug ("break at i:%d", i);
            break;
        }

        //print_debug ("f: %f melsPerBucket: %f fMax: %f bucket: %f => b0:%d w0:%f b1:%d w1:%f mag:%f", f, melsPerBucket, fMax, bucket, b0, w0, b1, w1, mag);

        if (b0 >= 0)
        {
            buckets[b0] += w0 * mag;
            weights[b0] += w0;
        }

        if (b1 < FEATLEN)
        {
            buckets[b1] += w1 * mag;
            weights[b1] += w1;
        }

    }

    for (int i=0; i<FEATLEN; i++)
    {
        //print_debug ("s:%f w:%f => a:%f", buckets[i], weights[i], buckets[i] / weights[i]);
        buckets[i] = (weights[i] > 0) ? (buckets[i] / weights[i]) : 0;
    }

    for (int i=0; i<FEATLEN; i++)
    {
        mel_in[i].r = buckets[i];
        mel_in[i].i = 0;
    }
    kiss_ifct (melCfg, mel_in, mel_out);

    //double *lastFeatures = featureHist[(featureIndex + FEATURES_HIST_SIZE - 1) % FEATURES_HIST_SIZE];
    double *features = featureHist[featureIndex];
    features[0] = 0;
    for (int i=1; i<FEATLEN; i++)
    {
        //features[i] = mel_out[i].r * mel_out[i].r * i;
        //features[i] = log (mel_out[i].r * mel_out[i].r);
        //features[i] = (1 - decay2) * lastFeatures[i] + mel_out[i].r;
        features[i] = mel_out[i].r;
        //print_debug ("%d: m: %f => f: %f", i, mel_out[i].r, features[i]);
    }
    featureIndex = (featureIndex + 1) % FEATURES_HIST_SIZE;
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

#define F 1.01
int on_encoder (void *twisterDev, int encoder, int dir)
{
    double f = (dir > 0) ? F : 1.0 / F;
    switch (encoder)
    {
     case 0: volume *= f; print_debug ("volume %f", volume); break;
     case 1: decay *= f; print_debug ("decay %f", decay); break;
     case 2: if (decay2 < 1) decay2 *= f; else decay2 = 2 - (2 - decay2) / f; print_debug ("decay2 %f", decay2); break;
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

uint64_t melfall_xy (CipHistogram *hist, CipGraph *graph, uint32_t logMode, char plotType)
{
    int *bins  = hist->bins;
    uint32_t w = hist->w;
    uint32_t h = hist->h;

    double featMinVal = 0;
    double featMaxVal = 0;
    CipArea dataRange = featWin->dataRange;
    featMinVal = dataRange.y1;
    featMaxVal = dataRange.y0;

    double featX0 = dataRange.x0;
    double featX1 = dataRange.x1;

    for (uint32_t yi=0; yi<h; yi++)
    {
        int idx = (featureIndex + FEATURES_HIST_SIZE - yi - 1) % FEATURES_HIST_SIZE;
        double *feat = featureHist[idx];
        for (uint32_t xi=0; xi<w; xi++)
        {
            int x = (xi * (1.0 / w)) * (featX1 - featX0) + featX0;
            if (x >= 0 && x < FEATLEN)
                bins[yi*w+xi] = (int) ((feat[x] - featMinVal) / (featMaxVal - featMinVal) * 1000);
            else
                bins[yi*w+xi] = 0;
        }
    }
    static int cnt = 1;
    return cnt++;
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

#if 0
    kiss_fft_cfg  testCfg  = kiss_fft_alloc (DCTLEN, 0, NULL, NULL);
    kiss_fft_cpx *testIn   = calloc (DCTLEN, sizeof (kiss_fft_cpx));
    kiss_fft_cpx *testOut  = calloc (DCTLEN, sizeof (kiss_fft_cpx));
    kiss_fft_cpx *testRes  = calloc (DCTLEN, sizeof (kiss_fft_cpx));
    for (int i=0; i<DCTLEN; i++)
    {
        testIn[i].r = (i % 2) * 2;
        testIn[i].i = 0;
    }

    printf ("\n");
    for (int i=0; i<DCTLEN; i++)
        printf ("%7.4f  ", testIn[i].r);

    kiss_fct (testCfg, testIn, testOut);
    printf ("\n");
    for (int i=0; i<DCTLEN; i++)
        printf ("%7.4f  ", testOut[i].r);

    kiss_ifct (testCfg, testOut, testRes);
    printf ("\n");
    for (int i=0; i<DCTLEN; i++)
        printf ("%7.4f  ", testRes[i].r);

    printf ("\n");

    cip_quit (cs);
    return 0;
#endif



    for (int gi=0; gi<GRAPH_SIZE; gi++)
    {
        switch (gi)
        {
         case GRAPH_TD:
             {
                 graphs[gi] = cip_graph_new (DCTLEN);
                 cip_graph_attach (cs, graphs[gi], gi, NULL, 'l', "red", 4);

                 for (int j=0; j<DCTLEN; j++)
                     cip_graph_add_point (graphs[gi], j, 0.0);

                 SubWindow *sw = cip_get_sub_window (cs, gi);
                 cip_continuous_scroll_enable (sw);
                 cip_set_x_range (sw, 0, DCTLEN - 1, 1);
                 cip_set_y_range (sw, -0.5, 0.5, 1);
                 break;
             }
         case GRAPH_FD:
             {
                 graphs[gi] = cip_graph_new (DCTLEN);
                 cip_graph_attach (cs, graphs[gi], gi, NULL, 'l', "yellow", 4);

                 for (int j=0; j<DCTLEN; j++)
                     cip_graph_add_point (graphs[gi], j, 0.0);

                 SubWindow *sw = cip_get_sub_window (cs, gi);
                 cip_set_x_range (sw, 0, DCTLEN - 1, 1);
                 cip_set_y_range (sw, -70, 70, 1);
                 break;
             }
         case GRAPH_FEAT:
             {
                 graphs[gi] = cip_graph_new (FEATLEN);
                 cip_graph_attach (cs, graphs[gi], gi, NULL, 'l', "gold", 4);

                 for (int j=0; j<FEATLEN; j++)
                     cip_graph_add_point (graphs[gi], j, 0.0);

                 SubWindow *sw = cip_get_sub_window (cs, gi);
                 featWin = sw;
                 cip_set_x_range (sw, 0, FEATLEN - 1, 1);
                 cip_set_y_range (sw, -0.4, 0.4, 1);
                 break;
             }
         case GRAPH_FEATFALL:
             {
                 CipGraph *nullGraph = cip_graph_new (0);
                 cip_graph_attach (cs, nullGraph, gi, melfall_xy, '0', "gold red black #4444ff cyan", 1000);

                 SubWindow *sw = cip_get_sub_window (cs, gi);
                 cip_set_x_range (sw, 0, FEATLEN - 1, 1);
                 cip_set_y_range (sw, -0.4, 0.4, 1);
                 break;
             }
         case GRAPH_NOISE_TD:
             {
                 graphs[gi] = cip_graph_new (DCTLEN);
                 cip_graph_attach (cs, graphs[gi], gi, NULL, 'l', "red", 4);

                 for (int j=0; j<DCTLEN; j++)
                     cip_graph_add_point (graphs[gi], j, 0.0);

                 SubWindow *sw = cip_get_sub_window (cs, gi);
                 cip_continuous_scroll_enable (sw);
                 cip_set_x_range (sw, 0, DCTLEN - 1, 1);
                 cip_set_y_range (sw, -0.5, 0.5, 1);
                 break;
             }
         case GRAPH_NOISE_FD:
             {
                 graphs[gi] = cip_graph_new (DCTLEN);
                 cip_graph_attach (cs, graphs[gi], gi, NULL, 'l', "yellow", 4);

                 for (int j=0; j<DCTLEN; j++)
                     cip_graph_add_point (graphs[gi], j, 0.0);

                 SubWindow *sw = cip_get_sub_window (cs, gi);
                 cip_set_x_range (sw, 0, DCTLEN - 1, 1);
                 cip_set_y_range (sw, -70, 70, 1);
                 break;
             }
         case GRAPH_NOISE_FLT_FD:
             {
                 graphs[gi] = cip_graph_new (DCTLEN);
                 cip_graph_attach (cs, graphs[gi], gi, NULL, 'l', "gold", 4);

                 for (int j=0; j<DCTLEN; j++)
                     cip_graph_add_point (graphs[gi], j, 0.0);

                 SubWindow *sw = cip_get_sub_window (cs, gi);
                 cip_set_x_range (sw, 0, DCTLEN - 1, 1);
                 cip_set_y_range (sw, -230, 230, 1);
                 break;
             }
         case GRAPH_NOISE_FLT_TD:
             {
                 graphs[gi] = cip_graph_new (DCTLEN);
                 cip_graph_attach (cs, graphs[gi], gi, NULL, 'l', "red", 4);

                 for (int j=0; j<DCTLEN; j++)
                     cip_graph_add_point (graphs[gi], j, 0.0);

                 SubWindow *sw = cip_get_sub_window (cs, gi);
                 cip_continuous_scroll_enable (sw);
                 cip_set_x_range (sw, 0, DCTLEN - 1, 1);
                 cip_set_y_range (sw, -2.5, 2.5, 1);
                 break;
             }
        }
    }

    int audioCounter = 0;

    cx_in1   = calloc (DCTLEN, sizeof (kiss_fft_cpx));
    cx_in2   = calloc (DCTLEN, sizeof (kiss_fft_cpx));
    cx_out1  = calloc (DCTLEN, sizeof (kiss_fft_cpx));
    cx_out2  = calloc (DCTLEN, sizeof (kiss_fft_cpx));
    flt_in   = calloc (DCTLEN, sizeof (kiss_fft_cpx));
    flt_out  = calloc (DCTLEN, sizeof (kiss_fft_cpx));
    assert (cx_in1); assert (cx_out1);
    assert (cx_in2); assert (cx_out2);
    assert (flt_in); assert (flt_out);

    kiss_fft_cfg kissCfg = kiss_fft_alloc (DCTLEN, 0, NULL, NULL);

    melCfg   = kiss_fft_alloc (FEATLEN, 0, NULL, NULL);
    mel_in   = calloc (FEATLEN, sizeof (kiss_fft_cpx));
    mel_out  = calloc (FEATLEN, sizeof (kiss_fft_cpx));
    assert (mel_in); assert (mel_out);

    while (cip_is_running (cs))
    {
        //cip_set_x_range (cip_get_sub_window (cs, GRAPH_FEAT),     0, FEATLEN - 1, 1);
        //cip_set_x_range (cip_get_sub_window (cs, GRAPH_FEATFALL), 0, FEATLEN - 1, 1);

        //CipArea *dr = & featWin->dataRange;
        //set_range (cip_get_sub_window (cs, GRAPH_FEAT), dr->x0, dr->y1, dr->x1, dr->y0, 0);

        midi_connect (twisterDev);
        twister_poll (twisterDev);
        for (int i=0; i<UPDATE_INTERVAL; i++)
        {
            float *samples = audio_in_samples_get ();
            cip_graph_add_point (graphs[GRAPH_TD],       audioCounter, samples[0]);
            cip_graph_add_point (graphs[GRAPH_NOISE_TD], audioCounter, randf () *0.1 - 0.05);
            audioCounter++;
        }


        //wait_for_access (& graphs[GRAPH_TD]->readAccess);
        double (*xys)[2];
        uint32_t len;
        stream_buffer_get (graphs[GRAPH_TD]->sb, & xys, & len);
        if (len != DCTLEN)
            print_error ("wtf");

        for (int i=0; i<DCTLEN; i++)
        {
            float x = 1 - (float) i * (1.0 / DCTLEN);
            float w = expf (-x * decay);
            cx_in1[i].r = w * xys[i][1];
            cx_in1[i].i = 0;
        }
        //release_access (& graphs[GRAPH_TD]->readAccess);


        //wait_for_access (& graphs[GRAPH_NOISE_TD]->readAccess);
        stream_buffer_get (graphs[GRAPH_NOISE_TD]->sb, & xys, & len);
        if (len != DCTLEN)
            print_error ("wtf");

        for (int i=0; i<DCTLEN; i++)
        {
            cx_in2[i].r = xys[i][1] * exp ((DCTLEN - i) * decay2);
            cx_in2[i].i = 0;
        }
        //release_access (& graphs[GRAPH_NOISE_TD]->readAccess);


        kiss_fct (kissCfg, cx_in1, cx_out1);
        kiss_fct (kissCfg, cx_in2, cx_out2);

        cip_graph_add_point (graphs[GRAPH_FD], 0, 0.0);
        cip_graph_add_point (graphs[GRAPH_NOISE_FD], 0, 0.0);
        for (int i=1; i<DCTLEN; i++)
        {
            cip_graph_add_point (graphs[GRAPH_FD], i, cx_out1[i].r);
            cip_graph_add_point (graphs[GRAPH_NOISE_FD], i, cx_out2[i].r);
        }

        compute_features ();
        cip_graph_add_point (graphs[GRAPH_FEAT], 0, 0.0);
        double *features = featureHist[(featureIndex + FEATURES_HIST_SIZE - 1) % FEATURES_HIST_SIZE];
        for (int i=1; i<FEATLEN; i++)
            cip_graph_add_point (graphs[GRAPH_FEAT], i, features[i]);

        apply_features ();

        cip_graph_add_point (graphs[GRAPH_NOISE_FLT_FD], 0, 0.0);
        for (int i=1; i<DCTLEN; i++)
            cip_graph_add_point (graphs[GRAPH_NOISE_FLT_FD], i, flt_in[i].r);

        kiss_ifct (kissCfg, flt_in, flt_out);

        cip_graph_add_point (graphs[GRAPH_NOISE_FLT_TD], 0, 0.0);
        for (int i=1; i<DCTLEN; i++)
            cip_graph_add_point (graphs[GRAPH_NOISE_FLT_TD], i, flt_out[i].r);

        static float outPrev[UPDATE_INTERVAL] = {0};
        static float outCurrent[UPDATE_INTERVAL] = {0};
        static float outNext[UPDATE_INTERVAL] = {0};

        for (int i=0; i<UPDATE_INTERVAL; i++)
        {
            outCurrent[i] = flt_out[DCTLEN - 2 * UPDATE_INTERVAL + i].r;
            outNext[i]    = flt_out[DCTLEN -     UPDATE_INTERVAL + i].r;

            //float wCurrent = (float) i / UPDATE_INTERVAL;
            //float wPrev = 1.0 - wCurrent;
            //float sample = wPrev * outPrev[i] + wCurrent * outCurrent[i];
            outPrev[i] = outNext[i];
            //audio_out_sample_push (sample * volume);
            float s[2] = {0};
            audio_out_samples_push (s);
        }

        cip_redraw_async (cs);
    }

    free (kissCfg);
    free (cx_in1);
    free (cx_in2);
    free (cx_out1);
    free (cx_out2);
    audio_close ();
    return 0;
}
