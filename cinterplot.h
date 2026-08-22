#ifndef _CINTERPLOT_H_
#define _CINTERPLOT_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdatomic.h>
#include <SDL2/SDL.h>
#include "stream_buffer.h"
#include "world_transform.h"

#define INITIAL_VARIABLE_LENGTH 16384
#define MAX_VARIABLE_LENGTH     16777216
#define MAX_NUM_ATTACHED_GRAPHS 4096
#define MAX_NUM_VERTICES        16
#define CINTERPLOT_INIT_WIDTH   1000
#define CINTERPLOT_INIT_HEIGHT  1000
#define CINTERPLOT_TITLE "Cinterplot"
#define MAKE_COLOR(r,g,b) (0xff000000 | (uint32_t) (((int)(r) << 16) | ((int)(g) << 8) | (int)(b)))

#define LOG101_VALUE 0.0099503308531681
#define LOG101_VALUE_INV (1.0 / LOG101_VALUE)
#define log101(x) (log (x) * LOG101_VALUE_INV)
#define exp101(x) exp ((x) * LOG101_VALUE)

#define LOGFUN log101
#define EXPFUN exp101


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
    char *name;
} CipGraph;

typedef struct CipArea
{
    int x0;
    int y0;
    int x1;
    int y1;
} CipArea;

typedef struct CipPosition
{
    int x;
    int y;
    int z;
} CipPosition;

typedef struct CipCanvas
{
    WorldTransform world;
    int    w;
    int    h;
    int    *bins;
    double *counts;
    double *sums;
    double *wz;
} CipCanvas;

typedef void (*CanvasFun) (WorldTransform *world, void *buf, size_t len, int firstUnusedIndex, uint32_t logMode, CipCanvas *canvas);

typedef struct GraphAttacher
{
    CipGraph       *graph;
    CipColorScheme *colorScheme;
    CipCanvas      canvas;
    uint64_t       lastGraphCounter;
    char           plotType;
    char           lastPlotType;
} GraphAttacher;

typedef struct CipSubWindow
{
    char *title;

    GraphAttacher **attachedGraphs;
    uint32_t maxNumAttachedGraphs;
    uint32_t numAttachedGraphs;
    uint32_t continuousScroll : 1;
    uint32_t logMode : 2;
    uint32_t gridMode : 2;
    uint32_t selectedGraph;

    WorldTransform world;
    CipArea windowArea;
    CipArea selectedArea;
    CipArea selectedAreaRaw;
} CipSubWindow;

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

int  cip_autoscale (CipState *cs, uint32_t windowIndex, double margin);
int  cip_autoscale_sw (CipSubWindow *sw, double margin);
int  cip_set_crosshair_enabled (CipState *cs, uint32_t enabled);
void cip_update_color_scheme (CipState *cs, GraphAttacher *attacher, char *spec, uint32_t nLevels);
int  cip_set_fullscreen (CipState *cs, uint32_t fullscreen);
int  cip_zoom (CipSubWindow *sw, double xf, double yf, double zf);
int  cip_move (CipSubWindow *sw, double xf, double yf);
int  cip_set_tracking_mode (CipState *cs, uint32_t mode);
int  cip_make_sub_windows (CipState *cs, uint32_t nRows, uint32_t nCols, uint32_t bordered, uint32_t margin);
void cip_set_range (CipSubWindow *sw, double xmin, double ymin, double xmax, double ymax, int setAsDefault);
void cip_set_x_range (CipState *cs, uint32_t windowIndex, double xmin, double xmax, int setAsDefault);
void cip_set_y_range (CipState *cs, uint32_t windowIndex, double ymin, double ymax, int setAsDefault);
int  cip_set_grid_mode (CipState *cs, uint32_t windowIndex, uint32_t mode);
int  cip_set_grid_mode_sw (CipSubWindow *sw, uint32_t mode);
int  cip_set_log_mode_sw (CipState *cs, CipSubWindow *sw, uint32_t mode);
int  cip_set_log_mode (CipState *cs, uint32_t windowIndex, uint32_t mode);
int  cip_set_statusline_enabled (CipState *cs, uint32_t enabled);
void cip_recursive_free_sub_windows (CipState *cs);
void cip_remove_attached_graphs (CipState *cs, uint32_t wi);
int  cip_force_refresh (CipState *cs);
void cip_graph_set_name (CipGraph *graph, char *name);

CipGraph *cip_graph_new (int dim, uint32_t len);
void cip_graph_delete (CipGraph *graph);
void cip_graph_add_1d_point (CipGraph *graph, double x);
void cip_graph_add_2d_point (CipGraph *graph, double x, double y);
void cip_graph_add_3d_point (CipGraph *graph, double x, double y, double z);
void cip_graph_add_4d_point (CipGraph *graph, double x, double y, double z, double u);
GraphAttacher *cip_graph_attach (CipState *cs, CipGraph *graph, uint32_t windowIndex, char plotType, char *colorSpec, uint32_t numColors);
int  cip_graph_detach (CipState *cs, CipGraph *graph, uint32_t windowIndex);
void cip_graph_remove_points (CipGraph *graph);
CipColorScheme *cip_make_color_scheme (char *spec, uint32_t nLevels);
void cip_delete_color_scheme (CipColorScheme *scheme);

int  cip_is_running (CipState *cs);
int  cip_quit (CipState *cs);
void cip_redraw_async (CipState *cs);
void cip_continuous_scroll_enable  (CipState *cs, uint32_t windowIndex);
void cip_continuous_scroll_disable (CipState *cs, uint32_t windowIndex);
CipSubWindow *cip_get_sub_window (CipState *cs, uint32_t windowIndex);
void cip_set_bg_shade (CipState *cs, float bgShade);
void cip_set_sub_window_title (CipState *cs, uint32_t windowIndex, char *title);
int  cip_toggle_paused (CipState *cs);
void cip_save_png (CipState* cs, char* imageDir, int frameCounter, int format);

void cip_set_app_keyboard_callback (CipState *cs, int (*app_on_keyboard) (CipState *cs, int key, int mod, int pressed, int repeat));
void cip_set_app_mouse_motion (CipState *cs, int (*app_on_mouse_motion) (CipState *cs, int windowIndex, double x, double y));
void cip_register_canvas_fun (int dim, char plotType, CanvasFun canvasFun);

//void wait_for_access (atomic_flag* accessFlag);
//void release_access (atomic_flag* accessFlag);

#ifdef __cplusplus
} /* end extern C */
#endif

#endif /* _CINTERPLOT_H_ */

