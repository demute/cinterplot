#include "common.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <stdatomic.h>

#include "cinterplot.h"
#include "font.c"
#include "oklab.h"

extern const unsigned int font[256][8];
static int interrupted = 0;

static inline void wait_for_access (atomic_flag* accessFlag)
{
    while (atomic_flag_test_and_set(accessFlag))
        continue;
}

static inline void release_access (atomic_flag* accessFlag)
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

int autoscale (CinterState *cs)                    { cs->autoscale = 1;        return 1; }
int background (CinterState *cs, uint32_t bgColor) { cs->bgColor = bgColor;    return 1; }
int toggle_mouse (CinterState *cs)                 { cs->mouseEnabled ^= 1;    return 1; }
int toggle_fullscreen (CinterState *cs)            { cs->toggleFullscreen = 1; return 1; }
int quit (CinterState *cs)                         { cs->running = 0;          return 0; }
int force_refresh (CinterState *cs)                { cs->forceRefresh = 0;     return 0; }
int toggle_tracking (CinterState *cs)              { cs->trackingEnabled ^= 1; return 1; }
int toggle_paused (CinterState *cs)                { cs->paused ^= 1;          return 1; }

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


static void lineRGBA (uint32_t *pixels, uint32_t _w, uint32_t _h, uint32_t _x0, uint32_t _y0, uint32_t _x1, uint32_t _y1, uint32_t color)
{
    if (!pixels)
        return;

    int w  = (int) _w;
    int h  = (int) _w;
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

int graph_attach (CinterState *cs, CinterGraph *graph, uint32_t row, uint32_t col, char plotType, char *colorSpec)
{
    if (row >= cs->nRows || col >= cs->nCols)
    {
        print_error ("row/col %d/%d out of range", row, col);
        return -1;
    }

    GraphAttacher *attacher = safe_calloc (1, sizeof (*attacher));
    attacher->graph = graph;
    attacher->plotType = plotType;
    attacher->hist.w = 0;
    attacher->hist.h = 0;
    attacher->hist.bins = NULL;
    attacher->colorScheme = make_color_scheme (colorSpec, 16);
    attacher->lastGraphCounter = 0;

    SubWindow *sw = & cs->subWindows[row * cs->nCols + col];
    if (sw->numAttachedGraphs >= sw->maxNumAttachedGraphs)
    {
        print_error ("maximum number of attached graphs reached");
        return -1;
    }

    sw->attachedGraphs[sw->numAttachedGraphs++] = attacher;
    return 0;
}

CinterGraph *graph_new (uint32_t len, int doublePrecision)
{
    CinterGraph *graph = safe_calloc (1, sizeof (*graph));
    graph->len = len;
    graph->doublePrecision = doublePrecision;
    atomic_flag_clear (& graph->readAccess);
    atomic_flag_clear (& graph->insertAccess);

    size_t itemSize = doublePrecision ? 2 * sizeof (double) : 2 * sizeof (float);

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

void graph_add_point (CinterGraph *graph, double x, double y)
{
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

    if (graph->doublePrecision)
    {
        double xy[2] = {x,y};
        wait_for_access (& graph->insertAccess);
        stream_buffer_insert (sb, xy);
        release_access (& graph->insertAccess);
    }
    else
    {
        float xy[2] = {(float) x, (float) y};
        stream_buffer_insert (sb, xy);
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
             case 'r': force_refresh (cs); break;
             case 't': toggle_tracking (cs); break;
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
                           cs->bgColor = make_gray (shades[index]);
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

void make_histogram (Histogram *hist, CinterGraph *graph, char plotType)
{
    int *bins  = hist->bins;
    uint32_t w = hist->w;
    uint32_t h = hist->h;

    wait_for_access (& graph->readAccess);

    if (graph->doublePrecision)
    {
        double xmin = hist->xmin;
        double xmax = hist->xmax;
        double ymin = hist->ymin;
        double ymax = hist->ymax;

        double (*xys)[2];
        uint32_t len;
        wait_for_access (& graph->insertAccess);
        stream_buffer_get (graph->sb, & xys, & len);
        if (!len)
        {
            release_access (& graph->insertAccess);
            release_access (& graph->readAccess);
            return;
        }
        if (graph->len && graph->len < len)
        {
            xys += (len - graph->len);
            len = graph->len;
        }

        if (xmin == xmax)
        {
                xmin = xys[0][0];
                xmax = xys[len-1][0];
        }
        release_access (& graph->insertAccess);
        //if (xmin == xmax)
        //{
        //    xmin = DBL_MAX;
        //    xmax = -DBL_MAX;
        //    for (uint32_t i=0; i<len; i++)
        //    {
        //        double x = xys[i][0];
        //        if (xmin > x)
        //            xmin = x;
        //        if (xmax < x)
        //            xmax = x;
        //    }
        //}

        uint32_t nBins = w * h;
        for (uint32_t i=0; i<nBins; i++)
            bins[i] = 0;

        for (uint32_t i=0; i<len; i++)
        {
            double x = xys[i][0];
            double y = xys[i][1];

            int xi = (int) (w * (x - xmin) / (xmax - xmin));
            int yi = (int) (h * (y - ymin) / (ymax - ymin));
            if (xi >= 0 && xi < w && yi >= 0 && yi < h)
                bins[(uint32_t) yi*w + (uint32_t) xi]++;
        }
    }
    else
    {
        float xmin = (float) hist->xmin;
        float xmax = (float) hist->xmax;
        float ymin = (float) hist->ymin;
        float ymax = (float) hist->ymax;

        float (*xys)[2];
        uint32_t len;
        wait_for_access (& graph->insertAccess);
        stream_buffer_get (graph->sb, & xys, & len);
        if (!len)
        {
            release_access (& graph->insertAccess);
            release_access (& graph->readAccess);
            return;
        }
        if (graph->len && graph->len < len)
        {
            xys += (len - graph->len);
            len = graph->len;
        }

        if (xmin == xmax)
        {
                xmin = xys[0][0];
                xmax = xys[len-1][0];
        }
        release_access (& graph->insertAccess);
        //if (xmin == xmax)
        //{
        //    xmin = DBL_MAX;
        //    xmax = -DBL_MAX;
        //    for (uint32_t i=0; i<len; i++)
        //    {
        //        float x = xys[i][0];
        //        if (xmin > x)
        //            xmin = x;
        //        if (xmax < x)
        //            xmax = x;
        //    }
        //}

        uint32_t nBins = w * h;
        for (uint32_t i=0; i<nBins; i++)
            bins[i] = 0;

        for (uint32_t i=0; i<len; i++)
        {
            float x = xys[i][0];
            float y = xys[i][1];

            int xi = (int) (w * (x - xmin) / (xmax - xmin));
            int yi = (int) (h * (y - ymin) / (ymax - ymin));
            if (xi >= 0 && xi < w && yi >= 0 && yi < h)
                bins[(uint32_t) yi*w + (uint32_t) xi]++;
        }
    }

    release_access (& graph->readAccess);
}

static void plot_data (CinterState *cs, uint32_t *pixels)
{
    uint32_t w     = cs->windowWidth;
    uint32_t h     = cs->windowHeight;
    uint32_t nCols = cs->nCols;
    uint32_t nRows = cs->nRows;
    uint32_t forceRefresh = cs->forceRefresh;
    cs->forceRefresh = 0;

    uint32_t dy = h / nRows;
    uint32_t dx = w / nCols;

    uint32_t subHeight = dy - 2*cs->bordered - 2*cs->margin;
    uint32_t subWidth  = dx - 2*cs->bordered - 2*cs->margin;

    int mouseCol = cs->mouseX / (int) dx;
    int mouseRow = cs->mouseY / (int) dy;
    SubWindow *activeSw = NULL;
    if (mouseCol >= 0 && mouseCol < nCols && mouseRow >= 0 && mouseRow < nRows)
        activeSw = & cs->subWindows[(uint32_t) mouseRow * nCols + (uint32_t) mouseCol];


    uint32_t activeColor   = MAKE_COLOR (255,255,255);
    uint32_t inactiveColor = MAKE_COLOR (100,100,100);

    for (uint32_t sy=0; sy < nRows; sy++)
    {
        uint32_t yOffset = dy * sy;
        for (uint32_t sx=0; sx < nCols; sx++)
        {
            uint32_t xOffset = dx * sx;
            SubWindow *sw = & cs->subWindows[sy * nCols + sx];

            if (cs->bordered)
            {
                uint32_t x0 = xOffset + cs->margin;
                uint32_t y0 = yOffset + cs->margin;
                uint32_t x1 = xOffset + dx - 1 - cs->margin;
                uint32_t y1 = yOffset + dy - 1 - cs->margin;
                draw_rect (pixels, w, h, x0, y0, x1, y1, (sw == activeSw) ? activeColor : inactiveColor);
            }

            uint32_t x0 = xOffset + cs->margin + cs->bordered;
            uint32_t y0 = yOffset + cs->margin + cs->bordered;
            uint32_t bgColor = cs->bgColor;

            for (uint32_t y=0; y<subHeight; y++)
                for (uint32_t x=0; x<subWidth;  x++)
                    pixels[(y0+y)*w + (x0+x)] = bgColor;

            for (int i=0; i<sw->numAttachedGraphs; i++)
            {
                GraphAttacher *attacher = sw->attachedGraphs[i];
                Histogram *hist = & attacher->hist;

                int updateHistogram =
                    (forceRefresh) ||
                    (attacher->lastGraphCounter != attacher->graph->sb->counter) ||
                    (hist->xmin != sw->xmin) ||
                    (hist->xmax != sw->xmax) ||
                    (hist->ymin != sw->ymin) ||
                    (hist->ymax != sw->ymax);

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
                    hist->xmin = sw->xmin;
                    hist->xmax = sw->xmax;
                    hist->ymin = sw->ymin;
                    hist->ymax = sw->ymax;
                    make_histogram (hist, attacher->graph, attacher->plotType);
                }
                else
                {
                    //foobar;
                }

                int *bins = hist->bins;
                uint32_t *colors = attacher->colorScheme->colors;
                uint32_t nLevels = attacher->colorScheme->nLevels;
                for (uint32_t y=0; y<subHeight; y++)
                {
                    for (uint32_t x=0; x<subWidth;  x++)
                    {
                        int cnt = bins[y * subWidth + x];
                        if (cnt > 0)
                        {
                            uint32_t color = colors[MIN (nLevels, (uint32_t) cnt) - 1];
                            pixels[(y0+y)*w + (x0+x)] = color;
                        }
                    }
                }
            }
        }
    }
}

int make_sub_windows (CinterState *cs, uint32_t nRows, uint32_t nCols, uint32_t bordered, uint32_t margin)
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

    uint32_t n = nRows * nCols;
    cs->subWindows = safe_calloc (n, sizeof (cs->subWindows[0]));
    for (uint32_t i=0; i<n; i++)
    {
        SubWindow *sw = & cs->subWindows[i];
        sw->maxNumAttachedGraphs = MAX_NUM_ATTACHED_GRAPHS;
        sw->attachedGraphs = safe_calloc (sw->maxNumAttachedGraphs, sizeof (*sw->attachedGraphs));
        sw->numAttachedGraphs = 0;
        sw->xmin = 0;
        sw->xmax = 0;
        sw->ymin = -1;
        sw->ymax =  1;
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
                                   (int) cs->windowWidth, (int) cs->windowHeight, SDL_WINDOW_SHOWN);
    if (!cs->window)
        exit_error ("Window could not be created: SDL Error: %s\n", SDL_GetError ());

    cs->renderer = SDL_CreateRenderer (cs->window, -1, SDL_RENDERER_ACCELERATED);
    if (!cs->renderer)
        exit_error ("Renderer could not be created! SDL Error: %s\n", SDL_GetError ());

    cs->texture = SDL_CreateTexture (cs->renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
                                     (int) cs->windowWidth, (int) cs->windowHeight);
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
    cs->bgColor          = make_gray (0.04f);
    cs->nRows            = 0;
    cs->nCols            = 0;
    cs->bordered         = 0;
    cs->paused           = 0;
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
                assert (displayMode.w > 0);
                assert (displayMode.h > 0);
                cs->windowWidth  = (uint32_t) displayMode.w;
                cs->windowHeight = (uint32_t) displayMode.h;
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
