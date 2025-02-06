#include <SDL2/SDL.h>
#include "common.h"
#include <stream_buffer.h>
#include "audio.h"


static SDL_AudioDeviceID audioInDev;
static SDL_AudioDeviceID audioOutDev;
SDL_AudioSpec* audioInSpec;
SDL_AudioSpec* audioOutSpec;

#define AUDIO_BUFFER_SIZE (2048)
#define AUDIO_BUFFER_MASK (AUDIO_BUFFER_SIZE - 1)
int  audioInBufHead = 0;
int  audioInBufTail = 0;
float audioInBuf[AUDIO_BUFFER_SIZE][2] = {0};
int  audioOutBufHead = 0;
int  audioOutBufTail = 0;
float audioOutBuf[AUDIO_BUFFER_SIZE][2] = {0};

static int quit = 0;
static int delay = 0;

void audio_out_samples_push (float *samples)
{
    while (((audioOutBufHead + 1) & AUDIO_BUFFER_MASK) == audioOutBufTail)
    {
        //printf ("waiting\n");
        // no space left in buffer, wait until other thread has dequeued buffer
        if (quit)
            return;
        else
            usleep (1000);
    }
    int nextHead = (audioOutBufHead + 1) & AUDIO_BUFFER_MASK;
    audioOutBuf[audioOutBufHead][0] = samples[0];
    audioOutBuf[audioOutBufHead][1] = samples[1];
    audioOutBufHead = nextHead;
}

void audio_in_samples_push (float *samples)
{
    if (delay > 2048)
        return;
    while (((audioInBufHead + 1) & AUDIO_BUFFER_MASK) == audioInBufTail)
    {
        //printf ("waiting\n");
        // no space left in buffer, wait until other thread has dequeued buffer
        if (quit)
            return;
        else
            usleep (1000);
    }
    int nextHead = (audioInBufHead + 1) & AUDIO_BUFFER_MASK;
    audioInBuf[audioInBufHead][0] = samples[0];
    audioInBuf[audioInBufHead][1] = samples[1];
    audioInBufHead = nextHead;
    delay++;
}

float *audio_in_samples_get (void)
{
    static float samples[2] = {0};
    while (audioInBufTail == audioInBufHead)
    {
        if (quit)
        {
            samples[0] = samples[1];
            return samples;
        }
        else
            usleep (1000);
    }

    int nextTail = (audioInBufTail + 1) & AUDIO_BUFFER_MASK;
    samples[0] = audioInBuf[audioInBufTail][0];
    samples[1] = audioInBuf[audioInBufTail][1];
    audioInBufTail = nextTail;

    return samples;
}

float *try_dequeue (void)
{
    static float samples[2] = {0};
    if (audioOutBufTail != audioOutBufHead)
    {
        int nextTail = (audioOutBufTail + 1) & AUDIO_BUFFER_MASK;
        samples[0] = audioOutBuf[audioOutBufTail][0];
        samples[1] = audioOutBuf[audioOutBufTail][1];
        audioOutBufTail = nextTail;
        delay--;
    }
    else
    {
        //usleep (1000000 / 48000);
        static double lastTsp = 0;
        double tsp = get_time ();
        if (tsp - lastTsp > 1)
        {
            lastTsp = tsp;
            printf ("%s: underflow\n", double_to_date_string (tsp));
        }
    }
    return samples;
}


void audio_in_callback (void *_buf, Uint8 *stream, int size)
{
    float (*s)[2] = (float (*)[2]) stream;
    int len = size / (int) (2*sizeof (float));

    for (int i=0; i<len; i++)
    {
        audio_in_samples_push (s[i]);
    }
}

void audio_out_callback (void *_buf, Uint8 *stream, int size)
{
    float (*s)[2] = (float (*)[2]) stream;
    int len = size / (int) (2*sizeof (float));

    for (int i=0; i<len; i++)
    {
        float *samples = try_dequeue ();
        s[i][0] = samples[0];
        s[i][1] = samples[1];
    }
}

void audio_in_init (float *dstBuffer)
{
    static SDL_AudioSpec request, result;
    int i, count = SDL_GetNumAudioDevices(1);

    for (i = 0; i < count; ++i)
        print_debug ("Audio  in option %d: %s", i, SDL_GetAudioDeviceName(i, 1));

    request.userdata = dstBuffer;
    request.freq = AUDIO_FREQUENCY;
    request.format = AUDIO_F32;
    request.channels = 2;
    request.samples = 512;
    request.callback = audio_in_callback;
    char *devs[] =
    {
        "USB AUDIO  CODEC",
        "MacBook Pro Microphone",
        "H Series Stereo Track Usb Audio",
        "Built-in Microphone",
        "External Microphone",
        "H1n Digital Stereo (IEC958)",
        "H1n Analog Stereo",
        "Built-in Audio Analog Stereo",
    };
    int n = sizeof (devs) / sizeof (devs[0]);
    for (int i=0; i<n; i++)
    {
        fprintf (stderr, "trying to open input %s...", devs[i]);
        fflush (stderr);
        audioInDev = SDL_OpenAudioDevice (devs[i], 1, & request, & result, 0);
        if (audioInDev)
        {
            fprintf (stderr, "success\n");
            fflush (stderr);
            break;
        }
        else
        {
            fprintf (stderr, "failed\n");
            fflush (stderr);
        }
    }
    if (!audioInDev)
    {
        print_error ("failed to open audio: %s\n", SDL_GetError ());

        const char *first = SDL_GetAudioDeviceName(0, 1);
        print_debug ("Trying to open %s", first);
        audioInDev = SDL_OpenAudioDevice (first, 1, & request, & result, 0);
        if (!audioInDev)
        {
            print_error ("failed to open audio: %s\n", SDL_GetError ());
            //cinterplot_quit (cs);
        }
    }
    audioInSpec = & result;

    if (result.channels != request.channels)
        print_error ("channels not supported");
    if (result.samples != request.samples)
        print_error ("samples not supported");
    if (result.freq != request.freq)
        print_error ("freq not supported");

    SDL_PauseAudioDevice (audioInDev, 0); // start audio playing.
}

void audio_out_init (float *srcBuffer)
{
    static SDL_AudioSpec request, result;

    int i, count = SDL_GetNumAudioDevices(0);

    for (i = 0; i < count; ++i)
        print_debug ("Audio out option %d: %s", i, SDL_GetAudioDeviceName(i, 0));

    request.userdata = srcBuffer;
    request.freq = AUDIO_FREQUENCY;
    request.format = AUDIO_F32;
    request.channels = 2;
    request.samples = 512;
    request.callback = audio_out_callback;
    char *devs[] =
    {
        "Audio Pro T14",
        "External Headphones",
        "MacBook Pro Speakers",
        "USB Audio DAC",
        "Externe Kopfhörer",
        "USB-AudioEVM",
        "BOOM 3",
    };
    int n = sizeof (devs) / sizeof (devs[0]);
    for (int i=0; i<n; i++)
    {
        printf ("trying to open output %s...", devs[i]);
        audioOutDev = SDL_OpenAudioDevice (devs[i], 0, & request, & result, 0);
        if (audioOutDev)
        {
            printf ("success\n");
            break;
        }
        else
        {
            printf ("failed\n");
        }
    }
    if (!audioOutDev)
    {
        print_error ("failed to open audio: %s\n", SDL_GetError ());
        const char *first = SDL_GetAudioDeviceName(i, 0);
        sleep (1);
        audioOutDev = SDL_OpenAudioDevice (first, 0, & request, & result, 0);
        if (!audioOutDev)
        {
            print_error ("failed to open audio: %s\n", SDL_GetError ());
            //cinterplot_quit (cs);
        }
    }
    audioOutSpec = & result;

    if (result.channels != request.channels)
        print_error ("channels not supported");
    if (result.samples != request.samples)
        print_error ("samples not supported");
    if (result.freq != request.freq)
        print_error ("freq not supported");

    SDL_PauseAudioDevice (audioOutDev, 0); // start audio playing.
}

void audio_init (void)
{
    if (SDL_Init (SDL_INIT_AUDIO) < 0)
        print_error ("SDL_Init error");

    //audioInBufLen = AUDIO_FREQUENCY * 60 * 5;
    //audioInBuf = safe_calloc ((size_t) audioInBufLen, sizeof (audioInBuf[0]));

    //for (int i=0; i<AUDIO_BUFFER_SIZE-1; i++)
    //    audio_out_sample_push (0.0);

    audio_in_init (NULL);
    audio_out_init (NULL);

}

void audio_close (void)
{
    print_debug ("closing audio device");
    quit = 1;

    SDL_PauseAudioDevice (audioInDev, 1);
    SDL_PauseAudioDevice (audioOutDev, 1);
    if (audioInDev)
    {
        SDL_CloseAudioDevice (audioInDev);
        audioInDev = 0;
    }

    if (audioOutDev)
    {
        SDL_CloseAudioDevice (audioOutDev);
        audioOutDev = 0;
    }

    SDL_CloseAudio ();
}

