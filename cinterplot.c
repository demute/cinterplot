#include "common.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <stdatomic.h>

#include "cinterplot.h"
#include "font.c"
#include "oklab.h"

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

int autoscale (CinterState *cs)                    { cs->autoscale = 1;          return 1; }
int reset_scaling (CinterState *cs)                { cs->resetScaling = 1;       return 1; }
int background (CinterState *cs, float bgShade)    { cs->bgShade = bgShade;      return 1; }
int toggle_mouse (CinterState *cs)                 { cs->mouseEnabled ^= 1;      return 1; }
int toggle_statusline (CinterState *cs)            { cs->statuslineEnabled ^= 1; return 1; }
int toggle_fullscreen (CinterState *cs)            { cs->toggleFullscreen = 1;   return 1; }
int quit (CinterState *cs)                         { cs->running = 0;            return 0; }
int force_refresh (CinterState *cs)                { cs->forceRefresh = 0;       return 0; }
int toggle_tracking (CinterState *cs)              { cs->trackingEnabled ^= 1;   return 1; }
int toggle_paused (CinterState *cs)                { cs->paused ^= 1;            return 1; }

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

static void window_pos_to_data_pos (const Area *srcArea, const Position *srcPos, const Area *dstArea, Position *dstPos)
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
    if (cs->zoomEnabled)
    {
        dst->x0 = 0.0 + xp;
        dst->y0 = 0.0 + yp;
        dst->x1 = 1.0 - xp - epsw;
        dst->y1 = 1.0 - yp - epsh;
    }
    else
    {
        dst->x0 = src->x0 + xp;
        dst->y0 = src->y0 + yp;
        dst->x1 = src->x1 - xp - epsw;
        dst->y1 = src->y1 - yp - epsh;
    }
}


static int on_mouse_pressed (CinterState *cs, int xi, int yi, int button, int clicks)
{
    if (cs->activeSw == NULL)
    {
        cs->mouseState = MOUSE_STATE_NONE;
        return 0;
    }
    //SubWindow *sw = cs->activeSw;

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

    return 1;
}

static int on_mouse_released (CinterState *cs, int xi, int yi)
{
    switch (cs->mouseState)
    {
     case MOUSE_STATE_SELECTING:
         {
             Area *sel = & cs->activeSw->selectedWindowArea1;
             if (sel->x0 == sel->x1 && sel->y0 == sel->y1)
                 cs->zoomEnabled ^= 1;
             else
             {

                 SubWindow *sw = cs->activeSw;
                 Area *dr  = & sw->dataRange;
                 Area *swa = & sw->selectedWindowArea1;
                 Area activeArea;
                 get_active_area (cs, & sw->windowArea, & activeArea);
                 Position wPos0 = {swa->x0, swa->y0};
                 Position wPos1 = {swa->x1, swa->y1};
                 Position dPos0, dPos1;

                 window_pos_to_data_pos (& activeArea, & wPos0, & sw->dataRange, & dPos0);
                 window_pos_to_data_pos (& activeArea, & wPos1, & sw->dataRange, & dPos1);

                 dr->x0 = dPos0.x;
                 dr->y0 = dPos0.y;
                 dr->x1 = dPos1.x;
                 dr->y1 = dPos1.y;
                 //print ("zoom to new area [%f, %f] < [%f, %f] => ", dr->x0, dr->y0, dr->x1, dr->y1);
                 sel->x0 = sel->x1 = sel->y0 = sel->y1 = NaN;
             }
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
                 SubWindow *sw = cs->zoomEnabled ? cs->activeSw : & cs->subWindows[i];
                 get_active_area (cs, & sw->windowArea, & activeArea);

                 cs->mouseWindowPos.x = (double) xi / w;
                 cs->mouseWindowPos.y = (double) yi / h;
                 window_pos_to_data_pos (& activeArea, & cs->mouseWindowPos, & sw->dataRange, & sw->mouseDataPos);

                 if (activeArea.x0 <= cs->mouseWindowPos.x && cs->mouseWindowPos.x <= activeArea.x1 &&
                     activeArea.y0 <= cs->mouseWindowPos.y && cs->mouseWindowPos.y <= activeArea.y1)
                     cs->activeSw = sw;

                 if (cs->zoomEnabled)
                     break;
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
             dx /= (sw->windowArea.x1 - sw->windowArea.x0);
             dy /= (sw->windowArea.y1 - sw->windowArea.y0);
             dx *= dr->x1 - dr->x0;
             dy *= dr->y1 - dr->y0;

             printn ("moving window [%f, %f] < [%f, %f] => ", dr->x0, dr->y0, dr->x1, dr->y1);
             dr->x0 -= dx;
             dr->x1 -= dx;
             dr->y0 -= dy;
             dr->y1 -= dy;
             print ("[%f, %f] < [%f, %f] => ", dr->x0, dr->y0, dr->x1, dr->y1);

             break;
         }
     case MOUSE_STATE_SELECTING:
         {
             uint32_t w = cs->windowWidth;
             uint32_t h = cs->windowHeight - cs->statuslineEnabled * STATUSLINE_HEIGHT;
             cs->mouseWindowPos.x = (double) xi / w;
             cs->mouseWindowPos.y = (double) yi / h;
             Area *a0 = & cs->activeSw->selectedWindowArea0;
             Area *a1 = & cs->activeSw->selectedWindowArea1;
             a0->x1 = cs->mouseWindowPos.x;
             a0->y1 = cs->mouseWindowPos.y;
             if (a0->x0 < a0->x1)
             {
                 a1->x0 = a0->x0;
                 a1->x1 = a0->x1;
             }
             else
             {
                 a1->x0 = a0->x1;
                 a1->x1 = a0->x0;
             }
             if (a0->y0 < a0->y1)
             {
                 a1->y0 = a0->y0;
                 a1->y1 = a0->y1;
             }
             else
             {
                 a1->y0 = a0->y1;
                 a1->y1 = a0->y0;
             }

             double minDiffX = 10.0 / w;
             double minDiffY = 10.0 / h;

             double dx = a1->x1 - a1->x0;
             double dy = a1->y1 - a1->y0;

             if (dx >= minDiffX || dy >= minDiffY)
             {
                 Area *wa = & cs->activeSw->windowArea;
                 if (dx < minDiffX)
                 {
                     a1->x0 = wa->x0;
                     a1->x1 = wa->x1;
                 }
                 if (dy < minDiffY)
                 {
                     a1->y0 = wa->y0;
                     a1->y1 = wa->y1;
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
    if (pressed && !repeat)
    {
        if (mod == 0)
        {
            switch (key)
            {
             case 'a': autoscale (cs); break;
             case 'r': reset_scaling (cs); break;
             case 'f': toggle_fullscreen (cs); break;
             case 'm': toggle_mouse (cs); break;
             case 's': toggle_statusline (cs); break;
             case 'q': quit (cs); break;
             case 'e': force_refresh (cs); break;
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
                           cs->bgShade = shades[index];
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

int graph_attach (CinterState *cs, CinterGraph *graph, uint32_t windowIndex, char plotType, char *colorSpec)
{
    if (windowIndex >= cs->numSubWindows)
    {
        print_error ("windowIndex %d out of range", windowIndex);
        return -1;
    }

    GraphAttacher *attacher = safe_calloc (1, sizeof (*attacher));
    attacher->graph = graph;
    attacher->plotType = plotType;
    attacher->hist.w = 0;
    attacher->hist.h = 0;
    attacher->hist.bins = NULL;
    attacher->colorScheme = make_color_scheme (colorSpec, 8);
    attacher->lastGraphCounter = 0;

    SubWindow *sw = & cs->subWindows[windowIndex];
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

void make_histogram (Histogram *hist, CinterGraph *graph, char plotType)
{
    int *bins  = hist->bins;
    uint32_t w = hist->w;
    uint32_t h = hist->h;

    wait_for_access (& graph->readAccess);

    if (graph->doublePrecision)
    {
        double xmin = hist->dataRange.x0;
        double xmax = hist->dataRange.x1;
        double ymin = hist->dataRange.y0;
        double ymax = hist->dataRange.y1;

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

            int xi = (int) ((w-1) * (x - xmin) / (xmax - xmin));
            int yi = (int) ((h-1) * (y - ymin) / (ymax - ymin));
            if (xi >= 0 && xi < w && yi >= 0 && yi < h)
            {
                bins[(uint32_t) yi*w + (uint32_t) xi]++;
            }
        }
    }
    else
    {
        float xmin = (float) hist->dataRange.x0;
        float xmax = (float) hist->dataRange.x1;
        float ymin = (float) hist->dataRange.y0;
        float ymax = (float) hist->dataRange.y1;

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

uint32_t draw_text (uint32_t* pixels, uint32_t w, uint32_t h, uint32_t x0, uint32_t y0, uint32_t color, int transparent, char *message)
{
    if (!pixels)
        return 0;

    uint32_t scale = 2;
    uint32_t fh = 8*scale;
    uint32_t fw = 6*scale;

    char *p = message;
    uint32_t cols = 256;
    uint32_t margin = 1;

    //y0 -= fh + margin;
    //x0 -= fw + margin;

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
            y += fh + 4 * margin;
            if (wrap) continue;
        }
        else if (*p == '\t')
        {
            while (++i % 4)
                x += fw + margin;
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
                            pixels[(y+yi)*w + (x+xi)] = ~color;
            }
            x += fw + margin;
        }
        p++;
    }
    return y - y0;
}

static void plot_data (CinterState *cs, uint32_t *pixels)
{
    uint32_t activeColor    = make_gray (1.0f);
    uint32_t inactiveColor  = make_gray (0.4f);
    uint32_t crossHairColor = make_gray (0.6f);
    uint32_t bgColor        = make_gray (cs->bgShade);
    uint32_t selectColor    = make_gray (cs->bgShade + (cs->bgShade < 0.5f ? 0.2f : -0.2f));

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

            if (x0 >= w) exit_error ("bug");
            if (x1 >= w) exit_error ("bug");
            if (y0 >= h) exit_error ("bug");
            if (y1 >= h) exit_error ("bug");

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

        for (int i=0; i<sw->numAttachedGraphs; i++)
        {
            GraphAttacher *attacher = sw->attachedGraphs[i];
            Histogram *hist = & attacher->hist;

            int updateHistogram =
                (forceRefresh) ||
                (attacher->lastGraphCounter != attacher->graph->sb->counter) ||
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
                make_histogram (hist, attacher->graph, attacher->plotType);
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
                    else if (regionSelected)
                        *pixel = selectColor;

                    //else if (mousePressed && (cs->zoomWindow || (coli == mouseCol && rowi == mouseRow)))
                    //{
                    //    Mouse *r = & relMouse;
                    //    int x0 = r->x;
                    //    int x1 = r->pressX;
                    //    int y0 = r->y;
                    //    int y1 = r->pressY;


                    //    if (x1 - x0 > snap || y1 - y0 > snap)
                    //    {
                    //        if (x1 - x0 < snap)
                    //        {
                    //            x0 = 0;
                    //            x1 = (int) subWidth;
                    //        }
                    //        else if (y1 - y0 < snap)
                    //        {
                    //            y0 = 0;
                    //            y1 = (int) subHeight;
                    //        }
                    //    }

                    //    if (x0 <= xi && xi <= x1 && y0 <= yi && yi <= y1)
                    //        *pixel = selectColor;
                    //}
                }
            }
        }
    }
    if (cs->statuslineEnabled && cs->activeSw)
    {
        uint32_t textColor = make_gray (0.9f);
        int transparent = 1;
        uint32_t x0 = 10;
        uint32_t y0 = cs->windowHeight - STATUSLINE_HEIGHT;
        char message[256];
        double mx = cs->activeSw->mouseDataPos.x;
        double my = cs->activeSw->mouseDataPos.y;
        snprintf (message, sizeof (message), "(x,y) = (%f,%f)", mx, my);
        draw_text (pixels, cs->windowWidth, cs->windowHeight, x0, y0, textColor, transparent, message);
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

            sw->dataRange.x0 = -1;
            sw->dataRange.x1 =  1;
            sw->dataRange.y0 = -1;
            sw->dataRange.y1 =  1;

            sw->windowArea.x0 = (ci    ) * dx;
            sw->windowArea.x1 = (ci + 1) * dx;
            sw->windowArea.y0 = (ri    ) * dy;
            sw->windowArea.y1 = (ri + 1) * dy;
        }
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

    cs->autoscale         = 0;
    cs->resetScaling      = 0;
    cs->mouseEnabled      = 1;
    cs->trackingEnabled   = 0;
    cs->statuslineEnabled = 1;
    cs->zoomEnabled       = 0;
    cs->toggleFullscreen  = 0;
    cs->fullscreen        = 0;
    cs->redraw            = 0;
    cs->redrawing         = 0;
    cs->running           = 1;
    cs->bgShade           = 0.04f;
    cs->bordered          = 0;
    cs->paused            = 0;
    cs->margin            = 10;
    cs->frameCounter      = 0;
    cs->pressedModifiers  = 0;
    cs->subWindows        = NULL;
    cs->windowWidth       = CINTERPLOT_INIT_WIDTH;
    cs->windowHeight      = CINTERPLOT_INIT_HEIGHT;

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
                 cs->redraw |= cs->on_mouse_pressed (cs, sdlEvent.button.x, sdlEvent.button.y, sdlEvent.button.button, sdlEvent.button.clicks);
                 break;
             case SDL_MOUSEBUTTONUP:
                 cs->redraw |= cs->on_mouse_released (cs, sdlEvent.button.x, sdlEvent.button.y);
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
