#include "common.h"

#include <SDL2/SDL.h>
#include <stdatomic.h>

#include "cinterplot.h"
#include "font.c"
#include "oklab.h"

typedef struct CinterState
{
    uint32_t mouseEnabled : 1;
    uint32_t trackingMode : 2;
    uint32_t statuslineEnabled : 1;
    uint32_t gridEnabled : 1;
    uint32_t zoomEnabled : 1;
    uint32_t fullscreen : 1;
    uint32_t continuousScroll : 1;
    uint32_t redraw : 1;
    uint32_t redrawing : 1;
    uint32_t running : 1;
    uint32_t bordered : 1;
    uint32_t forceRefresh : 1;
    uint32_t margin : 8;
    uint32_t showHelp : 1;


    int mouseState;
    Position mouseWindowPos;

    Mouse mouse;

    float bgShade;

    uint32_t numSubWindows;
    SubWindow *subWindows;
    SubWindow *activeSw;

    SDL_Window   *window;
    SDL_Renderer *renderer;
    SDL_Texture  *texture;

    uint32_t windowWidth;
    uint32_t windowHeight;

    uint32_t graphOrder;
    int frameCounter;
    int pressedModifiers;

    int  (*on_mouse_pressed)  (struct CinterState *cs, int xi, int yi, int button, int clicks);
    int  (*on_mouse_released) (struct CinterState *cs, int xi, int yi);
    int  (*on_mouse_motion)   (struct CinterState *cs, int xi, int yi);
    int  (*on_mouse_wheel)    (struct CinterState *cs, float xf, float yf);
    int  (*on_keyboard)       (struct CinterState *cs, int key, int mod, int pressed, int repeat);
    void (*plot_data)         (struct CinterState *cs, uint32_t *pixels);

} CinterState;


#define STATUSLINE_HEIGHT 20

enum
{
    MOUSE_HOVER_MODE_NONE,
    MOUSE_HOVER_MODE_WINDOW,
    MOUSE_HOVER_MODE_BORDER_L,
    MOUSE_HOVER_MODE_BORDER_R,
    MOUSE_HOVER_MODE_BORDER_T,
    MOUSE_HOVER_MODE_BORDER_B,
    MOUSE_HOVER_MODE_CORNER_TL,
    MOUSE_HOVER_MODE_CORNER_TR,
    MOUSE_HOVER_MODE_CORNER_BL,
    MOUSE_HOVER_MODE_CORNER_BR,
};

enum
{
    MOUSE_STATE_NONE,
    MOUSE_STATE_MOVING,
    MOUSE_STATE_SELECTING,
    MOUSE_STATE_RESIZING_L,
    MOUSE_STATE_RESIZING_R,
    MOUSE_STATE_RESIZING_T,
    MOUSE_STATE_RESIZING_B,
    MOUSE_STATE_RESIZING_TL,
    MOUSE_STATE_RESIZING_TR,
    MOUSE_STATE_RESIZING_BL,
    MOUSE_STATE_RESIZING_BR,
};

extern const unsigned int font[256][8];
static int interrupted = 0;
static int paused = 0;

void wait_for_access (atomic_flag* accessFlag)
{
    while (atomic_flag_test_and_set(accessFlag))
        continue;
}

void release_access (atomic_flag* accessFlag)
{
    atomic_flag_clear (accessFlag);
}


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

static uint32_t make_gray (float a)
{
    RGB rgb = {a,a,a};
    return rgb2color (& rgb);
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

static ColorScheme *make_color_scheme (char *spec, uint32_t nLevels)
{
    ColorScheme *scheme = safe_calloc (1, sizeof (*scheme));
    scheme->colors      = safe_calloc (nLevels, sizeof (scheme->colors[0]));
    scheme->nLevels     = nLevels;

    int argc;
    char **argv;
    char *modStr = parse_csv (spec, & argc, & argv, ' ', 0);
    if (!modStr)
        exit_error ("bug");

    uint32_t nVertices = (uint32_t) argc;
    if (nVertices > MAX_NUM_VERTICES)
        exit_error ("nVertices(%u) > MAX_NUM_VERTICES(%u) at '%s'", nVertices, MAX_NUM_VERTICES, spec);

    RGB vertices[MAX_NUM_VERTICES];
    for (int i=0; i<argc; i++)
    {
        char *str = argv[i];
        if (str[0] == '#')
        {
            int r,g,b;
            if (sscanf (& str[1], "%02x%02x%02x", & r, & g, & b) != 3)
                exit_error ("parse error at '%s' in str spec '%s'", str, spec);
            float s = 1.0f / 255.0f;
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
            oklab.L = 0.7f;
            float C = 0.5f;
            float h = (float) ((2 * M_PI * colorIndex) / numColors);
            oklab.a = C * cosf(h);
            oklab.b = C * sinf(h);
            oklab_to_srgb (& oklab, & vertices[i]);

        }
        else
        {
            int r,g,b;
            if (get_color_by_name (str, & r, & g, & b) < 0)
                exit_error ("parse error at '%s' in color spec '%s'", str, spec);
            float s = 1.0f / 255.0f;
            vertices[i].r = r * s;
            vertices[i].g = g * s;
            vertices[i].b = b * s;
        }
    }
    free (modStr);
    free (argv);

    if (nVertices == 1)
    {
        uint32_t color = rgb2color (& vertices[0]);
        for (uint32_t i=0; i<nLevels; i++)
            scheme->colors[i] = color;
    }
    else
    {
        uint32_t offset = 0;
        for (int v=0; v<nVertices-1; v++)
        {
            Lab c0, c1;
            srgb_to_oklab (& vertices[v],   & c0);
            srgb_to_oklab (& vertices[v+1], & c1);

            uint32_t len = nLevels / (nVertices - 1);
            if (v == 0)
            {
                uint32_t rest = nLevels - len * (nVertices - 1);
                len += rest;
            }

            for (uint32_t i=0; i<len; i++)
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
                if (offset + i >= nLevels)
                    exit_error ("bug");
                scheme->colors[offset + i] = rgb2color (& rgb);
            }
            offset += len;
        }
        if (offset != nLevels)
            exit_error ("bug");
    }

    return scheme;
}

void cycle_graph_order (CinterState *cs)
{
    cs->graphOrder++;
    //print_debug ("graph order %u", cs->graphOrder);
}

int autoscale (SubWindow *sw)
{
    if (!sw)
        return 0;

    double xmin =  DBL_MAX;
    double xmax = -DBL_MAX;
    double ymin =  DBL_MAX;
    double ymax = -DBL_MAX;

    GraphAttacher **ag = sw->attachedGraphs;
    for (int i=0; i<sw->numAttachedGraphs; i++)
    {
        CinterGraph *graph = ag[i]->graph;
        wait_for_access (& graph->readAccess);
        wait_for_access (& graph->insertAccess);

        double (*xys)[2];
        uint32_t len;
        stream_buffer_get (graph->sb, & xys, & len);
        for (uint32_t i=0; i<len; i++)
        {
            double x = xys[i][0];
            double y = xys[i][1];
            if (xmin > x) xmin = x;
            if (xmax < x) xmax = x;
            if (ymin > y) ymin = y;
            if (ymax < y) ymax = y;
        }

        release_access (& graph->insertAccess);
        release_access (& graph->readAccess);
    }

    if (xmin == DBL_MAX || xmax == -DBL_MAX || ymin == DBL_MAX || ymax == -DBL_MAX)
        return 0;

    double dx = xmax - xmin;
    double dy = ymax - ymin;
    double mx = dx * 0.0;
    double my = dy * 0.05;

    sw->dataRange.x0 = xmin - mx;
    sw->dataRange.y0 = ymax + my;
    sw->dataRange.x1 = xmax + mx;
    sw->dataRange.y1 = ymin - my;

    return 1;
}

void set_range (SubWindow *sw, double xmin, double ymin, double xmax, double ymax, int setAsDefault)
{
    if (!sw)
        exit_error ("bug");

    sw->dataRange.x0 = xmin;
    sw->dataRange.y0 = ymax;
    sw->dataRange.x1 = xmax;
    sw->dataRange.y1 = ymin;

    if (setAsDefault)
        memcpy (& sw->defaultDataRange, & sw->dataRange, sizeof (Area));
}

void set_x_range (SubWindow *sw, double xmin, double xmax, int setAsDefault)
{
    if (!sw)
        exit_error ("bug");

    sw->dataRange.x0 = xmin;
    sw->dataRange.x1 = xmax;

    if (setAsDefault)
        memcpy (& sw->defaultDataRange, & sw->dataRange, sizeof (Area));
}

void set_y_range (SubWindow *sw, double ymin, double ymax, int setAsDefault)
{
    if (!sw)
        exit_error ("bug");

    sw->dataRange.y0 = ymax;
    sw->dataRange.y1 = ymin;

    if (setAsDefault)
        memcpy (& sw->defaultDataRange, & sw->dataRange, sizeof (Area));
}

int continuous_scroll_update (SubWindow *sw)
{
    double xmin =  DBL_MAX;
    double xmax = -DBL_MAX;

    GraphAttacher **ag = sw->attachedGraphs;
    for (int i=0; i<sw->numAttachedGraphs; i++)
    {
        CinterGraph *graph = ag[i]->graph;
        wait_for_access (& graph->readAccess);
        wait_for_access (& graph->insertAccess);

        double (*xys)[2];
        uint32_t len;
        stream_buffer_get (graph->sb, & xys, & len);
        double x0 = xys[0][0];
        double x1 = xys[len-1][0];
        if (xmin > x0) xmin = x0;
        if (xmax < x1) xmax = x1;

        release_access (& graph->insertAccess);
        release_access (& graph->readAccess);
    }

    if (xmin == DBL_MAX || xmax == -DBL_MAX)
        return 0;

    sw->dataRange.x0 = xmin;
    sw->dataRange.x1 = xmax;

    return 1;
}


int toggle_mouse (CinterState *cs)                            { cs->mouseEnabled ^= 1;      return 1; }
int toggle_statusline (CinterState *cs)                       { cs->statuslineEnabled ^= 1; return 1; }
int toggle_help (CinterState *cs)                             { cs->showHelp ^= 1;          return 1; }
int quit (CinterState *cs)                                    { cs->running = 0; paused=0;  return 0; }
int force_refresh (CinterState *cs)                           { cs->forceRefresh = 0;       return 1; }
int set_tracking_mode (CinterState *cs, uint32_t mode)        { cs->trackingMode = mode;    return 1; }
int toggle_paused (CinterState *cs)                           { paused ^= 1;                return 1; }
void cinterplot_set_bg_shade (CinterState *cs, float bgShade) { cs->bgShade = bgShade; }

void undo_zooming (SubWindow *sw)
{
    if (!sw)
        return;
    if (sw->defaultDataRange.x0 == sw->defaultDataRange.x1)
        return;
    if (sw->defaultDataRange.y0 == sw->defaultDataRange.y1)
        return;

    memcpy (& sw->dataRange, & sw->defaultDataRange, sizeof (Area));
}

static void sub_window_change (CinterState *cs, int dir)
{
    int index = (int) (cs->activeSw - cs->subWindows);
    index += dir;
    if (index < 0)
        index = 0;
    if (index > (int) cs->numSubWindows - 1)
        index = (int) cs->numSubWindows - 1;
    cs->activeSw = & cs->subWindows[index];
}

int zoom (SubWindow *sw, double xf, double yf)
{
    if (!sw)
        return 0;
    Area *dr = & sw->dataRange;
    double dx = (dr->x1 - dr->x0) * xf;
    double dy = (dr->y1 - dr->y0) * yf;
    dr->x0 += dx;
    dr->x1 -= dx;
    dr->y0 += dy;
    dr->y1 -= dy;
    return 1;
}

int move (SubWindow *sw, double xf, double yf)
{
    if (!sw)
        return 0;
    Area *dr = & sw->dataRange;
    double dx = (dr->x1 - dr->x0) * xf;
    double dy = (dr->y1 - dr->y0) * yf;
    dr->x0 -= dx;
    dr->x1 -= dx;
    dr->y0 -= dy;
    dr->y1 -= dy;
    return 1;
}

static void reinitialise_sdl_context (CinterState *cs, int reinitWindow)
{
    if (cs->texture)
        SDL_DestroyTexture (cs->texture);
    if (cs->renderer)
        SDL_DestroyRenderer (cs->renderer);

    if (reinitWindow)
    {
        if (cs->window)
            SDL_DestroyWindow (cs->window);

        cs->window = SDL_CreateWindow (CINTERPLOT_TITLE, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                                       (int) cs->windowWidth, (int) cs->windowHeight, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
        if (!cs->window)
            exit_error ("Window could not be created: SDL Error: %s\n", SDL_GetError ());
    }

    cs->renderer = SDL_CreateRenderer (cs->window, -1, SDL_RENDERER_ACCELERATED);
    if (!cs->renderer)
        exit_error ("Renderer could not be created! SDL Error: %s\n", SDL_GetError ());

    cs->texture = SDL_CreateTexture (cs->renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
                                     (int) cs->windowWidth, (int) cs->windowHeight);
    if (!cs->texture)
        exit_error ("Texture could not be created: SDL Error: %s\n", SDL_GetError ());

    SDL_SetWindowFullscreen (cs->window, cs->fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
}

int set_grid_enabled (CinterState *cs, uint32_t gridEnabled)
{
    cs->gridEnabled = gridEnabled;
    return 1;
}

int set_fullscreen (CinterState *cs, uint32_t fullscreen)
{
    cs->fullscreen = fullscreen;

    if (cs->fullscreen)
    {
        SDL_DisplayMode displayMode;
        SDL_GetCurrentDisplayMode (0, & displayMode);
        assert (displayMode.w > 0);
        assert (displayMode.h > 0);
        print_debug ("fullscreen res: %u %u", displayMode.w, displayMode.h);
        cs->windowWidth  = (uint32_t) displayMode.w;
        cs->windowHeight = (uint32_t) displayMode.h;
    }
    else
    {
        cs->windowWidth  = CINTERPLOT_INIT_WIDTH;
        cs->windowHeight = CINTERPLOT_INIT_HEIGHT;
    }

    reinitialise_sdl_context (cs, 1);
    return 1;
}

static void lineRGBA (uint32_t *pixels, uint32_t _w, uint32_t _h, uint32_t _x0, uint32_t _y0, uint32_t _x1, uint32_t _y1, uint32_t color)
{
    if (!pixels)
        return;

    int w  = (int) _w;
    int h  = (int) _h;
    int x0 = (int) _x0;
    int y0 = (int) _y0;
    int x1 = (int) _x1;
    int y1 = (int) _y1;

    int xabs = (x1 > x0) ? x1 - x0 : x0 - x1;
    int yabs = (y1 > y0) ? y1 - y0 : y0 - y1;

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

void histogram_line (Histogram *hist, int x0, int y0, int x1, int y1)
{
    int *bins = hist->bins;

    int w = (int) hist->w;
    int h = (int) hist->h;

    if ((x0 < 0 && x1 < 0) ||
        (y0 < 0 && y1 < 0) ||
        (x0 > w && x1 > w) ||
        (y0 > h && y1 > h)
       )
        return;

    int xabs = (x1 > x0) ? x1 - x0 : x0 - x1;
    int yabs = (y1 > y0) ? y1 - y0 : y0 - y1;

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
                bins[y*w+x]++;
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
            if (x>=0 && y>=1 && x<w && y<h)
                bins[y*w+x]++;
        }
    }
    else
    {
        int x = x0;
        int y = y0;
        if (x>=0 && y>=0 && x<w && y<h)
            bins[y*w+x]++;
    }
}

static void draw_rect (uint32_t* pixels, uint32_t w, uint32_t h, uint32_t x0, uint32_t y0, uint32_t x1, uint32_t y1, uint32_t color)
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

    for (uint32_t yi=y0; yi<=y1 && yi<h; yi++)
    {
        pixels[yi*w + x0] = color;
        pixels[yi*w + x1] = color;
    }
    for (uint32_t xi=x0; xi<=x1 && xi<w; xi++)
    {
        pixels[y0*w + xi] = color;
        pixels[y1*w + xi] = color;
    }
}

static void transform_pos (const Area *srcArea, const Position *srcPos, const Area *dstArea, Position *dstPos)
{
    double xf = (srcPos->x - srcArea->x0) / (srcArea->x1 - srcArea->x0);
    double yf = (srcPos->y - srcArea->y0) / (srcArea->y1 - srcArea->y0);
    dstPos->x = xf * (dstArea->x1 - dstArea->x0) + dstArea->x0;
    dstPos->y = yf * (dstArea->y1 - dstArea->y0) + dstArea->y0;
}

static void get_active_area (CinterState *cs, Area *src, Area *dst)
{
    uint32_t w = cs->windowWidth;
    uint32_t h = cs->windowHeight - cs->statuslineEnabled * STATUSLINE_HEIGHT;

    double xp = (double) (cs->bordered + cs->margin) / w;
    double yp = (double) (cs->bordered + cs->margin) / h;

    double epsw = 1.0 / cs->windowWidth;
    double epsh = 1.0 / cs->windowHeight;

    dst->x0 = src->x0 + xp;
    dst->y0 = src->y0 + yp;
    dst->x1 = src->x1 - xp - epsw;
    dst->y1 = src->y1 - yp - epsh;
}


static int on_mouse_pressed (CinterState *cs, int xi, int yi, int button, int clicks)
{
    if (cs->activeSw == NULL)
    {
        cs->mouseState = MOUSE_STATE_NONE;
        return 0;
    }

    if (button == 1)
    {
        switch (cs->pressedModifiers)
        {
         case KMOD_NONE:
             {
                 cs->mouseState = MOUSE_STATE_SELECTING;
                 uint32_t w = cs->windowWidth;
                 uint32_t h = cs->windowHeight - cs->statuslineEnabled * STATUSLINE_HEIGHT;
                 cs->mouseWindowPos.x = (double) xi / w;
                 cs->mouseWindowPos.y = (double) yi / h;
                 cs->activeSw->selectedWindowArea0.x0 = cs->mouseWindowPos.x;
                 cs->activeSw->selectedWindowArea0.y0 = cs->mouseWindowPos.y;
                 cs->activeSw->selectedWindowArea0.x1 = cs->mouseWindowPos.x;
                 cs->activeSw->selectedWindowArea0.y1 = cs->mouseWindowPos.y;

                 cs->activeSw->selectedWindowArea1.x0 = cs->mouseWindowPos.x;
                 cs->activeSw->selectedWindowArea1.y0 = cs->mouseWindowPos.y;
                 cs->activeSw->selectedWindowArea1.x1 = cs->mouseWindowPos.x;
                 cs->activeSw->selectedWindowArea1.y1 = cs->mouseWindowPos.y;
                 break;
             }
         case KMOD_GUI:
             {
                 cs->mouseState = MOUSE_STATE_MOVING;
                 break;
             }
         default:
             break;
        }
    }
    else if (button == 3)
    {
        switch (cs->pressedModifiers)
        {
         case KMOD_NONE:
             {
                 cs->mouseState = MOUSE_STATE_MOVING;
                 break;
             }
         default:
             break;
        }
    }

    return 1;
}

static int on_mouse_released (CinterState *cs, int xi, int yi)
{
    switch (cs->mouseState)
    {
     case MOUSE_STATE_SELECTING:
         {
             Area *swa0 = & cs->activeSw->selectedWindowArea0;
             Area *swa1 = & cs->activeSw->selectedWindowArea1;
             if (swa1->x0 == swa1->x1 && swa1->y0 == swa1->y1)
                 cs->zoomEnabled ^= 1;
             else
             {

                 SubWindow *sw = cs->activeSw;
                 Area *dr  = & sw->dataRange;
                 Area *swa = & sw->selectedWindowArea1;
                 Area activeArea;
                 Area zoomWindowArea = {0,0,1,1};
                 get_active_area (cs, (cs->zoomEnabled ? & zoomWindowArea : & sw->windowArea), & activeArea);
                 Position wPos0 = {swa->x0, swa->y0};
                 Position wPos1 = {swa->x1, swa->y1};
                 Position dPos0, dPos1;

                 transform_pos (& activeArea, & wPos0, & sw->dataRange, & dPos0);
                 transform_pos (& activeArea, & wPos1, & sw->dataRange, & dPos1);

                 dr->x0 = dPos0.x;
                 dr->y0 = dPos0.y;
                 dr->x1 = dPos1.x;
                 dr->y1 = dPos1.y;
                 print ("zoom to new area [%f, %f] < [%f, %f] => ", dr->x0, dr->y0, dr->x1, dr->y1);
             }
             swa0->x0 = swa0->x1 = swa0->y0 = swa0->y1 = NaN;
             swa1->x0 = swa1->x1 = swa1->y0 = swa1->y1 = NaN;
             break;
         }
     default:
         {
             // do nothing
         }
    }

    cs->mouseState = MOUSE_STATE_NONE;

    return 1;
}

static int on_mouse_wheel (CinterState *cs, float xf, float yf)
{
    SubWindow *sw = cs->activeSw;
    if (sw && cs->mouseState == MOUSE_STATE_NONE)
    {
        if (cs->pressedModifiers == KMOD_GUI)
        {
            // zooming
            Area *dr = & sw->dataRange;
            double a = (sw->mouseDataPos.x - dr->x0) / (dr->x1 - dr->x0);
            double b = (sw->mouseDataPos.y - dr->y0) / (dr->y1 - dr->y0);

            double dx = -0.05 * (dr->x1 - dr->x0) * xf;
            double dy = -0.05 * (dr->y1 - dr->y0) * yf;
            dr->x0 += dx * a;
            dr->x1 -= dx * (1-a);
            dr->y0 += dy * b;
            dr->y1 -= dy * (1-b);
            return 1;
        }
        else
        {
            // moving
            SubWindow *sw = cs->activeSw;
            Area *dr = & sw->dataRange;

            double dx = xf * 0.01;
            double dy = yf * 0.01;

             Area activeArea;
             Area zoomWindowArea = {0,0,1,1};
             if (cs->zoomEnabled)
                 get_active_area (cs, & zoomWindowArea, & activeArea);
             else
                 get_active_area (cs, & sw->windowArea, & activeArea);

             dx /= activeArea.x1 - activeArea.x0;
             dy /= activeArea.y1 - activeArea.y0;

             dx *= dr->x1 - dr->x0;
             dy *= dr->y1 - dr->y0;

            //printn ("moving window [%f, %f] < [%f, %f] => ", dr->x0, dr->y0, dr->x1, dr->y1);
            dr->x0 += dx;
            dr->x1 += dx;
            dr->y0 -= dy;
            dr->y1 -= dy;
            //print ("[%f, %f] < [%f, %f] => ", dr->x0, dr->y0, dr->x1, dr->y1);
            return 1;
        }
    }
    return 0;
}

static int find_closest_point (Histogram *hist, uint32_t _x0, uint32_t _y0, uint32_t *_x, uint32_t *_y)
{
    // this algorithm takes a point (x0,y0) and spirals around it with a rectangular
    // spiral until it finds a point in the histogram that is set. When it is found,
    // it will make sure it will stop the loop after it is sure no other point can
    // be closer. The spiral looks like this:
    //   >>>>>>>>>|
    //   ^>>>>>>>||
    //   ^^>>>>>|||
    //   ^^^>>>||||
    //   ^^^^>|||||
    //   ^^^^<<||||
    //   ^^^<<<<|||
    //   ^^<<<<<<||
    //   ^<<<<<<<<|

    int x0 = (int) _x0;
    int y0 = (int) _y0;

    int dirx[4] = {1, 0, -1,  0};
    int diry[4] = {0, 1,  0, -1};

    int w = (int) hist->w;
    int h = (int) hist->h;
    int *bins = hist->bins;

    int x = x0;
    int y = y0;

    int maxSideLen = w+w+h+h;
    int minD2 = w*w+h*h;

    int sideLen = 1;
    int dir = 0;
    int sideCount = 0;
    int outsideCount = 0;
    int bestX = -1;
    int bestY = -1;
    while (outsideCount < 4 && sideLen <= maxSideLen)
    {
        int sideInside = (dirx[dir] && (0 <= y && y < h)) || (diry[dir] && (0 <= x && x < w));
        if (sideInside)
        {
            outsideCount = 0;
            for (int i=0; i<sideLen; i++)
            {
                if (0 <= y && y < h && 0 <= x && x < w)
                {
                    if (bins[y*w+x])
                    {
                        int newMaxSideLen = (int) (1.41421356 * sideLen) + 1;
                        if (maxSideLen < newMaxSideLen)
                            maxSideLen = newMaxSideLen;
                        int dx = x - x0;
                        int dy = y - y0;
                        int d2 = dx*dx+dy*dy;
                        if (minD2 > d2)
                        {
                            minD2 = d2;
                            bestX = x;
                            bestY = y;
                        }
                    }
                }
                x += dirx[dir];
                y += diry[dir];
            }
        }
        else
        {
            x += dirx[dir] * sideLen;
            y += diry[dir] * sideLen;
            outsideCount++;
        }

        dir = (dir + 1) % 4;
        if (++sideCount == 2)
        {
            sideCount = 0;
            sideLen++;
        }
    }
    if (bestX < 0 || bestY < 0)
    {
        print_debug ("no point found");
        return -1;
    }

    *_x = (uint32_t) bestX;
    *_y = (uint32_t) bestY;

    return 0;
}

static int on_mouse_motion (CinterState *cs, int xi, int yi)
{
    switch (cs->mouseState)
    {
     case MOUSE_STATE_NONE:
         {
             uint32_t w = cs->windowWidth;
             uint32_t h = cs->windowHeight - cs->statuslineEnabled * STATUSLINE_HEIGHT;
             Area activeArea;

             if (!cs->zoomEnabled)
                 cs->activeSw = NULL;

             for (int i=0; i<cs->numSubWindows; i++)
             {
                 SubWindow *sw;
                 if (cs->zoomEnabled)
                 {
                     sw = cs->activeSw;
                     Area zoomWindowArea = {0,0,1,1};
                     get_active_area (cs, & zoomWindowArea, & activeArea);
                 }
                 else
                 {
                     sw = & cs->subWindows[i];
                     get_active_area (cs, & sw->windowArea, & activeArea);
                 }


                 cs->mouseWindowPos.x = (double) xi / w;
                 cs->mouseWindowPos.y = (double) yi / h;
                 transform_pos (& activeArea, & cs->mouseWindowPos, & sw->dataRange, & sw->mouseDataPos);

                 if (activeArea.x0 <= cs->mouseWindowPos.x && cs->mouseWindowPos.x <= activeArea.x1 &&
                     activeArea.y0 <= cs->mouseWindowPos.y && cs->mouseWindowPos.y <= activeArea.y1)
                     cs->activeSw = sw;

                 if (cs->zoomEnabled)
                     break;
             }

             if (cs->trackingMode && cs->activeSw && cs->activeSw->numAttachedGraphs > 0)
             {
                 SubWindow *sw = cs->activeSw;
                 if (cs->zoomEnabled)
                 {
                     Area zoomWindowArea = {0,0,1,1};
                     get_active_area (cs, & zoomWindowArea, & activeArea);
                 }
                 else
                 {
                     get_active_area (cs, & sw->windowArea, & activeArea);
                 }

                 Histogram *hist = & sw->attachedGraphs[0]->hist;
                 uint32_t w = hist->w;
                 uint32_t h = hist->h;
                 int *bins = hist->bins;
                 if (!bins)
                     exit_error ("unexpected null pointer");
                 Area binArea = {0, 0, w, h};
                 Position binPos;
                 transform_pos (& activeArea, & cs->mouseWindowPos, & binArea, & binPos);
                 uint32_t x0 = (uint32_t) binPos.x;
                 uint32_t y0 = (uint32_t) binPos.y;

                 if (cs->trackingMode == 1)
                 {
                     if (x0 < w && y0 < h)
                     {
                         int _y0 = (int) y0;
                         int bestY = -1;
                         for (int dy=0; dy<h; dy++)
                         {
                             int yy0 = _y0 + dy;
                             int yy1 = _y0 - dy;
                             if (yy0 >= 0 && yy0 < h && bins[w * (uint32_t) yy0 + x0])
                             {
                                 bestY = yy0;
                                 break;
                             }
                             else if (yy1 >= 0 && yy1 < h && bins[w * (uint32_t) yy1 + x0])
                             {
                                 bestY = yy1;
                                 break;
                             }
                         }
                         if (bestY >= 0)
                         {
                             binPos.y = bestY;
                             transform_pos (& binArea, & binPos, & activeArea, & cs->mouseWindowPos);
                             transform_pos (& activeArea, & cs->mouseWindowPos, & sw->dataRange, & sw->mouseDataPos);
                         }
                     }
                 }
                 else if (cs->trackingMode == 2)
                 {
                     if (x0 < w && y0 < h)
                     {
                         int _x0 = (int) x0;
                         int bestX = -1;
                         for (int dx=0; dx<w; dx++)
                         {
                             int xx0 = _x0 + dx;
                             int xx1 = _x0 - dx;
                             if (xx0 >= 0 && xx0 < w && bins[w * y0 + (uint32_t) xx0])
                             {
                                 bestX = xx0;
                                 break;
                             }
                             else if (xx1 >= 0 && xx1 < w && bins[w * y0 + (uint32_t) xx1])
                             {
                                 bestX = xx1;
                                 break;
                             }
                         }
                         if (bestX >= 0)
                         {
                             binPos.x = bestX;
                             transform_pos (& binArea, & binPos, & activeArea, & cs->mouseWindowPos);
                             transform_pos (& activeArea, & cs->mouseWindowPos, & sw->dataRange, & sw->mouseDataPos);
                         }
                     }
                 }
                 else if (cs->trackingMode == 3)
                 {
                     uint32_t x, y;
                     if (find_closest_point (hist, x0, y0, & x, & y) >= 0)
                     {
                         binPos.x = x;
                         binPos.y = y;
                         transform_pos (& binArea, & binPos, & activeArea, & cs->mouseWindowPos);
                         transform_pos (& activeArea, & cs->mouseWindowPos, & sw->dataRange, & sw->mouseDataPos);
                     }
                 }
                 else
                     exit_error ("bug: %d", cs->trackingMode);
             }

             break;
         }
     case MOUSE_STATE_MOVING:
         {
             Position oldPos = {cs->mouseWindowPos.x, cs->mouseWindowPos.y};
             uint32_t w = cs->windowWidth;
             uint32_t h = cs->windowHeight - cs->statuslineEnabled * STATUSLINE_HEIGHT;
             cs->mouseWindowPos.x = (double) xi / w;
             cs->mouseWindowPos.y = (double) yi / h;
             SubWindow *sw = cs->activeSw;
             Area *dr = & sw->dataRange;
             double dx = (cs->mouseWindowPos.x - oldPos.x);
             double dy = (cs->mouseWindowPos.y - oldPos.y);
             if (!cs->zoomEnabled)
             {
                 dx /= (sw->windowArea.x1 - sw->windowArea.x0);
                 dy /= (sw->windowArea.y1 - sw->windowArea.y0);
             }
             dx *= dr->x1 - dr->x0;
             dy *= dr->y1 - dr->y0;

             //printn ("moving window [%f, %f] < [%f, %f] => ", dr->x0, dr->y0, dr->x1, dr->y1);
             dr->x0 -= dx;
             dr->x1 -= dx;
             dr->y0 -= dy;
             dr->y1 -= dy;
             //print ("[%f, %f] < [%f, %f] => ", dr->x0, dr->y0, dr->x1, dr->y1);

             break;
         }
     case MOUSE_STATE_SELECTING:
         {
             uint32_t w = cs->windowWidth;
             uint32_t h = cs->windowHeight - cs->statuslineEnabled * STATUSLINE_HEIGHT;
             cs->mouseWindowPos.x = (double) xi / w;
             cs->mouseWindowPos.y = (double) yi / h;
             Area *swa0 = & cs->activeSw->selectedWindowArea0;
             Area *swa1 = & cs->activeSw->selectedWindowArea1;
             swa0->x1 = cs->mouseWindowPos.x;
             swa0->y1 = cs->mouseWindowPos.y;
             if (swa0->x0 < swa0->x1)
             {
                 swa1->x0 = swa0->x0;
                 swa1->x1 = swa0->x1;
             }
             else
             {
                 swa1->x0 = swa0->x1;
                 swa1->x1 = swa0->x0;
             }
             if (swa0->y0 < swa0->y1)
             {
                 swa1->y0 = swa0->y0;
                 swa1->y1 = swa0->y1;
             }
             else
             {
                 swa1->y0 = swa0->y1;
                 swa1->y1 = swa0->y0;
             }

             double minDiffX = 16.0 / w;
             double minDiffY = 16.0 / h;

             double dx = swa1->x1 - swa1->x0;
             double dy = swa1->y1 - swa1->y0;

             if (dx >= minDiffX || dy >= minDiffY)
             {
                 Area zoomWindowArea = {0,0,1,1};
                 Area *wa = cs->zoomEnabled ? & zoomWindowArea : & cs->activeSw->windowArea;
                 if (dx < minDiffX)
                 {
                     swa1->x0 = wa->x0;
                     swa1->x1 = wa->x1;
                 }
                 if (dy < minDiffY)
                 {
                     swa1->y0 = wa->y0;
                     swa1->y1 = wa->y1;
                 }
             }
             break;
         }
     case MOUSE_STATE_RESIZING_L:
         {
             break;
         }
     case MOUSE_STATE_RESIZING_R:
         {
             break;
         }
     case MOUSE_STATE_RESIZING_T:
         {
             break;
         }
     case MOUSE_STATE_RESIZING_B:
         {
             break;
         }
     case MOUSE_STATE_RESIZING_TL:
         {
             break;
         }
     case MOUSE_STATE_RESIZING_TR:
         {
             break;
         }
     case MOUSE_STATE_RESIZING_BL:
         {
             break;
         }
     case MOUSE_STATE_RESIZING_BR:
         {
             break;
         }
     default:
         exit_error ("bug");
    }
    cs->mouse.x = xi;
    cs->mouse.y = yi;
    return 1;
}

void cycle_line_type (SubWindow *sw)
{
    if (!sw)
        return;

    for (int i=0; i<sw->numAttachedGraphs; i++)
    {
        GraphAttacher *attacher = sw->attachedGraphs[i];
        switch (attacher->plotType)
        {
         case 'p': attacher->plotType = '+'; break;
         case '+': attacher->plotType = 'l'; break;
         case 'l': attacher->plotType = 's'; break;
         case 's': attacher->plotType = 'p'; break;
         default: print_error ("unknown line type '%c'", attacher->plotType); break;
        }
    }
}


static int on_keyboard (CinterState *cs, int key, int mod, int pressed, int repeat)
{
    // FIXME: If both the left and right key of the same modifier gets pressed at
    // the same time and then one gets released, the state of pressedModifiers
    // gets zeroed out
    if (key == SDLK_LSHIFT || key == SDLK_RSHIFT)
    {
        if (pressed)
            cs->pressedModifiers |= KMOD_SHIFT;
        else
            cs->pressedModifiers &= ~KMOD_SHIFT;
        return 0;
    }
    else if (key == SDLK_LGUI || key == SDLK_RGUI)
    {
        if (pressed)
            cs->pressedModifiers |= KMOD_GUI;
        else
            cs->pressedModifiers &= ~KMOD_GUI;
        return 0;
    }
    else if (key == SDLK_LALT || key == SDLK_RALT)
    {
        if (pressed)
            cs->pressedModifiers |= KMOD_ALT;
        else
            cs->pressedModifiers &= ~KMOD_ALT;
        return 0;
    }
    else if (key == SDLK_LCTRL || key == SDLK_RCTRL)
    {
        if (pressed)
            cs->pressedModifiers |= KMOD_CTRL;
        else
            cs->pressedModifiers &= ~KMOD_CTRL;
        return 0;
    }

    int unhandled = 0;
    if (pressed)
    {
        if (!repeat)
        {
           if (mod == KMOD_NONE)
           {
               switch (key)
               {
                case 'a': autoscale (cs->activeSw); break;
                case 'c': cycle_graph_order (cs); break;
                case 'f': set_fullscreen (cs, ! cs->fullscreen); break;
                case 'g': set_grid_enabled (cs, ! cs->gridEnabled); break;
                case 'h': toggle_help (cs); break;
                case 'm': toggle_mouse (cs); break;
                case 's': toggle_statusline (cs); break;
                case 'q': quit (cs); break;
                case 'u': undo_zooming (cs->activeSw); break;
                case 'e': force_refresh (cs); break;
                case 't': set_tracking_mode (cs, cs->trackingMode + 1); break;
                case 'l': cycle_line_type (cs->activeSw); break;
                          //case 'x': exit_zoom (cs); break;
                case ' ': toggle_paused (cs); break;

                case '0':
                case '1':
                case '2':
                case '3':
                case '4':
                case '5':
                case '6':
                case '7':
                case '8':
                case '9':
                          {
                              int index = key - '1';
                              if (index < 0)
                                  index = 9;
                              float shades[10] = {0.0f, 0.04f, 0.06f, 0.08f, 0.10f, 0.14f, 0.2f, 0.4f, 0.7f, 1.0f};
                              cs->bgShade = shades[index];
                              break;
                          }
                case 27:
                          {
                              cs->mouseState = MOUSE_STATE_NONE;
                              if (cs->activeSw)
                              {
                                  Area *a0 = & cs->activeSw->selectedWindowArea0;
                                  Area *a1 = & cs->activeSw->selectedWindowArea1;
                                  bzero (a0, sizeof (*a0));
                                  bzero (a1, sizeof (*a1));
                              }
                              break;
                          }
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
        if (unhandled || repeat)
        {
            double zf = 0.05;
            double mf = 0.025;
            int unhandled2 = 0;
            switch (key)
            {
             case 'n': sub_window_change (cs,  1); break;
             case 'p': sub_window_change (cs, -1); break;
             case '+': zoom (cs->activeSw,  zf,  zf); break;
             case '-': zoom (cs->activeSw, -zf, -zf); break;
             case ',': zoom (cs->activeSw, -zf,  0.00); break;
             case '.': zoom (cs->activeSw,  zf,  0.00); break;
             case SDLK_UP:    move (cs->activeSw,  0.00, -mf); break;
             case SDLK_DOWN:  move (cs->activeSw,  0.00,  mf); break;
             case SDLK_LEFT:  move (cs->activeSw, -mf,  0.00); break;
             case SDLK_RIGHT: move (cs->activeSw,  mf,  0.00); break;
             default: unhandled2 = 1; break;
            }
            unhandled = (unhandled && unhandled2);
        }
    }

    if (cs->pressedModifiers == KMOD_CTRL && key == 'c')
    {
        quit (cs);
        unhandled = 0;
    }

    if (unhandled)
    {
        print_debug ("key: %c (%d), pressed: %d, repeat: %d", key, key, pressed, repeat);
        return 0;
    }

    return 1;
}


GraphAttacher *graph_attach (CinterState *cs, CinterGraph *graph, uint32_t windowIndex, HistogramFun histogramFun, char plotType, char *colorSpec, uint32_t numColors)
{
    if (windowIndex >= cs->numSubWindows)
    {
        print_error ("windowIndex %d out of range", windowIndex);
        return NULL;
    }

    SubWindow *sw = & cs->subWindows[windowIndex];
    if (sw->numAttachedGraphs >= sw->maxNumAttachedGraphs)
    {
        print_error ("maximum number of attached graphs reached");
        return NULL;
    }

    GraphAttacher *attacher = safe_calloc (1, sizeof (*attacher));
    attacher->graph = graph;
    attacher->plotType = plotType;
    attacher->hist.w = 0;
    attacher->hist.h = 0;
    attacher->hist.bins = NULL;
    attacher->histogramFun = histogramFun ? histogramFun : make_histogram;
    attacher->colorScheme = make_color_scheme (colorSpec, numColors);
    attacher->lastGraphCounter = 0;

    sw->attachedGraphs[sw->numAttachedGraphs++] = attacher;
    return attacher;
}

CinterGraph *graph_new (uint32_t len)
{
    CinterGraph *graph = safe_calloc (1, sizeof (*graph));
    graph->len = len;
    atomic_flag_clear (& graph->readAccess);
    atomic_flag_clear (& graph->insertAccess);

    size_t itemSize = 2 * sizeof (double);

    if (graph->len == 0)
    {
        // lazy infinite length
        graph->sb = stream_buffer_create (INITIAL_VARIABLE_LENGTH, itemSize);
    }
    else
    {
        uint32_t requestedLen = len;
        graph->sb = stream_buffer_create (requestedLen, itemSize);
    }

    return graph;
}

void graph_delete (CinterGraph *graph)
{
    if (!graph)
        return;

    stream_buffer_destroy (graph->sb);
    free (graph);
}

void graph_add_point (CinterGraph *graph, double x, double y)
{
    while (paused)
        usleep (10000);

    StreamBuffer *sb = graph->sb;
    assert (sb);

    if (graph->len == 0 &&
        sb->counter == sb->len &&
        sb->len <= MAX_VARIABLE_LENGTH)
    {

        wait_for_access (& graph->readAccess);
        stream_buffer_resize (sb, sb->len << 1);
        release_access (& graph->readAccess);
    }

    double xy[2] = {x,y};
    wait_for_access (& graph->insertAccess);
    stream_buffer_insert (sb, xy);
    release_access (& graph->insertAccess);
}

void graph_remove_points (CinterGraph *graph)
{
    while (paused)
        usleep (10000);

    StreamBuffer *sb = graph->sb;
    assert (sb);

    wait_for_access (& graph->readAccess);
    stream_buffer_reset (sb);
    release_access (& graph->readAccess);
}

uint64_t make_histogram (Histogram *hist, CinterGraph *graph, char plotType)
{
    uint64_t counter = 0;
    int *bins  = hist->bins;
    uint32_t w = hist->w;
    uint32_t h = hist->h;

    wait_for_access (& graph->readAccess);

    double xmin = (double) hist->dataRange.x0;
    double xmax = (double) hist->dataRange.x1;
    double ymin = (double) hist->dataRange.y0;
    double ymax = (double) hist->dataRange.y1;

    double (*xys)[2];
    uint32_t len;
    wait_for_access (& graph->insertAccess);
    stream_buffer_get (graph->sb, & xys, & len);
    if (!len)
    {
        release_access (& graph->insertAccess);
        release_access (& graph->readAccess);
        return 0;
    }
    if (graph->len && graph->len < len)
    {
        xys += (len - graph->len);
        len = graph->len;
    }
    counter = graph->sb->counter;
    release_access (& graph->insertAccess);

    uint32_t nBins = w * h;
    for (uint32_t i=0; i<nBins; i++)
        bins[i] = 0;

    double invXRange = 1.0 / (xmax - xmin);
    double invYRange = 1.0 / (ymax - ymin);
    if (plotType == 'p')
    {
        for (uint32_t i=0; i<len; i++)
        {
            double x = xys[i][0];
            double y = xys[i][1];

            if (isnan (x) || isnan (y))
                continue;

            int xi = (int) ((w-1) * (x - xmin) * invXRange);
            int yi = (int) ((h-1) * (y - ymin) * invYRange);
            if (xi >= 0 && xi < w && yi >= 0 && yi < h)
                bins[(uint32_t) yi*w + (uint32_t) xi]++;
        }
    }
    else if (plotType == '+')
    {
        for (uint32_t i=0; i<len; i++)
        {
            double x = xys[i][0];
            double y = xys[i][1];

            if (isnan (x) || isnan (y))
                continue;

            int xi = (int) ((w-1) * (x - xmin) * invXRange);
            int yi = (int) ((h-1) * (y - ymin) * invYRange);
            int xx[9] = { 0,  0, -2, -1, 0, 1, 2, 0, 0};
            int yy[9] = {-2, -1,  0,  0, 0, 0, 0, 1, 2};
            for (int j=0; j<9; j++)
            {
                int xp = xi+xx[j];
                int yp = yi+yy[j];
                if (xp >= 0 && xp < w && yp >= 0 && yp < h)
                    bins[(uint32_t) yp*w + (uint32_t) xp]++;
            }
        }
    }
    else if (plotType == 'l')
    {
        for (uint32_t i=0; i<len-1; i++)
        {
            double x0 = xys[i][0];
            double y0 = xys[i][1];
            double x1 = xys[i+1][0];
            double y1 = xys[i+1][1];

            if (isnan (x0) || isnan (y0) || isnan (x1) || isnan (y1))
                continue;

            int xi0 = (int) ((w-1) * (x0 - xmin) * invXRange);
            int yi0 = (int) ((h-1) * (y0 - ymin) * invYRange);
            int xi1 = (int) ((w-1) * (x1 - xmin) * invXRange);
            int yi1 = (int) ((h-1) * (y1 - ymin) * invYRange);
            histogram_line (hist, xi0, yi0, xi1, yi1);
        }
    }
    else if (plotType == 's')
    {
        for (uint32_t i=0; i<len-1; i++)
        {
            double x0 = xys[i][0];
            double y0 = xys[i][1];
            double x1 = xys[i+1][0];
            double y1 = xys[i+1][1];

            if (isnan (x0) || isnan (y0) || isnan (x1) || isnan (y1))
                continue;

            int xi0 = (int) ((w-1) * (x0 - xmin) * invXRange);
            int yi0 = (int) ((h-1) * (y0 - ymin) * invYRange);
            int xi1 = (int) ((w-1) * (x1 - xmin) * invXRange);
            int yi1 = (int) ((h-1) * (y1 - ymin) * invYRange);
            histogram_line (hist, xi0, yi0, xi1, yi0);
            histogram_line (hist, xi1, yi0, xi1, yi1);
        }
    }
    else
    {
        exit_error ("unknown plot type '%c'", plotType);
    }

    release_access (& graph->readAccess);
    return counter;
}

enum {
    ALIGN_TL, ALIGN_TC, ALIGN_TR,
    ALIGN_ML, ALIGN_MC, ALIGN_MR,
    ALIGN_BL, ALIGN_BC, ALIGN_BR
};

void lighten_pixel (uint32_t *pixel, int amount)
{
    int b =  *pixel        & 0xff;
    int g = (*pixel >> 8)  & 0xff;
    int r = (*pixel >> 16) & 0xff;
    r = (int) (r + amount);
    g = (int) (g + amount);
    b = (int) (b + amount);

    if (r > 255) r = 255;
    if (g > 255) g = 255;
    if (b > 255) b = 255;

    if (r < 0) r = 0;
    if (g < 0) g = 0;
    if (b < 0) b = 0;

    *pixel = MAKE_COLOR (r,g,b);
}

uint32_t draw_text (uint32_t* pixels, uint32_t w, uint32_t h, uint32_t x0, uint32_t y0, uint32_t color, int transparent, char *text, uint32_t scale, int alignment )
{
    if (!pixels)
        return 0;

    uint32_t fh = 8*scale;
    uint32_t fw = 6*scale;
    uint32_t spacing = 0;

    char *p = text;
    uint32_t cols = 256;

    uint32_t numChars = (uint32_t) strlen (text);
    uint32_t boxWidth = numChars * fw + (numChars - 1) * spacing;
    uint32_t boxHeight = fh; // ignoring multiline text

    switch (alignment)
    {
     case ALIGN_TL: 
     case ALIGN_TC:
     case ALIGN_TR:
         break;

     case ALIGN_ML:
     case ALIGN_MC:
     case ALIGN_MR:
         y0 -= boxHeight >> 1;
         break;

     case ALIGN_BL:
     case ALIGN_BC:
     case ALIGN_BR:
         y0 -= boxHeight;
         break;
    }

    switch (alignment)
    {
     case ALIGN_TL:
     case ALIGN_ML:
     case ALIGN_BL:
         break;

     case ALIGN_TC:
     case ALIGN_MC:
     case ALIGN_BC:
         x0 -= boxWidth >> 1;
         break;

     case ALIGN_TR:
     case ALIGN_MR:
     case ALIGN_BR:
         x0 -= boxWidth;
         break;
    }

    uint32_t x = x0;
    uint32_t y = y0;

    int i=0;
    while (*p)
    {
        if ((++i == cols) || (*p == '\n'))
        {
            int wrap = (i == cols);
            i = 0;
            x = x0;
            y += fh + 4 * spacing;
            if (wrap) continue;
        }
        else if (*p == '\t')
        {
            while (++i % 4)
                x += fw + spacing;
        }
        else
        {
            if (x > 0 && x < w-1-fw && 
               (y > 0 && y < h-1-fh))
            {
                for (uint32_t yi=0; yi<fh; yi++)
                    for (uint32_t xi=0; xi<fw; xi++)
                        if (!((font[(int)(*p)][yi/scale] >> (11-(xi/scale))) & 1))
                            pixels[(y+yi)*w + (x+xi)] = color;
                        else if (!transparent)
                        {
                            uint32_t *pixel = & pixels[(y+yi)*w + (x+xi)];
                            lighten_pixel (pixel, -100);
                        }
            }
            x += fw + spacing;
        }
        p++;
    }
    return y - y0;
}

void draw_data_line (uint32_t *pixels, uint32_t w, uint32_t h, CinterState *cs, SubWindow *sw, double pos, int vertical, uint32_t color)
{
    Area activeArea;
    Area zoomWindowArea = {0,0,1,1};
    get_active_area (cs, (cs->zoomEnabled ? & zoomWindowArea : & sw->windowArea), & activeArea);

    Position dataPos0, dataPos1;
    if (vertical)
    {
        dataPos0.x = pos;
        dataPos0.y = sw->dataRange.y0;
        dataPos1.x = pos;
        dataPos1.y = sw->dataRange.y1;
    }
    else
    {
        dataPos0.x = sw->dataRange.x0;
        dataPos0.y = pos;
        dataPos1.x = sw->dataRange.x1;
        dataPos1.y = pos;
    }

    Position winPos0, winPos1;
    transform_pos (& sw->dataRange, & dataPos0, & activeArea, & winPos0);
    transform_pos (& sw->dataRange, & dataPos1, & activeArea, & winPos1);

    uint32_t x0 = (uint32_t) (winPos0.x * w);
    uint32_t y0 = (uint32_t) (winPos0.y * h);
    uint32_t x1 = (uint32_t) (winPos1.x * w);
    uint32_t y1 = (uint32_t) (winPos1.y * h);

    if (vertical)
    {
        if (activeArea.x0 <= winPos0.x && winPos0.x <= activeArea.x1)
            lineRGBA (pixels, w, h, x0, y0, x1, y1, color);
    }
    else
    {
        if (activeArea.y0 <= winPos0.y && winPos0.y <= activeArea.y1)
            lineRGBA (pixels, w, h, x0, y0, x1, y1, color);
    }
}

#define HELP_TEXT(text) \
draw_text (pixels, cs->windowWidth, cs->windowHeight, x0, y0, textColor, transparent, text, 2, ALIGN_TL); y0+=16

static void plot_data (CinterState *cs, uint32_t *pixels)
{
    uint32_t activeColor    = make_gray (1.0f);
    uint32_t inactiveColor  = make_gray (0.4f);
    uint32_t crossHairColor = MAKE_COLOR (0,255,255);
    uint32_t bgColor        = make_gray (cs->bgShade);
    //uint32_t selectColor    = make_gray (cs->bgShade + (cs->bgShade < 0.5f ? 0.2f : -0.2f));
    uint32_t gridColor0     = make_gray (0.2f);
    uint32_t gridColor1     = make_gray (0.4f);

    uint32_t forceRefresh = cs->forceRefresh;
    cs->forceRefresh = 0;

    uint32_t w = cs->windowWidth;
    uint32_t h = cs->windowHeight - cs->statuslineEnabled * STATUSLINE_HEIGHT;

    for (uint32_t wi=0; wi < (cs->numSubWindows); wi++)
    {
        SubWindow *sw = & cs->subWindows[wi];

        uint32_t x0, y0, x1, y1;

        if (cs->zoomEnabled)
        {
            if (cs->activeSw != sw)
                continue;

            x0 = cs->margin;
            y0 = cs->margin;
            x1 = w - cs->margin;
            y1 = h - cs->margin;
        }
        else
        {
            if (sw->windowArea.x0 > sw->windowArea.x1) exit_error ("bug");
            if (sw->windowArea.y0 > sw->windowArea.y1) exit_error ("bug");

            x0 = (uint32_t) (sw->windowArea.x0 * w) + cs->margin;
            y0 = (uint32_t) (sw->windowArea.y0 * h) + cs->margin;
            x1 = (uint32_t) (sw->windowArea.x1 * w) - cs->margin;
            y1 = (uint32_t) (sw->windowArea.y1 * h) - cs->margin;

            if (x0 >= w) exit_error ("bug %u >= %u", x0, w);
            if (x1 >= w) exit_error ("bug %u >= %u", x1, w);
            if (y0 >= h) exit_error ("bug %u >= %u", y0, h);
            if (y1 >= h) exit_error ("bug %u >= %u", y1, h);

            if (cs->bordered)
            {
                draw_rect (pixels, w, h, x0, y0, x1, y1, (sw == cs->activeSw && cs->mouseEnabled) ? activeColor : inactiveColor);
                x0++; y0++; x1--; y1--;
            }
        }
        uint32_t subWidth  = x1 - x0;
        uint32_t subHeight = y1 - y0;
        if (subWidth > w || subHeight > h)
            exit_error ("bug");

        for (uint32_t y=y0; y<y1; y++)
            for (uint32_t x=x0; x<x1;  x++)
                pixels[y*w + x] = bgColor;

        if (cs->continuousScroll && !paused)
            continuous_scroll_update (sw);

        if (cs->gridEnabled)
        {
            // keep in mind y1 < y0 because plot window has positive y-data direction upwards
            double dy = __exp10 (floor (log10 (sw->dataRange.y0 - sw->dataRange.y1)));
            double y0 = ceil (sw->dataRange.y0 / dy) * dy;
            double y1 = floor (sw->dataRange.y1 / dy) * dy;
            int numTens = (int) ((y0 - y1) / dy);
            if (numTens < 1)
                numTens = 1;
            int numSubs = 1;
            while (numSubs * numTens < 8)
                numSubs *= 2;
            int cnt = 0;
            for (double y=y1; y<y0 && cnt<100 && subHeight > 200; y+=dy/(numSubs*5))
            {
                cnt++;
                if (y1 <= y && y <= y0)
                    draw_data_line (pixels, w, h, cs, sw, y, 0, gridColor0);
            }
            cnt = 0;
            uint32_t lastYi = 0;
            for (double y=y1; y<y0 && cnt<100; y+=dy/numSubs)
            {
                cnt++;
                if (y < sw->dataRange.y1 || sw->dataRange.y0 < y)
                    continue;

                Area activeArea;
                Area zoomWindowArea = {0,0,1,1};
                get_active_area (cs, (cs->zoomEnabled ? & zoomWindowArea : & sw->windowArea), & activeArea);

                uint32_t textColor = make_gray (0.9f);
                int transparent = 1;
                char text[256];
                snprintf (text, sizeof (text), "%g", y);

                Position dataPos;
                dataPos.x = sw->dataRange.x0;
                dataPos.y = y;

                Position winPos;
                transform_pos (& sw->dataRange, & dataPos, & activeArea, & winPos);

                uint32_t xi = (uint32_t) (winPos.x * w) + 2;
                uint32_t yi = (uint32_t) (winPos.y * h);
                uint32_t scale = 1+(cs->zoomEnabled || cs->fullscreen);
                if (abs ((int) yi - (int) lastYi) > 10*scale)
                {
                    draw_data_line (pixels, w, h, cs, sw, y, 0, gridColor1);
                    draw_text (pixels, w, h, xi, yi, textColor, transparent, text, scale, ALIGN_BL);
                    lastYi = yi;
                }
            }
        }

        for (uint32_t gi=0; gi<sw->numAttachedGraphs; gi++)
        {
            GraphAttacher *attacher = sw->attachedGraphs[(gi + cs->graphOrder) % (sw->numAttachedGraphs)];
            Histogram *hist = & attacher->hist;

            int updateHistogram =
                (forceRefresh) ||
                (attacher->lastGraphCounter != attacher->graph->sb->counter) ||
                (attacher->lastPlotType != attacher->plotType) ||
                (hist->dataRange.x0 != sw->dataRange.x0) ||
                (hist->dataRange.x1 != sw->dataRange.x1) ||
                (hist->dataRange.y0 != sw->dataRange.y0) ||
                (hist->dataRange.y1 != sw->dataRange.y1);

            if (hist->bins == NULL)
            {
                hist->w = subWidth;
                hist->h = subHeight;
                hist->bins = safe_calloc (hist->w * hist->h, sizeof (hist->bins[0]));
                updateHistogram = 1;
            }
            else if (hist->w != subWidth || hist->h != subHeight)
            {
                free (hist->bins);
                hist->w = subWidth;
                hist->h = subHeight;
                hist->bins = safe_calloc (hist->w * hist->h, sizeof (hist->bins[0]));
                updateHistogram = 1;
            }

            if (updateHistogram)
            {
                hist->dataRange.x0 = sw->dataRange.x0;
                hist->dataRange.x1 = sw->dataRange.x1;
                hist->dataRange.y0 = sw->dataRange.y0;
                hist->dataRange.y1 = sw->dataRange.y1;
                attacher->lastGraphCounter = attacher->histogramFun (hist, attacher->graph, attacher->plotType);
                attacher->lastPlotType = attacher->plotType;
            }
            else
            {
                //foobar;
            }

            int *bins = hist->bins;
            uint32_t *colors = attacher->colorScheme->colors;
            uint32_t nLevels = attacher->colorScheme->nLevels;

            uint32_t mousePosX = (uint32_t) (cs->mouseWindowPos.x * w);
            uint32_t mousePosY = (uint32_t) (cs->mouseWindowPos.y * h);


            for (uint32_t yi=0; yi<subHeight; yi++)
            {
                for (uint32_t xi=0; xi<subWidth;  xi++)
                {
                    uint32_t x = x0 + xi;
                    uint32_t y = y0 + yi;

                    int regionSelected;
                    if (isnan (sw->selectedWindowArea1.x0) || isnan (sw->selectedWindowArea1.x1) ||
                        isnan (sw->selectedWindowArea1.y0) || isnan (sw->selectedWindowArea1.y1))
                    {
                        regionSelected = 0;
                    }
                    else
                    {

                        uint32_t x0 = (uint32_t) (sw->selectedWindowArea1.x0 * w);
                        uint32_t y0 = (uint32_t) (sw->selectedWindowArea1.y0 * h);
                        uint32_t x1 = (uint32_t) (sw->selectedWindowArea1.x1 * w);
                        uint32_t y1 = (uint32_t) (sw->selectedWindowArea1.y1 * h);
                        if (x0 <= x && x <= x1 && y0 <= y && y <= y1)
                            regionSelected = 1;
                        else
                            regionSelected = 0;
                    }

                    uint32_t *pixel = & pixels[y*w + x];
                    int cnt = bins[yi * subWidth + xi];
                    if (cs->mouseEnabled && sw == cs->activeSw && (x == mousePosX || y == mousePosY))
                        *pixel = crossHairColor;
                    else if (cnt > 0)
                    {
                        uint32_t color = colors[MIN (nLevels, (uint32_t) cnt) - 1];
                        *pixel = color;
                    }

                    if (regionSelected)
                        lighten_pixel (pixel, 50);
                }
            }
        }
    }
    if (cs->statuslineEnabled && cs->activeSw)
    {
        uint32_t textColor = make_gray (0.9f);
        int transparent = 1;
        uint32_t x0 = 10;
        uint32_t y0 = cs->windowHeight - STATUSLINE_HEIGHT / 2;
        char text[256];
        double mx = cs->activeSw->mouseDataPos.x;
        double my = cs->activeSw->mouseDataPos.y;
        char *tm[] = {"(none)", "(x-fix, y-find)", "(x-find, y-fix)", "(x-find, y-find)"};
        snprintf (text, sizeof (text), "(x,y) = (%f,%f) trackingMode:%s", mx, my, tm[cs->trackingMode]);
        draw_text (pixels, cs->windowWidth, cs->windowHeight, x0, y0, textColor, transparent, text, 2, ALIGN_ML);

        char *title = cs->activeSw->title;
        if (title)
        {
            snprintf (text, sizeof (text), "<%s>", title);
            x0 = cs->windowWidth - 10;
            draw_text (pixels, cs->windowWidth, cs->windowHeight, x0, y0, textColor, transparent, text, 2, ALIGN_MR);
        }
    }

    if (cs->showHelp)
    {
        uint32_t textColor = make_gray (0.9f);
        int transparent = 0;
        uint32_t x0 = 10;
        uint32_t y0 = 20;
        char text[256];
        snprintf (text, sizeof (text), "h - toggle help");
        HELP_TEXT (" Keyboard bindings:");
        HELP_TEXT ("   a       - autoscale");
        HELP_TEXT ("   c       - cycle graph order");
        HELP_TEXT ("   e       - force refresh");
        HELP_TEXT ("   f       - toggle fullscreen");
        HELP_TEXT ("   g       - toggle grid");
        HELP_TEXT ("   h       - toggle help");
        HELP_TEXT ("   l       - cycle between line types");
        HELP_TEXT ("   m       - toggle mouse crosshair");
        HELP_TEXT ("   n       - next sub window");
        HELP_TEXT ("   p       - prev sub window");
        HELP_TEXT ("   q       - quit");
        HELP_TEXT ("   s       - toggle statusline");
        HELP_TEXT ("   t       - cycle between tracking modes");
        HELP_TEXT ("   u       - reset zoom to default");
        HELP_TEXT ("   <space> - pause new data");
        HELP_TEXT ("   [0-9]   - set background shade");
        HELP_TEXT ("   +       - zoom xy in");
        HELP_TEXT ("   -       - zoom xy out");
        HELP_TEXT ("   .       - zoom x in");
        HELP_TEXT ("   ,       - zoom x out");
        HELP_TEXT ("   <up>    - move center point up");
        HELP_TEXT ("   <down>  - move center point down");
        HELP_TEXT ("   <left>  - move center point left");
        HELP_TEXT ("   <right> - move center point right");
        HELP_TEXT ("   <up>    - move center point up");
        HELP_TEXT ("   <C-c>   - quit");
        HELP_TEXT ("");
        HELP_TEXT (" Mouse gestures:");
        HELP_TEXT ("   click in sub window          - enter or exit zoom mode");
        HELP_TEXT ("   move cursor                  - read off crosshair coordinates");
        HELP_TEXT ("   click select area            - zoom to area, <esc> to cancel");
        HELP_TEXT ("   two finger click and drag    - move center point");
        HELP_TEXT ("   scroll motion x/y            - move center point");
        HELP_TEXT ("   <GUI>-button + scroll motion - zoom in/out");
    }
}

int make_sub_windows (CinterState *cs, uint32_t nRows, uint32_t nCols, uint32_t bordered, uint32_t margin)
{
    if (cs->subWindows)
    {
        print_error ("cs->subWindows must not have been set previously");
        return -1;
    }

    cs->numSubWindows = nRows * nCols;
    cs->bordered = bordered;
    cs->margin   = margin;

    uint32_t n = nRows * nCols;
    cs->subWindows = safe_calloc (n, sizeof (cs->subWindows[0]));

    double dy = 1.0 / nRows;
    double dx = 1.0 / nCols;

    for (uint32_t ri=0; ri<nRows; ri++)
    {
        for (uint32_t ci=0; ci<nCols; ci++)
        {
            SubWindow *sw = & cs->subWindows[ri * nCols + ci];
            sw->maxNumAttachedGraphs = MAX_NUM_ATTACHED_GRAPHS;
            sw->attachedGraphs = safe_calloc (sw->maxNumAttachedGraphs, sizeof (*sw->attachedGraphs));
            sw->numAttachedGraphs = 0;

            sw->selectedWindowArea0.x0 = NaN;
            sw->selectedWindowArea0.x1 = NaN;
            sw->selectedWindowArea0.y0 = NaN;
            sw->selectedWindowArea0.y1 = NaN;

            sw->selectedWindowArea1.x0 = NaN;
            sw->selectedWindowArea1.x1 = NaN;
            sw->selectedWindowArea1.y0 = NaN;
            sw->selectedWindowArea1.y1 = NaN;

            sw->dataRange.x0 = -1;
            sw->dataRange.x1 =  1;
            sw->dataRange.y0 =  1;
            sw->dataRange.y1 = -1;
            memcpy (& sw->defaultDataRange, & sw->dataRange, sizeof (Area));

            sw->windowArea.x0 = (ci    ) * dx;
            sw->windowArea.x1 = (ci + 1) * dx;
            sw->windowArea.y0 = (ri    ) * dy;
            sw->windowArea.y1 = (ri + 1) * dy;
        }
    }

    return 0;
}

void set_sub_window_title (CinterState *cs, uint32_t windowIndex, char *title)
{
    if (windowIndex >= cs->numSubWindows)
        exit_error ("windowIndex %d out of range", windowIndex);
    SubWindow *sw = & cs->subWindows[windowIndex];
    sw->title = strdup (title);
}

SubWindow *get_sub_window (CinterState *cs, uint32_t windowIndex)
{
    if (windowIndex >= cs->numSubWindows)
        exit_error ("windowIndex %d out of range", windowIndex);
    SubWindow *sw = & cs->subWindows[windowIndex];
    return sw;
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
    if (userMainRetVal)
        data->cs->running = 0;
    return NULL;
}

static void update_image (CinterState *cs, SDL_Texture *texture, int init)
{
    uint32_t* pixels;
    int wb;
    int status = SDL_LockTexture (texture, NULL, (void**) & pixels, & wb);
    if (status)
        exit_error ("texture: %p, status: %d: %s\n", (void*) texture, status, SDL_GetError());

    if (init)
        memset (pixels, 0x11, sizeof (uint32_t) * cs->windowWidth * cs->windowHeight);
    else
        cs->plot_data (cs, pixels);
    SDL_UnlockTexture (texture);
}

static void *abort_thread (void *unused)
{
    usleep (100000);
    pid_t pid = getpid ();
    print_debug ("sending SIGSTOP, attach debugger to %u", pid);
    raise(SIGSTOP);
    return NULL;
}

static void signal_handler (int sig)
{
    if (interrupted)
    {
        printn ("Ctrl+C received again, attach debugger? [y/N] ");
        char buf[8];
        fgets (buf, sizeof (buf), stdin);

        if (buf[0] == 'y')
        {
            pthread_t abortThread;
            if (pthread_create (& abortThread, NULL, abort_thread, NULL))
                exit_error ("could not create thread\n");
            interrupted = 0;
        }
        else
        {
            print_debug ("Ctrl+C received again, Exiting process");
            exit (0);
        }
    }
    else
    {
        printf ("Ctrl+C received, stopping program\n");
        interrupted = 1;
    }
}

static CinterState *cinterplot_init (void)
{
    CinterState *cs = safe_calloc (1, sizeof (*cs));
    cs->on_mouse_pressed  = on_mouse_pressed;
    cs->on_mouse_released = on_mouse_released;
    cs->on_mouse_motion   = on_mouse_motion;
    cs->on_mouse_wheel    = on_mouse_wheel;
    cs->on_keyboard       = on_keyboard;
    cs->plot_data         = plot_data;

    cs->mouseEnabled      = 1;
    cs->trackingMode      = 0;
    cs->statuslineEnabled = 1;
    cs->gridEnabled       = 1;
    cs->zoomEnabled       = 0;
    cs->fullscreen        = 0;
    cs->continuousScroll  = 0;
    cs->redraw            = 0;
    cs->redrawing         = 0;
    cs->running           = 1;
    cs->bordered          = 0;
    cs->forceRefresh      = 0;
    cs->margin            = 10;
    cs->showHelp          = 0;

    cs->graphOrder        = 0;
    cs->frameCounter      = 0;
    cs->pressedModifiers  = 0;

    cs->bgShade           = 0.04f;
    cs->subWindows        = NULL;
    cs->windowWidth       = CINTERPLOT_INIT_WIDTH;
    cs->windowHeight      = CINTERPLOT_INIT_HEIGHT;

    cs->mouseState = MOUSE_STATE_NONE;

    signal (SIGINT, signal_handler);

    if (SDL_Init (SDL_INIT_VIDEO) < 0)
        exit_error ("SDL could not initialize! SDL Error: %s\n", SDL_GetError ());

    reinitialise_sdl_context (cs, 1);

    update_image (cs, cs->texture, 1);

    SDL_RenderCopy (cs->renderer, cs->texture, NULL, NULL);
    SDL_RenderPresent (cs->renderer);

    return cs;
}

int cinterplot_is_running (CinterState *cs) { return cs->running; }
void cinterplot_redraw_async(CinterState *cs) { cs->redraw=1; }
void cinterplot_continuous_scroll_enable(CinterState *cs)  { cs->continuousScroll=1; }
void cinterplot_continuous_scroll_disable(CinterState *cs) { cs->continuousScroll=0; }

static int cinterplot_run_until_quit (CinterState *cs)
{
    double fps = 30;
    double periodTime = 1.0 / fps;
    double lastFrameTsp = 0;

    while (cs->running && !interrupted)
    {
        cs->frameCounter++;
        SDL_Event event;
        while (SDL_PollEvent (& event))
        {
            switch(event.type)
            {
             case SDL_QUIT:
                 return 0;
             case SDL_MOUSEBUTTONDOWN:
                 cs->redraw |= cs->on_mouse_pressed (cs, event.button.x, event.button.y, event.button.button, event.button.clicks);
                 break;
             case SDL_MOUSEBUTTONUP:
                 cs->redraw |= cs->on_mouse_released (cs, event.button.x, event.button.y);
                 break;
             case SDL_MOUSEMOTION:
                 cs->redraw |= cs->on_mouse_motion (cs, event.motion.x, event.motion.y);
                 break;
             case SDL_MOUSEWHEEL:
                 cs->redraw |= cs->on_mouse_wheel (cs, event.wheel.preciseX, event.wheel.preciseY);
                 break;
             case SDL_KEYDOWN:
             case SDL_KEYUP:
                 {
                     int repeat = event.key.repeat;
                     int pressed = event.key.state == SDL_PRESSED;
                     int key = event.key.keysym.sym;
                     int mod = event.key.keysym.mod;

                     cs->redraw |= cs->on_keyboard (cs, key, mod, pressed, repeat);
                     break;
                 }
             case SDL_WINDOWEVENT:
                 {
                     switch (event.window.event)
                     {
                      case SDL_WINDOWEVENT_RESIZED:
                          {
                              int newWidth = event.window.data1;
                              int newHeight = event.window.data2;
                              cs->windowWidth  = (uint32_t) newWidth;
                              cs->windowHeight = (uint32_t) newHeight;
                              reinitialise_sdl_context (cs, 0);
                              cs->redraw = 1;
                              break;
                          }
                      default:
                          //print_debug ("event.window.event: %d", event.window.event);
                          break;
                     }
                     break;
                 }
             default:
                 break;
            }
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
        else
            usleep (100);
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
