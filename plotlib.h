#ifndef _PLOTLIB_H_
#define _PLOTLIB_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "common.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>


#define XINT(w,x) ((int)(round((x) * (w))))
#define YINT(h,y) ((int)(round((1.0-y) * (h))))
#define XTOFLOAT(w,xint) (((double) (xint)) / (w))
#define YTOFLOAT(h,yint) (1.0 - ((double) (yint)) / (h))
#define MAKE_COLOR(r,g,b) ((uint32_t) (((int)(r) << 16) | ((int)(g) << 8) | (int)(b)))
#define MAX_SUB_WINDOWS 4

struct plotlib_config_t
{
    int    windowWidth;
    int    windowHeight;
    int fullscreen;
    SDL_Window   *window;
    SDL_Renderer *renderer;
    SDL_Texture  *texture;
    int    frameCounter;
    void *userdata;

    int  (*on_mouse_pressed) (struct plotlib_config_t *config, int button);
    int  (*on_mouse_released) (struct plotlib_config_t *config);
    int  (*on_mouse_motion) (struct plotlib_config_t *config, int xi, int yi);
    int  (*on_keyboard) (struct plotlib_config_t *config, int c, int pressed, int repeat);
    void (*callback) (struct plotlib_config_t *config, uint32_t *pixels, int w, int h);
};
typedef struct plotlib_config_t PlotlibConfig;

struct points_t
{
    int nPoints;
    int maxPoints;
    double xmin;
    double xmax;
    double ymin;
    double ymax;
    double *xs;
    double *ys;
};
typedef struct points_t Points;

struct sub_window_t
{
    PlotlibConfig *config;
    uint32_t *pixels;
    int w;
    int h;

    int bordered;
    int margin;
    int subw;
    int subh;
    int xOffset;
    int yOffset;
};
typedef struct sub_window_t SubWindow;

int plotlib_main_loop (PlotlibConfig *config, int redraw);
void plotlib_init (PlotlibConfig *config);
void plotlib_cleanup (PlotlibConfig *config);
void lineRGBA (uint32_t *pixels, int w, int h, int x0, int y0, int x1, int y1, uint32_t color);
void lineRGBAf (uint32_t *pixels, int w, int h, double x0f, double y0f, double x1f, double y1f, uint32_t color);
void make_sub_windows (PlotlibConfig *config, uint32_t *pixels, int w, int h, int margin, int bordered, int nrows, int ncols);
Points *allocate_points (int maxPoints, double xmin, double xmax, double ymin, double ymax);
void draw_points (int subx, int suby, Points *points, char drawType, uint32_t color);
void put_point (Points *points, double x, double y);
void reset_points (Points *points);
void printRect (uint32_t* pixels, int w, int h, int x0, int y0, int x1, int y1, uint32_t color);
void printRectf (uint32_t* pixels, int w, int h, double x0f, double y0f, double x1f, double y1f, uint32_t color);
void draw_heatmap (int subx, int suby, int yoffs, float *heatmap, int width, int height, float scale);
void get_sub_properties (int subx, int suby, int *width, int *height, int *xoffs, int *yoffs);
void get_floating_mouse_pos (int subx, int suby, int mouseX, int mouseY, double *xf, double *yf);
int putTextf (uint32_t* pixels, int w, int h, double x0f, double y0f, uint32_t color, int transparent, char *message);
int putText (uint32_t* pixels, int w, int h, int x0, int y0, uint32_t color, int transparent, char *message);
void add_text (int subx, int suby, Points *points, double x, double y, char *text, uint32_t color);

#ifdef __cplusplus
} /* end extern C */
#endif

#endif /* _PLOTLIB_H_ */
