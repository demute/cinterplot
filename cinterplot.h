#ifndef _CINTERPLOT_H_
#define _CINTERPLOT_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "common.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>

#define CINTERPLOT_INIT_WIDTH 1080
#define CINTERPLOT_INIT_HEIGHT 600
#define CINTERPLOT_TITLE "Cinterplot"
#define MAKE_COLOR(r,g,b) ((uint32_t) (((int)(r) << 16) | ((int)(g) << 8) | (int)(b)))

typedef struct ColorScheme
{
    int nLevels;
    uint32_t *colors;
} ColorScheme;

typedef struct CinterGraph
{

} CinterGraph;

typedef struct Histogram
{
    int w;
    int h;
    int *count;
} Histogram;

typedef struct GraphAttacher
{
    CinterGraph *graph;
    ColorScheme  colorScheme;
    Histogram   *hist;
    char         plotType;
} GraphAttacher;

typedef struct SubWindow
{
    GraphAttacher **attachedGraphs;
    int maxNumAttachedGraphs;
    int numAttachedGraphs;

    float xmin;
    float xmax;
    float ymin;
    float ymax;

    int w;
    int h;
} SubWindow;

typedef struct CinterState
{
    uint32_t autoscale : 1;
    uint32_t mouseEnabled : 1;
    uint32_t trackingEnabled : 1;
    uint32_t toggleFullscreen : 1;
    uint32_t fullscreen : 1;
    uint32_t redraw : 1;
    uint32_t redrawing : 1;
    uint32_t running : 1;
    uint32_t bordered : 1;
    uint32_t margin : 8;

    uint32_t bgColor;
    int mouseX;
    int mouseY;

    int nRows;
    int nCols;

    SubWindow *subWindows;

    SDL_Window   *window;
    SDL_Renderer *renderer;
    SDL_Texture  *texture;

    int frameCounter;

    int  (*on_mouse_pressed)  (struct CinterState *cs, int button);
    int  (*on_mouse_released) (struct CinterState *cs);
    int  (*on_mouse_motion)   (struct CinterState *cs, int xi, int yi);
    int  (*on_keyboard)       (struct CinterState *cs, int c, int pressed, int repeat);
    void (*plot_data)         (struct CinterState *cs, uint32_t *pixels, int w, int h);

} CinterState;

int autoscale (CinterState *cs);
int background (CinterState *cs, uint32_t bgColor);
int toggle_mouse (CinterState *cs);
int toggle_fullscreen (CinterState *cs);
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
int make_sub_windows (CinterState *cs, int nRows, int nCols, int bordered, int margin);

#ifdef __cplusplus
} /* end extern C */
#endif

#endif /* _CINTERPLOT_H_ */

