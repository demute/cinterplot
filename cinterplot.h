#ifndef _CINTERPLOT_H_
#define _CINTERPLOT_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "common.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <stdatomic.h>
#include "stream_buffer.h"

#define INITIAL_VARIABLE_LENGTH 16384
#define MAX_VARIABLE_LENGTH     1048576
#define MAX_NUM_ATTACHED_GRAPHS 32
#define MAX_NUM_VERTICES        16
#define CINTERPLOT_INIT_WIDTH   1100
#define CINTERPLOT_INIT_HEIGHT  600
#define CINTERPLOT_TITLE "Cinterplot"
#define MAKE_COLOR(r,g,b) ((uint32_t) (((int)(r) << 16) | ((int)(g) << 8) | (int)(b)))

typedef struct ColorScheme
{
    uint32_t nLevels;
    uint32_t *colors;
} ColorScheme;

typedef struct CinterGraph
{
    int doublePrecision : 1;
    StreamBuffer *sb;
    uint32_t len;
    atomic_flag readAccess;
    atomic_flag insertAccess;

} CinterGraph;

typedef struct Area
{
    double x0;
    double y0;
    double x1;
    double y1;
} Area;

typedef struct Position
{
    double x;
    double y;
} Position;

typedef struct Histogram
{
    Area dataRange;

    uint32_t w;
    uint32_t h;
    int *bins;
} Histogram;

typedef struct GraphAttacher
{
    CinterGraph *graph;
    ColorScheme *colorScheme;
    Histogram    hist;
    uint64_t     lastGraphCounter;
    char         plotType;
} GraphAttacher;

typedef struct SubWindow
{
    GraphAttacher **attachedGraphs;
    uint32_t maxNumAttachedGraphs;
    uint32_t numAttachedGraphs;

    Position mouseDataPos;
    Area dataRange;
    Area windowArea;
    Area selectedWindowArea0;
    Area selectedWindowArea1;
} SubWindow;

#define KMOD_NONE  0
#define KMOD_SHIFT 1
#define KMOD_GUI   2
#define KMOD_ALT   4
#define KMOD_CTRL  8

typedef struct Mouse
{
    int x;
    int y;
    int lastX;
    int lastY;
    int pressX;
    int pressY;
    int releaseX;
    int releaseY;
    int clicks;
    int button;
} Mouse;

typedef struct CinterState
{
    uint32_t mouseEnabled : 1;
    uint32_t trackingEnabled : 1;
    uint32_t statuslineEnabled : 1;
    uint32_t zoomEnabled : 1;
    uint32_t fullscreen : 1;
    uint32_t redraw : 1;
    uint32_t redrawing : 1;
    uint32_t running : 1;
    uint32_t bordered : 1;
    uint32_t paused : 1;
    uint32_t forceRefresh : 1;
    uint32_t margin : 8;

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

    int frameCounter;
    int pressedModifiers;

    int  (*on_mouse_pressed)  (struct CinterState *cs, int xi, int yi, int button, int clicks);
    int  (*on_mouse_released) (struct CinterState *cs, int xi, int yi);
    int  (*on_mouse_motion)   (struct CinterState *cs, int xi, int yi);
    int  (*on_keyboard)       (struct CinterState *cs, int key, int mod, int pressed, int repeat);
    void (*plot_data)         (struct CinterState *cs, uint32_t *pixels);

} CinterState;

int autoscale (SubWindow *sw);
int toggle_mouse (CinterState *cs);
int set_fullscreen (CinterState *cs, uint32_t fullscreen);
int quit (CinterState *cs);
int toggle_tracking (CinterState *cs);
int move_left (CinterState *cs);
int move_right (CinterState *cs);
int move_up (CinterState *cs);
int move_down (CinterState *cs);
int expand_x (CinterState *cs);
int compress_x (CinterState *cs);
int expand_y (CinterState *cs);
int compress_y (CinterState *cs);
int make_sub_windows (CinterState *cs, uint32_t nRows, uint32_t nCols, uint32_t bordered, uint32_t margin);

CinterGraph *graph_new (uint32_t len, int doublePrecision);
void graph_add_point (CinterGraph *graph, double x, double y);
int graph_attach (CinterState *cs, CinterGraph *graph, uint32_t windowIndex, char plotType, char *colorSpec);

#ifdef __cplusplus
} /* end extern C */
#endif

#endif /* _CINTERPLOT_H_ */

