#ifdef __APPLE__

#include "macos_icon.h"

#include <CoreGraphics/CoreGraphics.h>
#include <objc/objc.h>
#include <objc/runtime.h>
#include <objc/message.h>
#include <objc/NSObjCRuntime.h>
#include <stdlib.h>

#define MAKE_COLOR(r,g,b) (0xff000000 | (uint32_t) (((int)(r) << 16) | ((int)(g) << 8) | (int)(b)))

struct CGSizeC {
    double width;
    double height;
};

static void free_data_callback(void *info, const void *data, size_t size)
{
    (void)info;
    (void)size;
    free((void *)data);
}

static int inside_rounded_rect(int x, int y, int left, int top, int right, int bottom, int radius)
{
    if (left > right || top > bottom)
        return 0;

    if (radius < 0)
        radius = 0;

    int w = right - left + 1;
    int h = bottom - top + 1;

    if (radius > w / 2)
        radius = w / 2;
    if (radius > h / 2)
        radius = h / 2;

    if (radius <= 0)
        return x >= left && x <= right && y >= top && y <= bottom;

    if (x < left || x > right || y < top || y > bottom)
        return 0;

    if (x >= left + radius && x <= right - radius)
        return 1;
    if (y >= top + radius && y <= bottom - radius)
        return 1;

    int cx = (x < left + radius) ? (left + radius) : (right - radius);
    int cy = (y < top  + radius) ? (top  + radius) : (bottom - radius);
    int dx = x - cx;
    int dy = y - cy;

    return dx * dx + dy * dy <= radius * radius;
}

static int make_icon (const uint32_t *srcPixels, int srcWidth, int srcHeight, uint32_t *dstPixels, int dstWidth, int dstHeight)
{
    int paddingSize  = 50;
    int borderWidth  = 15;
    int borderRadius = 85;

    int outerLeft   = paddingSize;
    int outerTop    = paddingSize;
    int outerRight  = dstWidth  - paddingSize - 1;
    int outerBottom = dstHeight - paddingSize - 1;

    int innerLeft   = outerLeft   + borderWidth;
    int innerTop    = outerTop    + borderWidth;
    int innerRight  = outerRight  - borderWidth;
    int innerBottom = outerBottom - borderWidth;

    int innerRadius = borderRadius - borderWidth;
    if (innerRadius < 0)
        innerRadius = 0;

    int contentWidth  = innerRight  - innerLeft + 1;
    int contentHeight = innerBottom - innerTop  + 1;

    for (int dstY = 0; dstY < dstHeight; dstY++)
    {
        for (int dstX = 0; dstX < dstWidth; dstX++)
        {
            uint32_t *dst = &dstPixels[dstY * dstWidth + dstX];
            *dst = 0x00000000u;

            int inOuter = inside_rounded_rect(
              dstX, dstY,
              outerLeft, outerTop, outerRight, outerBottom,
              borderRadius
              );

            if (!inOuter)
                continue;

            int inInner = inside_rounded_rect(
              dstX, dstY,
              innerLeft, innerTop, innerRight, innerBottom,
              innerRadius
              );

            if (!inInner)
            {
                double w = (double) (0.5 * dstX + dstY) / (dstHeight + 0.5 * dstWidth);
                w *= w;
                int rMin =  80; int rMax = 235;
                int gMin =  80; int gMax = 235;
                int bMin =  80; int bMax = 255;
                int r = rMin + (1-w) * (rMax - rMin);
                int g = gMin + (1-w) * (gMax - gMin);
                int b = bMin + (1-w) * (bMax - bMin);

                *dst = MAKE_COLOR (r,g,b);
                continue;
            }

            if (contentWidth <= 0 || contentHeight <= 0)
                continue;

            int relX = dstX - innerLeft;
            int relY = dstY - innerTop;

            int srcX = (int)(relX * ((double)srcWidth  / contentWidth));
            int srcY = (int)(relY * ((double)srcHeight / contentHeight));

            if (srcX < 0) srcX = 0;
            if (srcX >= srcWidth) srcX = srcWidth - 1;
            if (srcY < 0) srcY = 0;
            if (srcY >= srcHeight) srcY = srcHeight - 1;

            *dst = 0xff000000u;

            for (int i = -2; i <= 2; i++)
            {
                int y = srcY + i;
                for (int j = -2; j <= 2; j++)
                {
                    int x = srcX + j;
                    if (0 <= y && y < srcHeight && 0 <= x && x < srcWidth)
                    {
                        const uint32_t *src = &srcPixels[y * srcWidth + x];
                        if (*dst < *src)
                            *dst = *src;
                    }
                }
            }
        }
    }
    return 0;
}

void update_macos_icon (const uint32_t *srcPixels, int srcWidth, int srcHeight, int srcBytesPerRow)
{
    if (!srcPixels || srcWidth <= 0 || srcHeight <= 0 || srcBytesPerRow <= 0)
        return;

    CGColorSpaceRef   cs       = 0;
    CGDataProviderRef provider = 0;
    CGImageRef        cgimg    = 0;
    id                img      = 0;
    id                pool     = 0;

    int dstWidth  = 512;
    int dstHeight = 512;
    int dstBytesPerRow = dstWidth * 4;

    size_t    dstSizeBytes = dstBytesPerRow * dstHeight;
    uint32_t *dstPixels    = (uint32_t *) calloc (dstSizeBytes, sizeof (uint8_t));
    if (!dstPixels)
        return;

    make_icon (srcPixels, srcWidth, srcHeight, dstPixels, dstWidth, dstHeight);

    cs = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
    if (!cs)
        goto done;

    provider = CGDataProviderCreateWithData(NULL, dstPixels, dstSizeBytes, free_data_callback);
    if (!provider)
        goto done;

    cgimg = CGImageCreate (
        (size_t) dstWidth,
        (size_t) dstHeight,
        8,
        32,
        (size_t) dstBytesPerRow,
        cs,
        kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Little,
        provider,
        NULL,
        0,
        kCGRenderingIntentDefault
    );
    if (!cgimg)
        goto done;

    Class NSAutoreleasePoolClass = (Class)objc_getClass("NSAutoreleasePool");
    if (NSAutoreleasePoolClass)
    {
        id poolAlloc = ((id (*)(id, SEL))objc_msgSend)(
          (id)NSAutoreleasePoolClass, sel_registerName("alloc"));
        if (poolAlloc)
        {
            pool = ((id (*)(id, SEL))objc_msgSend)(
              poolAlloc, sel_registerName("init"));
        }
    }

    Class NSAppClass = (Class)objc_getClass("NSApplication");
    Class NSImageClass = (Class)objc_getClass("NSImage");
    if (!NSAppClass || !NSImageClass)
        goto done;

    id app = ((id (*)(id, SEL))objc_msgSend)(
        (id)NSAppClass, sel_registerName("sharedApplication"));
    if (!app)
        goto done;

    img = ((id (*)(id, SEL))objc_msgSend)(
      (id)NSImageClass, sel_registerName("alloc"));
    if (!img)
        goto done;

    struct CGSizeC sz =
    {
        .width = (double)dstWidth,
        .height = (double)dstHeight
    };

    {
        id tmp = ((id (*)(id, SEL, CGImageRef, struct CGSizeC))objc_msgSend)(
          img,
          sel_registerName("initWithCGImage:size:"),
          cgimg,
          sz
          );
        if (!tmp)
        {
            ((void (*)(id, SEL))objc_msgSend)(img, sel_registerName("release"));
            img = 0;
            goto done;
        }
        img = tmp;
    }

    ((void (*)(id, SEL, id))objc_msgSend)(
      app,
      sel_registerName("setApplicationIconImage:"),
      img
      );

    ((void (*)(id, SEL))objc_msgSend)(img, sel_registerName("release"));
    img = 0;

done:
    if (img)
        ((void (*)(id, SEL))objc_msgSend)(img, sel_registerName("release"));
    if (cgimg) CGImageRelease(cgimg);
    if (provider) CGDataProviderRelease(provider);
    if (cs) CGColorSpaceRelease(cs);
    if (!provider && dstPixels) free(dstPixels);
    if (pool)
        ((void (*)(id, SEL))objc_msgSend)(pool, sel_registerName("drain"));
}

#else

#include "macos_icon.h"

void update_macos_icon (const uint32_t *pixels, int srcWidth, int srcHeight, int srcBytesPerRow)
{
    (void)pixels;
    (void)srcWidth;
    (void)srcHeight;
    (void)srcBytesPerRow;
}

#endif
