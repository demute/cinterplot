#ifndef _CINTERPLOT_H_
#define _CINTERPLOT_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdatomic.h>
#include <SDL2/SDL.h>
#include "stream_buffer.h"

#define INITIAL_VARIABLE_LENGTH 16384
#define MAX_VARIABLE_LENGTH     16777216
#define MAX_NUM_ATTACHED_GRAPHS 4096
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

typedef uint64_t (*HistogramFun) (Histogram *hist, CinterGraph *graph, uint32_t logMode, char plotType);

typedef struct GraphAttacher
{
    CinterGraph *graph;
    ColorScheme *colorScheme;
    Histogram    hist;
    uint64_t     lastGraphCounter;
    char         plotType;
    char         lastPlotType;
    HistogramFun histogramFun;
} GraphAttacher;

typedef struct SubWindow
{
    char *title;

    GraphAttacher **attachedGraphs;
    uint32_t maxNumAttachedGraphs;
    uint32_t numAttachedGraphs;
    uint32_t continuousScroll : 1;
    uint32_t logMode : 2;
    uint32_t selectedGraph;

    Position mouseDataPos;
    Area dataRange;
    Area defaultDataRange;
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

typedef struct CinterState CinterState;

int autoscale (SubWindow *sw);
int toggle_mouse (CinterState *cs);
void update_color_scheme (CinterState *cs, GraphAttacher *attacher, char *spec, uint32_t nLevels);
int set_fullscreen (CinterState *cs, uint32_t fullscreen);
int quit (CinterState *cs);
int zoom (SubWindow *sw, double xf, double yf);
int move (SubWindow *sw, double xf, double yf);
int set_tracking_mode (CinterState *cs, uint32_t mode);
int make_sub_windows (CinterState *cs, uint32_t nRows, uint32_t nCols, uint32_t bordered, uint32_t margin);
void set_range (SubWindow *sw, double xmin, double ymin, double xmax, double ymax, int setAsDefault);
void set_x_range (SubWindow *sw, double xmin, double xmax, int setAsDefault);
void set_y_range (SubWindow *sw, double ymin, double ymax, int setAsDefault);
int set_grid_mode (CinterState *cs, uint32_t mode);
int set_log_mode (SubWindow *sw, uint32_t mode);
int set_crosshair_enabled (CinterState *cs, uint32_t enabled);
int set_statusline_enabled (CinterState *cs, uint32_t enabled);
void wait_for_access (atomic_flag* accessFlag);
void release_access (atomic_flag* accessFlag);
void histogram_line (Histogram *hist, int x0, int y0, int x1, int y1);
void cinterplot_recursive_free_sub_windows (CinterState *cs);
void cinterplot_remove_attached_graphs (CinterState *cs, uint32_t wi);
int force_refresh (CinterState *cs);

CinterGraph *graph_new (uint32_t len);
void graph_delete (CinterGraph *graph);
void graph_add_point (CinterGraph *graph, double x, double y);
GraphAttacher *graph_attach (CinterState *cs, CinterGraph *graph, uint32_t windowIndex, HistogramFun histogramFun, char plotType, char *colorSpec, uint32_t numColors);
void graph_remove_points (CinterGraph *graph);

int  cinterplot_is_running (CinterState *cs);
void cinterplot_quit (CinterState *cs);
void cinterplot_redraw_async (CinterState *cs);
void cinterplot_continuous_scroll_enable (SubWindow *sw);
void cinterplot_continuous_scroll_disable (SubWindow *sw);
void cinterplot_set_bg_shade (CinterState *cs, float bgShade);
SubWindow *get_sub_window (CinterState *cs, uint32_t windowIndex);
void cinterplot_set_bg_shade (CinterState *cs, float bgShade);
void set_sub_window_title (CinterState *cs, uint32_t windowIndex, char *title);
int toggle_paused (CinterState *cs);
SDL_Surface *createSurfaceFromImage (char *file);

void cinterplot_set_app_keyboard_callback (CinterState *cs, int (*app_on_keyboard) (CinterState *cs, int key, int mod, int pressed, int repeat));
void cinterplot_set_app_mouse_motion (CinterState *cs, int (*app_on_mouse_motion) (CinterState *cs, int windowIndex, double x, double y));
#ifdef __cplusplus
} /* end extern C */
#endif

#endif /* _CINTERPLOT_H_ */

