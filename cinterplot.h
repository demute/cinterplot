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

typedef struct CipColorScheme
{
    uint32_t nLevels;
    uint32_t *colors;
} CipColorScheme;

typedef struct CipGraph
{
    StreamBuffer *sb;
    uint32_t len;
    atomic_flag readAccess;
    atomic_flag insertAccess;

} CipGraph;

typedef struct CipArea
{
    double x0;
    double y0;
    double x1;
    double y1;
} CipArea;

typedef struct CipPosition
{
    double x;
    double y;
} CipPosition;

typedef struct CipHistogram
{
    CipArea dataRange;

    uint32_t w;
    uint32_t h;
    int *bins;
} CipHistogram;

typedef uint64_t (*HistogramFun) (CipHistogram *hist, CipGraph *graph, uint32_t logMode, char plotType);

typedef struct GraphAttacher
{
    CipGraph *graph;
    CipColorScheme *colorScheme;
    CipHistogram    hist;
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

    CipPosition mouseDataPos;
    CipArea dataRange;
    CipArea defaultDataRange;
    CipArea windowArea;
    CipArea selectedWindowArea0;
    CipArea selectedWindowArea1;
} SubWindow;

#define KMOD_NONE  0
#define KMOD_SHIFT 1
#define KMOD_GUI   2
#define KMOD_ALT   4
#define KMOD_CTRL  8

typedef struct CipMouse
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
} CipMouse;

typedef struct CipState CipState;

int  cip_autoscale (CipState *cs, uint32_t windowIndex);
int  cip_autoscale_sw (SubWindow *sw);
int  cip_set_crosshair_enabled (CipState *cs, uint32_t enabled);
void cip_update_color_scheme (CipState *cs, GraphAttacher *attacher, char *spec, uint32_t nLevels);
int  cip_set_fullscreen (CipState *cs, uint32_t fullscreen);
int  cip_zoom (SubWindow *sw, double xf, double yf);
int  cip_move (SubWindow *sw, double xf, double yf);
int  cip_set_tracking_mode (CipState *cs, uint32_t mode);
int  cip_make_sub_windows (CipState *cs, uint32_t nRows, uint32_t nCols, uint32_t bordered, uint32_t margin);
void cip_set_range (SubWindow *sw, double xmin, double ymin, double xmax, double ymax, int setAsDefault);
void cip_set_x_range (CipState *cs, uint32_t windowIndex, double xmin, double xmax, int setAsDefault);
void cip_set_y_range (CipState *cs, uint32_t windowIndex, double ymin, double ymax, int setAsDefault);
int  cip_set_grid_mode (CipState *cs, uint32_t mode);
int  cip_set_log_mode_sw (SubWindow *sw, uint32_t mode);
int  cip_set_log_mode (CipState *cs, uint32_t windowIndex, uint32_t mode);
int  cip_set_statusline_enabled (CipState *cs, uint32_t enabled);
void cip_histogram_line (CipHistogram *hist, int x0, int y0, int x1, int y1);
void cip_recursive_free_sub_windows (CipState *cs);
void cip_remove_attached_graphs (CipState *cs, uint32_t wi);
int  cip_force_refresh (CipState *cs);

CipGraph *cip_graph_new (uint32_t len);
void cip_graph_delete (CipGraph *graph);
void cip_graph_add_point (CipGraph *graph, double x, double y);
GraphAttacher *cip_graph_attach (CipState *cs, CipGraph *graph, uint32_t windowIndex, HistogramFun histogramFun, char plotType, char *colorSpec, uint32_t numColors);
int  cip_graph_detach (CipState *cs, CipGraph *graph, uint32_t windowIndex);
void cip_graph_remove_points (CipGraph *graph);

int  cip_is_running (CipState *cs);
int  cip_quit (CipState *cs);
void cip_redraw_async (CipState *cs);
void cip_continuous_scroll_enable  (CipState *cs, uint32_t windowIndex);
void cip_continuous_scroll_disable (CipState *cs, uint32_t windowIndex);
SubWindow *cip_get_sub_window (CipState *cs, uint32_t windowIndex);
void cip_set_bg_shade (CipState *cs, float bgShade);
void cip_set_sub_window_title (CipState *cs, uint32_t windowIndex, char *title);
int  cip_toggle_paused (CipState *cs);
void cip_save_png (CipState* cs, char* imageDir, int frameCounter, int format);

void cip_set_app_keyboard_callback (CipState *cs, int (*app_on_keyboard) (CipState *cs, int key, int mod, int pressed, int repeat));
void cip_set_app_mouse_motion (CipState *cs, int (*app_on_mouse_motion) (CipState *cs, int windowIndex, double x, double y));

//void wait_for_access (atomic_flag* accessFlag);
//void release_access (atomic_flag* accessFlag);

#ifdef __cplusplus
} /* end extern C */
#endif

#endif /* _CINTERPLOT_H_ */

