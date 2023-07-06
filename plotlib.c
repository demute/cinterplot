#include "common.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>

#include "plotlib.h"
#include "font.c"
extern const unsigned int font[256][8];

SubWindow subWindows[MAX_SUB_WINDOWS][MAX_SUB_WINDOWS];

void make_sub_windows (PlotlibConfig *config, uint32_t *pixels, int w, int h, int margin, int bordered, int nrows, int ncols)
{
    if (!config || !pixels || nrows <= 0 || ncols <= 0 || nrows > MAX_SUB_WINDOWS || ncols > MAX_SUB_WINDOWS)
        exit_error ("invalid arguments passed: config:%p pixels:%p w:%d, h:%d margin:%d bordered:%d nrows:%d ncols:%d",
                    (void*) config, (void*) pixels, w, h, margin, bordered, nrows, ncols);

    int fontSpace = 12*(w <= 1000 ? 1 : 2);

    bzero (subWindows, sizeof (subWindows));
    int width = w / ncols;
    int height = (h-fontSpace) / nrows;
    int yOffset = 0;
    for (int row=0; row<nrows; row++)
    {
        int xOffset = 0;
        for (int col=0; col<ncols; col++)
        {
            subWindows[row][col].config   = config;
            subWindows[row][col].pixels   = pixels;
            subWindows[row][col].w        = w;
            subWindows[row][col].h        = h;
            subWindows[row][col].bordered = bordered;
            subWindows[row][col].margin   = margin;
            subWindows[row][col].xOffset  = xOffset;
            subWindows[row][col].yOffset  = yOffset;
            subWindows[row][col].subw     = width;
            subWindows[row][col].subh     = height;
            xOffset += width;
        }
        yOffset += height;
    }
}

Points *allocate_points (int maxPoints, double xmin, double xmax, double ymin, double ymax)
{
    if (maxPoints <= 0)
        exit_error ("maxPoints <= 0");
    Points *points = (Points*) malloc (sizeof (Points));
    assert (points);
    points->maxPoints = maxPoints;
    points->nPoints = 0;
    points->xmin = xmin;
    points->xmax = xmax;
    points->ymin = ymin;
    points->ymax = ymax;
    points->xs = (double*) malloc ((size_t) maxPoints * sizeof (double));
    points->ys = (double*) malloc ((size_t) maxPoints * sizeof (double));
    assert (points->xs);
    assert (points->ys);
    return points;
}

#define TRUNCATE(x,min,max) (((x) < (min)) ? (min) : ((x) > (max)) ? (max) : (x))
uint32_t fire_palette (float x)
{
    //static int foo = 0;
    //if (++foo % 4000 == 0)
    //    print_debug ("x:%g", x);
    if (isnan (x))
        return MAKE_COLOR (50,50,50);

    int r,g,b;

    if (x < 0)
        x = 0;
    if (x > 1)
        x = 1;

    if (x < 0.5)
    {
        double minr = 0;
        double ming = 0;
        double minb = 0;

        double maxr = 255;
        double maxg = 0;
        double maxb = 0;

        double s = x * 2;
        r = (int) (minr + s * (maxr - minr));
        g = (int) (ming + s * (maxg - ming));
        b = (int) (minb + s * (maxb - minb));
    }
    else
    {
        double minr = 255;
        double ming = 0;
        double minb = 0;

        double maxr = 255;
        double maxg = 255;
        double maxb = 255;

        double s = (x - 0.5) * 2;
        r = (int) (minr + s * (maxr - minr));
        g = (int) (ming + s * (maxg - ming));
        b = (int) (minb + s * (maxb - minb));
    }

    r = TRUNCATE (r, 0, 255);
    g = TRUNCATE (g, 0, 255);
    b = TRUNCATE (b, 0, 255);

    return MAKE_COLOR (r,g,b);
}

void draw_heatmap (int subx, int suby, int yoffs, float *heatmap, int hw, int hh, float scale)
{
    SubWindow *sub = & subWindows[suby][subx];
    assert (sub);

    uint32_t *pixels = sub->pixels;
    int w = sub->w;
    int h = sub->h;

    if (sub->bordered)
    {
        int x0 = sub->xOffset + sub->margin;
        int y0 = sub->yOffset + sub->margin;
        int x1 = x0 + sub->subw - 2 * sub->margin;
        int y1 = y0 + sub->subh - 2 * sub->margin;
        uint32_t color;
        //if (x0 < mouseX && mouseX < x1 && y0 < mouseY && mouseY < y1)
        //    color = MAKE_COLOR (255, 0, 0);
        //else
            color = MAKE_COLOR (100, 100, 100);
        lineRGBA (pixels, w, h, x0, y0, x1, y0, color);
        lineRGBA (pixels, w, h, x1, y0, x1, y1, color);
        lineRGBA (pixels, w, h, x1, y1, x0, y1, color);
        lineRGBA (pixels, w, h, x0, y1, x0, y0, color);
    }

    int width  = sub->subw - sub->margin * 2 - 2;
    int height = sub->subh - sub->margin * 2 - 2;

    for (int yi=0; yi<height; yi++)
    {
        for (int xi=0; xi<width; xi++)
        {
            int x = sub->xOffset + sub->margin + 1 + xi;
            int y = sub->yOffset + sub->margin + 1 + yi;

            int hxi0 = (int) ((float) hw * ((float) (xi  ) / (float) width));
            int hxi1 = (int) ((float) hw * ((float) (xi+1) / (float) width));
            int hyi  = (int) ((float) hh * ((float) (yi  ) / (float) height));
            hyi = (hyi + yoffs) % hh;

            //static int foo = 0;
            //if (++foo % 4000 == 0)
            //    print_debug ("[%d,%d] ==> heatmap[%d,%d] --> [%d,%d]", yi, xi, hyi, hxi, y, x);


            //if (xi == 10 && yi == 0)
            //    print_debug ("val:%f", heatmap[hh * hyi + hxi]);
            float sum = 0;
            float n = 0;
            for (int hxi=hxi0; hxi<=hxi1 && hxi<hw; hxi++)
            {
                sum += heatmap[hw * hyi + hxi];
                n += 1;
            }

            if (x>=0 && y>=0 && x<w && y<h)
                pixels[y * w + x] = fire_palette (sum * scale / n);
        }
    }
}

void draw_points (int subx, int suby, Points *points, char drawType, uint32_t color)
{
    SubWindow *sub = & subWindows[suby][subx];
    assert (sub);
    if (sub->bordered)
    {
        int x0 = sub->xOffset + sub->margin;
        int y0 = sub->yOffset + sub->margin;
        int x1 = x0 + sub->subw - 2 * sub->margin;
        int y1 = y0 + sub->subh - 2 * sub->margin;
        uint32_t color;
        //if (x0 < mouseX && mouseX < x1 && y0 < mouseY && mouseY < y1)
        //    color = MAKE_COLOR (255, 0, 0);
        //else
            color = MAKE_COLOR (100, 100, 100);
        uint32_t *pixels = sub->pixels;
        int w = sub->w;
        int h = sub->h;
        lineRGBA (pixels, w, h, x0, y0, x1, y0, color);
        lineRGBA (pixels, w, h, x1, y0, x1, y1, color);
        lineRGBA (pixels, w, h, x1, y1, x0, y1, color);
        lineRGBA (pixels, w, h, x0, y1, x0, y0, color);
    }

    double xmin = points->xmin;
    double xmax = points->xmax;
    double ymin = points->ymin;
    double ymax = points->ymax;

    if (xmin == xmax)
    {
        xmin =  DBL_MAX;
        xmax = -DBL_MAX;
        for (int i=0; i<points->nPoints; i++)
        {
            if (isnan (points->xs[i])) continue;
            if (points->xs[i] < xmin) xmin = points->xs[i];
            if (points->xs[i] > xmax) xmax = points->xs[i];
        }
        if (xmin == xmax)
        {
            xmin -= 0.5;
            xmax += 0.5;
        }
    }

    if (ymin == ymax)
    {
        ymin =  DBL_MAX;
        ymax = -DBL_MAX;
        for (int i=0; i<points->nPoints; i++)
        {
            if (isnan (points->ys[i])) continue;
            if (points->ys[i] < ymin) ymin = points->ys[i];
            if (points->ys[i] > ymax) ymax = points->ys[i];
        }
        if (ymin == ymax)
        {
            ymin -= 0.5;
            ymax += 0.5;
        }
    }

    double invDiffX = 1.0 / (xmax - xmin);
    double invDiffY = 1.0 / (ymax - ymin);
    int lastX=0, lastY=0;
    int width  = sub->subw - sub->margin * 2;
    int height = sub->subh - sub->margin * 2;
    int wasNan = 0;
    for (int i=0; i<points->nPoints; i++)
    {
        double x = points->xs[i];
        double y = points->ys[i];

        if (isnan (x) || isnan (y))
        {
            wasNan = 1;
            continue; // FIXME: make a jump here
        }
        double xf = (x - xmin) * invDiffX;
        double yf = (y - ymin) * invDiffY;

        int xi = sub->xOffset + sub->margin + (int) (xf * width);
        int yi = sub->yOffset + sub->subh - sub->margin - (int) (yf * height);

        if (drawType == 'h')
        {
            lastX = xi;
            lastY = sub->yOffset + sub->subh - 1 - sub->bordered * (sub->margin);
        }

        int drawEverything = 0;
        int drawThis = (drawEverything || (lastX >= 0 && lastY >= 0 && lastX < sub->w && lastY < sub->h && xi >= 0 && yi >= 0 && xi < sub->w && yi < sub->h));
        if (drawThis && i && !wasNan)
        {
            lineRGBA (sub->pixels, sub->w, sub->h, lastX, lastY, xi, yi, color);
            if (drawType == 't')
            {
                lineRGBA (sub->pixels, sub->w, sub->h, lastX, lastY-1, xi, yi-1, color);
                lineRGBA (sub->pixels, sub->w, sub->h, lastX, lastY+1, xi, yi+1, color);
                lineRGBA (sub->pixels, sub->w, sub->h, lastX-1, lastY, xi-1, yi, color);
                lineRGBA (sub->pixels, sub->w, sub->h, lastX+1, lastY, xi+1, yi, color);
            }
            //print_debug ("(%f %f) [%f,%f] [%f,%f] (mar:%d subw:%d subh:%d xoffs:%d yoffs:%d) => (%d,%d)", x, y, xmin, xmax, ymin, ymax, sub->margin, sub->subw, sub->subh, sub->xOffset, sub->yOffset, xi, yi);
        }
        wasNan = 0;
        lastX = xi;
        lastY = yi;
    }
}

void add_text (int subx, int suby, Points *points, double x, double y, char *text, uint32_t color)
{
    SubWindow *sub = & subWindows[suby][subx];
    assert (sub);

    double xmin = points->xmin;
    double xmax = points->xmax;
    double ymin = points->ymin;
    double ymax = points->ymax;

    if (xmin == xmax)
    {
        xmin =  DBL_MAX;
        xmax = -DBL_MAX;
        for (int i=0; i<points->nPoints; i++)
        {
            if (isnan (points->xs[i])) continue;
            if (points->xs[i] < xmin) xmin = points->xs[i];
            if (points->xs[i] > xmax) xmax = points->xs[i];
        }
        if (xmin == xmax)
        {
            xmin -= 0.5;
            xmax += 0.5;
        }
    }

    if (ymin == ymax)
    {
        ymin =  DBL_MAX;
        ymax = -DBL_MAX;
        for (int i=0; i<points->nPoints; i++)
        {
            if (isnan (points->ys[i])) continue;
            if (points->ys[i] < ymin) ymin = points->ys[i];
            if (points->ys[i] > ymax) ymax = points->ys[i];
        }
        if (ymin == ymax)
        {
            ymin -= 0.5;
            ymax += 0.5;
        }
    }

    double invDiffX = 1.0 / (xmax - xmin);
    double invDiffY = 1.0 / (ymax - ymin);
    int width  = sub->subw - sub->margin * 2;
    int height = sub->subh - sub->margin * 2;

    if (isnan (x) || isnan (y))
        return;
    double xf = (x - xmin) * invDiffX;
    double yf = (y - ymin) * invDiffY;

    int xi = sub->xOffset + sub->margin + (int) (xf * width);
    int yi = sub->yOffset + sub->subh - sub->margin - (int) (yf * height);

    putText (sub->pixels, sub->w, sub->h, xi, yi, color, 0, text);
}

void get_sub_properties (int subx, int suby, int *width, int *height, int *xoffs, int *yoffs)
{
    SubWindow *sub = & subWindows[suby][subx];
    assert (sub);

    *width = sub->subw - sub->margin * 2;
    *height = sub->subh - sub->margin * 2;
    *xoffs = sub->xOffset + sub->margin;
    *yoffs = sub->yOffset + sub->margin;
}

void get_floating_mouse_pos (int subx, int suby, int mouseX, int mouseY, double *xf, double *yf)
{
    int width, height, xoffs, yoffs;
    get_sub_properties (subx, suby, & width, & height, & xoffs, & yoffs);

    *xf = (double) (mouseX - xoffs) / width;
    *yf = (double) (mouseY - yoffs) / height;
}

void put_point (Points *points, double x, double y)
{
    if (points->nPoints >= points->maxPoints)
        exit_error ("buffer overflow %d>=%d", points->nPoints, points->maxPoints);

    points->xs[points->nPoints] = x;
    points->ys[points->nPoints] = y;
    points->nPoints++;
}

void reset_points (Points *points)
{
    points->nPoints = 0;
}

int putTextf (uint32_t* pixels, int w, int h, double x0f, double y0f, uint32_t color, int transparent, char *message)
{
    int x0 = XINT (w, x0f);
    int y0 = YINT (h, y0f);
    return putText (pixels, w, h, x0, y0, color, transparent, message);
}

int putText (uint32_t* pixels, int w, int h, int x0, int y0, uint32_t color, int transparent, char *message)
{
    if (!pixels)
        return 0;

    int scale = w <= 1000 ? 1 : 2;
    int fh = 8*scale;
    int fw = 6*scale;

    char *p = message;
    int cols = 256;
    int margin = 1;

    y0 -= fh + margin;
    x0 -= (fw + margin) * ((int)strlen (message) / 2 - 1);

    int x = x0;
    int y = y0;

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
                for (int yi=0; yi<fh; yi++)
                    for (int xi=0; xi<fw; xi++)
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

void lineRGBA (uint32_t *pixels, int w, int h, int x0, int y0, int x1, int y1, uint32_t color)
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

void lineRGBAf (uint32_t* pixels, int w, int h, double x0f, double y0f, double x1f, double y1f, uint32_t color)
{
    int x0 = XINT (w, x0f);
    int y0 = YINT (h, y0f);
    int x1 = XINT (w, x1f);
    int y1 = YINT (h, y1f);
    lineRGBA (pixels, w, h, x0, y0, x1, y1, color);
}

void printRectf (uint32_t* pixels, int w, int h, double x0f, double y0f, double x1f, double y1f, uint32_t color)
{
    int x0 = XINT (w, x0f);
    int y0 = YINT (h, y0f);
    int x1 = XINT (w, x1f);
    int y1 = YINT (h, y1f);
    printRect (pixels, w, h, x0, y0, x1, y1, color);
}

void printRect (uint32_t* pixels, int w, int h, int x0, int y0, int x1, int y1, uint32_t color)
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
        for (int xi=x0; xi<=x1 && xi<w; xi++)
            pixels[yi*w + xi]=color;
}


#define CASE_RETURN_STRING(x) case x : return # x
char *sdl_pixelformat_to_string (uint32_t value)
{
    switch (value)
    {
        CASE_RETURN_STRING (SDL_PIXELFORMAT_UNKNOWN);
        CASE_RETURN_STRING (SDL_PIXELFORMAT_INDEX1LSB);
        CASE_RETURN_STRING (SDL_PIXELFORMAT_INDEX1MSB);
        CASE_RETURN_STRING (SDL_PIXELFORMAT_INDEX4LSB);
        CASE_RETURN_STRING (SDL_PIXELFORMAT_INDEX4MSB);
        CASE_RETURN_STRING (SDL_PIXELFORMAT_INDEX8);
        CASE_RETURN_STRING (SDL_PIXELFORMAT_RGB332);
        CASE_RETURN_STRING (SDL_PIXELFORMAT_RGB444);
        CASE_RETURN_STRING (SDL_PIXELFORMAT_RGB555);
        CASE_RETURN_STRING (SDL_PIXELFORMAT_BGR555);
        CASE_RETURN_STRING (SDL_PIXELFORMAT_ARGB4444);
        CASE_RETURN_STRING (SDL_PIXELFORMAT_RGBA4444);
        CASE_RETURN_STRING (SDL_PIXELFORMAT_ABGR4444);
        CASE_RETURN_STRING (SDL_PIXELFORMAT_BGRA4444);
        CASE_RETURN_STRING (SDL_PIXELFORMAT_ARGB1555);
        CASE_RETURN_STRING (SDL_PIXELFORMAT_RGBA5551);
        CASE_RETURN_STRING (SDL_PIXELFORMAT_ABGR1555);
        CASE_RETURN_STRING (SDL_PIXELFORMAT_BGRA5551);
        CASE_RETURN_STRING (SDL_PIXELFORMAT_RGB565);
        CASE_RETURN_STRING (SDL_PIXELFORMAT_BGR565);
        CASE_RETURN_STRING (SDL_PIXELFORMAT_RGB24);
        CASE_RETURN_STRING (SDL_PIXELFORMAT_BGR24);
        CASE_RETURN_STRING (SDL_PIXELFORMAT_RGB888);
        CASE_RETURN_STRING (SDL_PIXELFORMAT_RGBX8888);
        CASE_RETURN_STRING (SDL_PIXELFORMAT_BGR888);
        CASE_RETURN_STRING (SDL_PIXELFORMAT_BGRX8888);
        CASE_RETURN_STRING (SDL_PIXELFORMAT_ARGB8888);
        CASE_RETURN_STRING (SDL_PIXELFORMAT_RGBA8888);
        CASE_RETURN_STRING (SDL_PIXELFORMAT_ABGR8888);
        CASE_RETURN_STRING (SDL_PIXELFORMAT_BGRA8888);
        CASE_RETURN_STRING (SDL_PIXELFORMAT_ARGB2101010);
        CASE_RETURN_STRING (SDL_PIXELFORMAT_YV12);
        CASE_RETURN_STRING (SDL_PIXELFORMAT_IYUV);
        CASE_RETURN_STRING (SDL_PIXELFORMAT_YUY2);
        CASE_RETURN_STRING (SDL_PIXELFORMAT_UYVY);
        CASE_RETURN_STRING (SDL_PIXELFORMAT_YVYU);
     default: return "(unknown format)";
    }
}
#undef CASE_RETURN_STRING

static void ffwd (PlotlibConfig *config, uint32_t *pixels, int w, int h)
{
    putTextf (pixels, w, h, 0.5, 0.5, ~0u, 1, "ffwd...");
}

void update_image (PlotlibConfig* config, SDL_Texture* texture, int isReady)
{
    int w, h;
    if (SDL_QueryTexture (texture, NULL, NULL, & w, & h))
        exit_error ("failed to query texture");

    uint32_t* pixels;
    int wb;
    int status = SDL_LockTexture (texture, NULL, (void**) & pixels, & wb);
    if (status)
        exit_error ("texture: %p, status: %d: %s\n", (void*) texture, status, SDL_GetError());

    memset (pixels, 0x11, sizeof (uint32_t) * (uint32_t) w * (uint32_t) h);
    if (isReady)
        config->callback (config, pixels, w, h);
    else
        ffwd (config, pixels, w, h);
    SDL_UnlockTexture (texture);
}

void plotlib_init (PlotlibConfig *config)
{
    if (SDL_Init (SDL_INIT_VIDEO) < 0)
        exit_error ("SDL could not initialize! SDL Error: %s\n", SDL_GetError ());

    config->window = SDL_CreateWindow ("Particle Simulator", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, config->windowWidth, config->windowHeight, SDL_WINDOW_SHOWN);
    true_or_exit_error (config->window != NULL, "Window could not be created! SDL Error: %s\n", SDL_GetError ());

    config->renderer = SDL_CreateRenderer (config->window, -1, SDL_RENDERER_ACCELERATED);
    true_or_exit_error (config->renderer != NULL, "Renderer could not be created! SDL Error: %s\n", SDL_GetError ());

    SDL_SetRenderDrawColor (config->renderer, 0xff, 0xff, 0xff, 0xff);
    SDL_SetWindowFullscreen (config->window, (config->fullscreen ? SDL_WINDOW_FULLSCREEN : 0));

    config->texture = SDL_CreateTexture (config->renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, config->windowWidth, config->windowHeight);
    true_or_exit_error (config->texture != NULL, "Texture could not be created! SDL Error: %s\n", SDL_GetError ());

    update_image (config, config->texture, 0);
    SDL_RenderCopy (config->renderer, config->texture, NULL, NULL);
    SDL_RenderPresent (config->renderer);
}

void plotlib_cleanup (PlotlibConfig *config)
{
    SDL_DestroyRenderer (config->renderer);
    SDL_DestroyWindow (config->window);
    SDL_Quit();
    config->renderer = NULL;
    config->window = NULL;
}

#define SDLK_APPLE_KEY 1073742051
int plotlib_main_loop (PlotlibConfig *config, int redraw)
{
    config->frameCounter++;
    int toggleFullscreen = 0;
    SDL_Event sdlEvent;
    while (SDL_PollEvent (& sdlEvent))
    {
        int pressed = 0;
        switch(sdlEvent.type)
        {
         case SDL_QUIT:
             //gameRunning = 0;
             break;
         case SDL_MOUSEBUTTONDOWN:
             redraw = config->on_mouse_pressed (config, sdlEvent.button.button);
             break;
         case SDL_MOUSEBUTTONUP:
             redraw = config->on_mouse_released (config);
             break;
         case SDL_MOUSEMOTION:
             redraw = config->on_mouse_motion (config, sdlEvent.motion.x, sdlEvent.motion.y);
             break;
         case SDL_KEYDOWN:
             pressed = 1;
             // fall through
         case SDL_KEYUP:
             {
                 static int state[300] = {0};
                 int key = sdlEvent.key.keysym.sym;
                 int cacheIndex = key;
                 if (cacheIndex < 0 || cacheIndex >= 256)
                 {
                     cacheIndex = 256;
                     switch (key)
                     {
                      case SDLK_TAB: cacheIndex++;
                      case SDLK_LEFT: cacheIndex++;
                      case SDLK_RIGHT: cacheIndex++;
                      case SDLK_UP: cacheIndex++;
                      case SDLK_DOWN: break;
                      case SDLK_APPLE_KEY: break;
                      default: cacheIndex = -1; print_debug ("key: %d", key); break;
                     }
                 }

                 int repeat = 0;
                 if (cacheIndex >= 0 && state[cacheIndex] == pressed)
                     repeat = 1;

                 if (cacheIndex >= 0)
                     state[cacheIndex] = pressed;

                 int ret = config->on_keyboard (config, key, pressed, repeat);
                 if (ret < 0)
                     return -1;
                 else if (ret == 1)
                     redraw = 1;
                 break;
             }
         default:
             break;
        }
    }

    if (toggleFullscreen)
    {
        toggleFullscreen = 0;
        config->fullscreen = !config->fullscreen;
        SDL_DestroyRenderer (config->renderer);
        SDL_DestroyWindow (config->window);
        SDL_DestroyTexture (config->texture);
        SDL_Quit();
        if (SDL_Init (SDL_INIT_VIDEO) < 0)
            exit_error ("SDL could not initialize! SDL Error: %s\n", SDL_GetError ());
        config->window = SDL_CreateWindow ("Plotlib", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                                           (config->fullscreen ? 2560 : config->windowWidth),
                                           (config->fullscreen ? 1600 : config->windowHeight),
                                           SDL_WINDOW_SHOWN);

        true_or_exit_error (config->window != NULL, "Window could not be created! SDL Error: %s\n", SDL_GetError ());
        config->renderer = SDL_CreateRenderer (config->window, -1, SDL_RENDERER_ACCELERATED);
        true_or_exit_error (config->renderer != NULL, "Renderer could not be created! SDL Error: %s\n", SDL_GetError ());
        SDL_SetRenderDrawColor (config->renderer, 0xff, 0xff, 0xff, 0xff);
        config->texture = SDL_CreateTexture (config->renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
                                             (config->fullscreen ? 2560 : config->windowWidth),
                                             (config->fullscreen ? 1600 : config->windowHeight));
        assert (config->texture);
        SDL_SetWindowFullscreen (config->window, config->fullscreen ? SDL_WINDOW_FULLSCREEN : 0);
        SDL_RenderPresent (config->renderer);
        usleep (100000);
    }

    if (redraw)
    {
        redraw = 0;
        update_image (config, config->texture, 1);
        SDL_RenderCopy (config->renderer, config->texture, NULL, NULL);
        SDL_RenderPresent (config->renderer);
    }

    return 0;
}
