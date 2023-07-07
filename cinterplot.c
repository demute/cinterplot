#include "common.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>

#include "cinterplot.h"
#include "font.c"
#include "oklab.h"

extern const unsigned int font[256][8];
static int interrupted = 0;

static uint32_t rgb2color (RGB *rgb)
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

typedef struct RGBNames
{
    const char *name;
    const int r;
    const int g;
    const int b;
} RGBNames;

static const RGBNames rgbNameList[] =
{
    {"red",        255,   0,   0},
    {"green",        0, 255,   0},
    {"blue",         0,   0, 255},
    {"black",        0,   0,   0},
    {"white",      255, 255, 255},
    {"cyan",         0, 255, 255},
    {"magenta",    255,   0, 255},
    {"yellow",     255, 255,   0},
    {"orange",     255, 165,   0},
    {"purple",     128,   0, 128},
    {"pink",       255, 192, 203},
    {"brown",      165,  42,  42},
    {"gray",       128, 128, 128},
    {"darkgray",   169, 169, 169},
    {"lightgray",  211, 211, 211},
    {"lime",       191, 255,   0},
    {"indigo",      75,   0, 130},
    {"violet",     238, 130, 238},
    {"silver",     192, 192, 192},
    {"gold",       255, 215,   0},
    {"beige",      245, 245, 220},
    {"navy",         0,   0, 128},
    {"maroon",     128,   0,   0},
    {"olive",      128, 128,   0},
    {"teal",         0, 128, 128},
    {"lime",         0, 255,   0},
    {"aquamarine", 127, 255, 212},
    {"turquoise",   64, 224, 208},
    {"royalblue",   70, 130, 180},
    {"slategray",  112, 128, 144},
    {"plum",       221, 160, 221},
    {"orchid",     218, 112, 214},
    {"salmon",     250, 128, 114},
    {"coral",      255, 127,  80},
    {"wheat",      245, 222, 179},
    {"peru",       205, 133,  63},
    {"sienna",     160,  82,  45},
    {"chocolate",  210, 105,  30}
};


static int get_color_by_name (char *str, int *r, int *g, int *b)
{
    int n = sizeof (rgbNameList) / sizeof (rgbNameList[0]);
    for (int i=0; i<n; i++)
    {
        if (equal (str, rgbNameList[i].name))
        {
            *r = rgbNameList[i].r;
            *g = rgbNameList[i].g;
            *b = rgbNameList[i].b;
            return 0;
        }
    }
    return -1;
}

#define MAX_NUM_VERTICES 16
static void compute_color_scheme (char *spec, ColorScheme *scheme)
{
    int argc;
    char **argv;
    char *copy = parse_csv (spec, & argc, & argv, ' ', 0);
    if (!copy)
        exit_error ("bug");

    int nVertices = argc;
    if (nVertices > MAX_NUM_VERTICES)
        exit_error ("nVertices(%d) > MAX_NUM_VERTICES(%d) at '%s'", nVertices, MAX_NUM_VERTICES, spec);

    RGB vertices[MAX_NUM_VERTICES];
    for (int i=0; i<argc; i++)
    {
        char *str = argv[i];
        if (str[0] == '#')
        {
            int r,g,b;
            if (sscanf (& str[1], "%02x%02x%02x", & r, & g, & b) != 3)
                exit_error ("parse error at '%s' in str spec '%s'", str, spec);
            float s = 1.0 / 255.0;
            vertices[i].r = r * s;
            vertices[i].g = g * s;
            vertices[i].b = b * s;

        }
        else if ('0' <= str[0] && str[0] <= '9')
        {
            int colorIndex, numColors;
            if (sscanf (str, "%d/%d", & colorIndex, & numColors) != 2)
                exit_error ("parse error at '%s' in str spec '%s'", str, spec);
            Lab oklab;
            oklab.L = 0.7;
            float C = 0.5;
            float h = (2 * M_PI * colorIndex) / numColors;
            oklab.a = C * cos(h);
            oklab.b = C * sin(h);
            oklab_to_srgb (& oklab, & vertices[i]);

        }
        else
        {
            int r,g,b;
            if (get_color_by_name (str, & r, & g, & b) < 0)
                exit_error ("parse error at '%s' in color spec '%s'", str, spec);
            float s = 1.0 / 255.0;
            vertices[i].r = r * s;
            vertices[i].g = g * s;
            vertices[i].b = b * s;
        }
    }
    free (copy);
    free (argv);

    if (nVertices == 1)
    {
        uint32_t color = rgb2color (& vertices[0]);
        int n = scheme->nLevels;
        for (int i=0; i<n; i++)
            scheme->colors[i] = color;
    }
    else
    {
        int offset = 0;
        for (int v=0; v<nVertices-1; v++)
        {
            Lab c0, c1;
            srgb_to_oklab (& vertices[v],   & c0);
            srgb_to_oklab (& vertices[v+1], & c1);

            int len = scheme->nLevels / (nVertices - 1);
            if (v == 0)
            {
                int rest = scheme->nLevels - len * (nVertices - 1);
                len += rest;
            }

            for (int i=0; i<len; i++)
            {
                float w1;
                if (v+1 == nVertices-1)
                    w1 = (float) i / (len - 1);
                else
                    w1 = (float) i / len;
                float w0 = 1 - w1;
                Lab c =
                {
                    w0 * c0.L + w1 * c1.L,
                    w0 * c0.a + w1 * c1.a,
                    w0 * c0.b + w1 * c1.b,
                };
                RGB rgb;
                oklab_to_srgb (& c, & rgb);
                if (offset + i >= scheme->nLevels)
                    exit_error ("bug");
                scheme->colors[offset + i] = rgb2color (& rgb);
            }
            offset += len;
        }
        if (offset != scheme->nLevels)
            exit_error ("bug");
    }
}

int autoscale (CinterState *cs)                         { cs->autoscale = 1;                       return 1; }
int background (CinterState *cs, uint32_t bgColor)      { cs->bgColor = bgColor;                   return 1; }
int toggle_mouse (CinterState *cs)                      { cs->mouseEnabled ^= 1;                   return 1; }
int toggle_fullscreen (CinterState *cs)                 { cs->toggleFullscreen = 1;                return 1; }
int quit (CinterState *cs)                              { cs->running = 0;                         return 0; }
int toggle_tracking (CinterState *cs)                   { cs->trackingEnabled ^= 1;                return 1; }

int move_left (CinterState *cs)
{
    //CinterGraph *g = cs->graphs[cs->activeGraphIndex];
    return 1;
}

int move_right (CinterState *cs)
{
    foobar;
    return 1;
}

int move_up (CinterState *cs)
{
    foobar;
    return 1;
}

int move_down (CinterState *cs)
{
    foobar;
    return 1;
}

int expand_x (CinterState *cs)
{
    foobar;
    return 1;
}

int compress_x (CinterState *cs)
{
    foobar;
    return 1;
}

int expand_y (CinterState *cs)
{
    foobar;
    return 1;
}

int compress_y (CinterState *cs)
{
    foobar;
    return 1;
}


static void lineRGBA (uint32_t *pixels, int w, int h, int x0, int y0, int x1, int y1, uint32_t color)
{
    if (!pixels)
        return;

    int xabs = abs (x0 - x1);
    int yabs = abs (y0 - y1);

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
                pixels[y*w+x] = color;
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
            if (x>=0 && y>=0 && x<w && y<h)
                pixels[y*w+x] = color;
        }
    }
    else
    {
        int x = x0;
        int y = y0;
        if (x>=0 && y>=0 && x<w && y<h)
            pixels[y*w+x] = color;
    }
}

static void draw_rect (uint32_t* pixels, int w, int h, int x0, int y0, int x1, int y1, uint32_t color)
{
    if (x1 < x0)
    {
        x1 ^= x0;
        x0 ^= x1;
        x1 ^= x0;
    }
    if (y1 < y0)
    {
        y1 ^= y0;
        y0 ^= y1;
        y1 ^= y0;
    }
    if (!pixels)
        return;

    for (int yi=y0; yi<=y1 && yi<h; yi++)
    {
        pixels[yi*w + x0] = color;
        pixels[yi*w + x1] = color;
    }
    for (int xi=x0; xi<=x1 && xi<w; xi++)
    {
        pixels[y0*w + xi] = color;
        pixels[y1*w + xi] = color;
    }
}

static int on_mouse_pressed (CinterState *cs, int button)
{
    return 0;
}

static int on_mouse_released (CinterState *cs)
{
    return 0;
}

static int on_mouse_motion (CinterState *cs, int xi, int yi)
{
    cs->mouseX = xi;
    cs->mouseY = yi;
    return 1;
}

#define COLOR_DARK_GRAY  MAKE_COLOR ( 73, 73, 73)
#define COLOR_GRAY       MAKE_COLOR (111,111,111)
#define COLOR_LIGHT_GRAY MAKE_COLOR (144,144,144)
#define COLOR_WHITE_GRAY MAKE_COLOR (182,182,182)

static int on_keyboard (CinterState *cs, int key, int mod, int pressed, int repeat)
{
    // FIXME: If both the left and right key of the same modifier gets pressed at
    // the same time and then one gets released, the state of pressedModifiers
    // gets zeroed out
    if (key == SDLK_LSHIFT || key == SDLK_RSHIFT)
    {
        if (pressed)
            cs->pressedModifiers &= ~KMOD_SHIFT;
        else
            cs->pressedModifiers |= KMOD_SHIFT;
        return 0;
    }
    else if (key == SDLK_LGUI || key == SDLK_RGUI)
    {
        if (pressed)
            cs->pressedModifiers &= ~KMOD_GUI;
        else
            cs->pressedModifiers |= KMOD_GUI;
        return 0;
    }
    else if (key == SDLK_LALT || key == SDLK_RALT)
    {
        if (pressed)
            cs->pressedModifiers &= ~KMOD_ALT;
        else
            cs->pressedModifiers |= KMOD_ALT;
        return 0;
    }
    else if (key == SDLK_LCTRL || key == SDLK_RCTRL)
    {
        if (pressed)
            cs->pressedModifiers &= ~KMOD_CTRL;
        else
            cs->pressedModifiers |= KMOD_CTRL;
        return 0;
    }

    int unhandled = 0;
    if (pressed && !repeat)
    {
        if (mod == 0)
        {
            switch (key)
            {
             case 'a': autoscale (cs); break;
             case 'f': toggle_fullscreen (cs); break;
             case 'm': toggle_mouse (cs); break;
             case 'q': quit (cs); break;
             case 't': toggle_tracking (cs); break;
                       //case 'x': exit_zoom (cs); break;

             case '7': background (cs, COLOR_DARK_GRAY); break;
             case '8': background (cs, COLOR_GRAY); break;
             case '9': background (cs, COLOR_LIGHT_GRAY); break;
             case '0': background (cs, COLOR_WHITE_GRAY); break;
             default: unhandled = 1;
            }
        }
        else if (mod == KMOD_LSHIFT || mod == KMOD_RSHIFT || mod == KMOD_SHIFT)
        {
            unhandled = 1;
        }
        else
        {
            unhandled = 1;
        }
    }

    else if (pressed)
    {
        switch (key)
        {
         case SDLK_LEFT: move_left (cs); break;
         case SDLK_RIGHT: move_right (cs); break;
         case SDLK_UP: move_up (cs); break;
         case SDLK_DOWN: move_down (cs); break;
         case '+': expand_x (cs); break;
         case '-': compress_x (cs); break;
         case '.': expand_y (cs); break;
         case ',': compress_y (cs); break;
         default: unhandled = 1;
        }
    }

    if (unhandled)
        print_debug ("key: %c (%d), pressed: %d, repeat: %d", key, key, pressed, repeat);

    return 0;
}

#ifndef randf
#define randf() ((double) rand () / ((double) RAND_MAX+1))
#endif
static void plot_data (CinterState *cs, uint32_t *pixels, int w, int h)
{
    int nCols = cs->nCols;
    int nRows = cs->nRows;
    int dy = h / nRows;
    int dx = w / nCols;

    int mouseCol = cs->mouseX / dx;
    int mouseRow = cs->mouseY / dy;
    SubWindow *activeSw = NULL;
    if (mouseCol >= 0 && mouseCol < nCols && mouseRow >= 0 && mouseRow < nRows)
        activeSw = & cs->subWindows[mouseRow * nCols + mouseCol];


    uint32_t activeColor   = MAKE_COLOR (255,255,255);
    uint32_t inactiveColor = MAKE_COLOR (100,100,100);

    for (int sy=0; sy < nRows; sy++)
    {
        int yOffset = dy * sy;
        for (int sx=0; sx < nCols; sx++)
        {
            int xOffset = dx * sx;
            SubWindow *sw = & cs->subWindows[sy * nCols + sx];

            if (cs->bordered)
            {
                int x0 = xOffset + cs->margin;
                int y0 = yOffset + cs->margin;
                int x1 = xOffset + dx - cs->margin;
                int y1 = yOffset + dy - cs->margin;
                draw_rect (pixels, w, h, x0, y0, x1, y1, (sw == activeSw) ? activeColor : inactiveColor);
            }

            // Här vill vi ta alla histogram som finns och plotta dem en efter en. För alla subkoordinater tar vi
            // ut histogramkoordinater och pixelkoordinater, sedan applicerar vi färgen colorScheme->colors[binCount]
            // på denna pixel. Inte svårare än så!
        }
    }
}

int make_sub_windows (CinterState *cs, int nRows, int nCols, int bordered, int margin)
{
    if (cs->subWindows || cs->nRows || cs->nCols)
    {
        print_error ("cs->subWindows must not have been set previously");
        return -1;
    }

    cs->nRows    = nRows;
    cs->nCols    = nCols;
    cs->bordered = bordered;
    cs->margin   = margin;

    int subw = cs->windowWidth  / nCols - 2*cs->bordered - 2*cs->margin;
    int subh = cs->windowHeight / nCols - 2*cs->bordered - 2*cs->margin;

    int n = nRows * nCols;
    cs->subWindows = safe_calloc (n, sizeof (cs->subWindows[0]));
    for (int i=0; i<n; i++)
    {
        SubWindow *sw = & cs->subWindows[i];
        sw->attachedGraphs = NULL;
        sw->maxNumAttachedGraphs = 0;
        sw->numAttachedGraphs = 0;
        sw->xmin = 0;
        sw->xmax = 0;
        sw->ymin = 0;
        sw->ymax = 0;
        sw->w    = subw;
        sw->h    = subh;
    }

    return 0;
}

typedef struct
{
    int argc;
    char **argv;
    CinterState *cs;
} UserData;

int user_main (int argc, char **argv, CinterState *cs);
static int userMainRetVal = 1;
static void *userMainCaller (void *_data)
{
    UserData *data = _data;
    userMainRetVal = user_main (data->argc, data->argv, data->cs);
    return NULL;
}

static void update_image (CinterState *cs, SDL_Texture *texture, int init)
{
    int w, h;
    if (SDL_QueryTexture (texture, NULL, NULL, & w, & h))
        exit_error ("failed to query texture");

    uint32_t* pixels;
    int wb;
    int status = SDL_LockTexture (texture, NULL, (void**) & pixels, & wb);
    if (status)
        exit_error ("texture: %p, status: %d: %s\n", (void*) texture, status, SDL_GetError());

    if (init)
        memset (pixels, 0x11, sizeof (uint32_t) * (uint32_t) w * (uint32_t) h);
    else
        cs->plot_data (cs, pixels, w, h);
    SDL_UnlockTexture (texture);
}

static void signal_handler (int sig)
{
    interrupted = 1;
}

static void reinitialise_sdl_context (CinterState *cs)
{
    if (cs->texture)
        SDL_DestroyTexture (cs->texture);
    if (cs->renderer)
        SDL_DestroyRenderer (cs->renderer);
    if (cs->window)
        SDL_DestroyWindow (cs->window);

    cs->window = SDL_CreateWindow (CINTERPLOT_TITLE, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                                   cs->windowWidth, cs->windowHeight, SDL_WINDOW_SHOWN);
    if (!cs->window)
        exit_error ("Window could not be created: SDL Error: %s\n", SDL_GetError ());

    cs->renderer = SDL_CreateRenderer (cs->window, -1, SDL_RENDERER_ACCELERATED);
    if (!cs->renderer)
        exit_error ("Renderer could not be created! SDL Error: %s\n", SDL_GetError ());

    cs->texture = SDL_CreateTexture (cs->renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
                                     cs->windowWidth, cs->windowHeight);
    if (!cs->texture)
        exit_error ("Texture could not be created: SDL Error: %s\n", SDL_GetError ());

    SDL_SetWindowFullscreen (cs->window, cs->fullscreen ? SDL_WINDOW_FULLSCREEN : 0);
}

static CinterState *cinterplot_init (void)
{
    CinterState *cs = safe_calloc (1, sizeof (*cs));
    cs->on_mouse_pressed  = on_mouse_pressed;
    cs->on_mouse_released = on_mouse_released;
    cs->on_mouse_motion   = on_mouse_motion;
    cs->on_keyboard       = on_keyboard;
    cs->plot_data         = plot_data;

    cs->autoscale        = 0;
    cs->mouseEnabled     = 1;
    cs->trackingEnabled  = 0;
    cs->toggleFullscreen = 0;
    cs->fullscreen       = 0;
    cs->redraw           = 0;
    cs->redrawing        = 0;
    cs->running          = 1;
    cs->bgColor          = COLOR_DARK_GRAY;
    cs->nRows            = 0;
    cs->nCols            = 0;
    cs->bordered         = 0;
    cs->margin           = 10;
    cs->frameCounter     = 0;
    cs->pressedModifiers = 0;
    cs->subWindows       = NULL;
    cs->windowWidth      = CINTERPLOT_INIT_WIDTH;
    cs->windowHeight     = CINTERPLOT_INIT_HEIGHT;

    signal (SIGINT, signal_handler);

    if (SDL_Init (SDL_INIT_VIDEO) < 0)
        exit_error ("SDL could not initialize! SDL Error: %s\n", SDL_GetError ());

    reinitialise_sdl_context (cs);

    update_image (cs, cs->texture, 1);

    SDL_RenderCopy (cs->renderer, cs->texture, NULL, NULL);
    SDL_RenderPresent (cs->renderer);

    return cs;
}

static int cinterplot_run_until_quit (CinterState *cs)
{
    double fps = 30;
    double periodTime = 1.0 / fps;
    double lastFrameTsp = 0;

    print_debug ("%d %d", cs->running, interrupted);
    while (cs->running && !interrupted)
    {
        cs->frameCounter++;
        SDL_Event sdlEvent;
        while (SDL_PollEvent (& sdlEvent))
        {
            switch(sdlEvent.type)
            {
             case SDL_QUIT:
                 return 0;
             case SDL_MOUSEBUTTONDOWN:
                 cs->redraw |= cs->on_mouse_pressed (cs, sdlEvent.button.button);
                 break;
             case SDL_MOUSEBUTTONUP:
                 cs->redraw |= cs->on_mouse_released (cs);
                 break;
             case SDL_MOUSEMOTION:
                 cs->redraw |= cs->on_mouse_motion (cs, sdlEvent.motion.x, sdlEvent.motion.y);
                 break;
             case SDL_KEYDOWN:
             case SDL_KEYUP:
                 {
                     int repeat = sdlEvent.key.repeat;
                     int pressed = sdlEvent.key.state == SDL_PRESSED;
                     int key = sdlEvent.key.keysym.sym;
                     int mod = sdlEvent.key.keysym.mod;

                     cs->redraw |= cs->on_keyboard (cs, key, mod, pressed, repeat);
                     break;
                 }
             default:
                 break;
            }
        }

        if (cs->toggleFullscreen)
        {
            cs->toggleFullscreen = 0;
            cs->fullscreen = !cs->fullscreen;

            if (cs->fullscreen)
            {
                SDL_DisplayMode displayMode;
                SDL_GetCurrentDisplayMode (0, & displayMode);
                cs->windowWidth  = displayMode.w;
                cs->windowHeight = displayMode.h;
            }
            else
            {
                cs->windowWidth  = CINTERPLOT_INIT_WIDTH;
                cs->windowHeight = CINTERPLOT_INIT_HEIGHT;
            }

            reinitialise_sdl_context (cs);
        }

        double tsp = get_time ();
        if (cs->redraw && tsp - lastFrameTsp > periodTime)
        {
            cs->redrawing = 1;
            lastFrameTsp = tsp;
            update_image (cs, cs->texture, 0);
            SDL_RenderCopy (cs->renderer, cs->texture, NULL, NULL);
            SDL_RenderPresent (cs->renderer);
            cs->redrawing = 0;
            cs->redraw = 0;
        }
    }

    return 0;
}

static void cinterplot_cleanup (CinterState *cs)
{
    SDL_DestroyRenderer (cs->renderer);
    SDL_DestroyWindow (cs->window);
    SDL_Quit();
    cs->renderer = NULL;
    cs->window = NULL;
}

int main (int argc, char **argv)
{
    // SDL, at least on my version of macOS, prevents SDL from running on any other thread
    // than the main thread
    sranddev ();

    CinterState *cs = cinterplot_init ();
    if (!cs)
        return 1;

    UserData data = {argc, argv, cs};
    pthread_t userThread;
    if (pthread_create (& userThread, NULL, userMainCaller, & data))
        exit_error ("could not create thread\n");

    int ret = cinterplot_run_until_quit (cs);
    cs->running = 0;

    // Wait for the thread to finish
    if (pthread_join (userThread, NULL)) {
        exit_error("could not join thread\n");
    }

    cinterplot_cleanup (cs);

    return userMainRetVal ? userMainRetVal : ret;
}

