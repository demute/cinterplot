#ifdef __APPLE__

#include "macos_icon.h"

#include <CoreGraphics/CoreGraphics.h>
#include <objc/NSObjCRuntime.h>
#include <objc/message.h>
#include <objc/objc.h>
#include <objc/runtime.h>
#include <stdint.h>
#include <stdlib.h>

#define MAKE_COLOR(r,g,b) \
    (0xff000000u | (uint32_t)(((int)(r) << 16) | ((int)(g) << 8) | (int)(b)))

struct CGSizeC
{
    double width;
    double height;
};

/* ------------------------------------------------------------------------- */
/*  Objective-C runtime helpers                                              */
/* ------------------------------------------------------------------------- */

/*
 * These wrappers hide the noisy objc_msgSend casts. They do not add any new
 * behavior; they only make the call sites readable.
 */

static id objc_msg_id(id obj, SEL sel)
{
    return ((id (*)(id, SEL))objc_msgSend)(obj, sel);
}

// unused
//static id objc_msg_id_id(id obj, SEL sel, id arg)
//{
//    return ((id (*)(id, SEL, id))objc_msgSend)(obj, sel, arg);
//}

static id objc_msg_id_cgimage_size(id obj, SEL sel, CGImageRef image, struct CGSizeC size)
{
    return ((id (*)(id, SEL, CGImageRef, struct CGSizeC))objc_msgSend)(obj, sel, image, size);
}

static void objc_msg_void(id obj, SEL sel)
{
    ((void (*)(id, SEL))objc_msgSend)(obj, sel);
}

static void objc_msg_void_id(id obj, SEL sel, id arg)
{
    ((void (*)(id, SEL, id))objc_msgSend)(obj, sel, arg);
}

/* ------------------------------------------------------------------------- */
/*  Pixel memory lifetime                                                    */
/* ------------------------------------------------------------------------- */

/*
 * CoreGraphics calls this when the data provider is destroyed.
 * That means:
 *   - if provider creation succeeds, provider owns dstPixels
 *   - if provider creation fails, we must free dstPixels ourselves
 */
static void free_data_callback(void *info, const void *data, size_t size)
{
    (void)info;
    (void)size;
    free((void *)data);
}

/* ------------------------------------------------------------------------- */
/*  Rounded-rectangle test                                                   */
/* ------------------------------------------------------------------------- */

static int inside_rounded_rect(int x, int y, int left, int top, int right, int bottom, int radius)
{
    if (left > right || top > bottom)
        return 0;

    if (radius < 0)
        radius = 0;

    {
        int w = right - left + 1;
        int h = bottom - top + 1;

        if (radius > w / 2)
            radius = w / 2;
        if (radius > h / 2)
            radius = h / 2;
    }

    if (radius <= 0)
        return x >= left && x <= right && y >= top && y <= bottom;

    if (x < left || x > right || y < top || y > bottom)
        return 0;

    /* Fast accept in the central horizontal/vertical bands. */
    if (x >= left + radius && x <= right - radius)
        return 1;
    if (y >= top + radius && y <= bottom - radius)
        return 1;

    /* Otherwise test against the appropriate rounded corner circle. */
    {
        int cx = (x < left + radius) ? (left + radius) : (right - radius);
        int cy = (y < top  + radius) ? (top  + radius) : (bottom - radius);
        int dx = x - cx;
        int dy = y - cy;

        return dx * dx + dy * dy <= radius * radius;
    }
}

/* ------------------------------------------------------------------------- */
/*  Icon raster generation                                                   */
/* ------------------------------------------------------------------------- */

/*
 * Build a 512x512 icon:
 *   - transparent outside the rounded outer shape
 *   - gradient border between outer and inner rounded rectangles
 *   - source image scaled into the inner rectangle
 *   - a simple 5x5 max filter to thicken/highlight bright pixels
 */
static void make_icon(const uint32_t *srcPixels,
                      int srcWidth,
                      int srcHeight,
                      uint32_t *dstPixels,
                      int dstWidth,
                      int dstHeight)
{
    const int paddingSize  = 50;
    const int borderWidth  = 15;
    const int borderRadius = 85;

    const int outerLeft   = paddingSize;
    const int outerTop    = paddingSize;
    const int outerRight  = dstWidth  - paddingSize - 1;
    const int outerBottom = dstHeight - paddingSize - 1;

    const int innerLeft   = outerLeft   + borderWidth;
    const int innerTop    = outerTop    + borderWidth;
    const int innerRight  = outerRight  - borderWidth;
    const int innerBottom = outerBottom - borderWidth;

    int innerRadius = borderRadius - borderWidth;
    if (innerRadius < 0)
        innerRadius = 0;

    {
        const int contentWidth  = innerRight  - innerLeft + 1;
        const int contentHeight = innerBottom - innerTop  + 1;

        int dstY;
        for (dstY = 0; dstY < dstHeight; ++dstY)
        {
            int dstX;
            for (dstX = 0; dstX < dstWidth; ++dstX)
            {
                uint32_t *dst = &dstPixels[dstY * dstWidth + dstX];
                int inOuter;
                int inInner;

                *dst = 0x00000000u;

                inOuter = inside_rounded_rect(
                    dstX, dstY,
                    outerLeft, outerTop, outerRight, outerBottom,
                    borderRadius
                );

                if (!inOuter)
                    continue;

                inInner = inside_rounded_rect(
                    dstX, dstY,
                    innerLeft, innerTop, innerRight, innerBottom,
                    innerRadius
                );

                /* Border area: paint a simple diagonal gradient. */
                if (!inInner)
                {
                    double w = (double)(0.5 * dstX + dstY) / (dstHeight + 0.5 * dstWidth);
                    int r, g, b;
                    const int rMin =  80, rMax = 235;
                    const int gMin =  80, gMax = 235;
                    const int bMin =  80, bMax = 255;

                    w *= w;
                    r = rMin + (int)((1.0 - w) * (rMax - rMin));
                    g = gMin + (int)((1.0 - w) * (gMax - gMin));
                    b = bMin + (int)((1.0 - w) * (bMax - bMin));

                    *dst = MAKE_COLOR(r, g, b);
                    continue;
                }

                if (contentWidth <= 0 || contentHeight <= 0)
                    continue;

                /* Map destination pixel to source coordinates. */
                {
                    int relX = dstX - innerLeft;
                    int relY = dstY - innerTop;
                    int srcX = (int)(relX * ((double)srcWidth  / contentWidth));
                    int srcY = (int)(relY * ((double)srcHeight / contentHeight));

                    if (srcX < 0) srcX = 0;
                    if (srcX >= srcWidth) srcX = srcWidth - 1;
                    if (srcY < 0) srcY = 0;
                    if (srcY >= srcHeight) srcY = srcHeight - 1;

                    /*
                     * Max over a 5x5 neighborhood.
                     * This preserves/expands bright pixels and gives the icon
                     * a slightly bolder appearance.
                     */
                    {
                        int i, j;
                        *dst = 0xff000000u;

                        for (i = -2; i <= 2; ++i)
                        {
                            int y = srcY + i;
                            for (j = -2; j <= 2; ++j)
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
            }
        }
    }
}

/* ------------------------------------------------------------------------- */
/*  Cocoa/AppKit icon update                                                 */
/* ------------------------------------------------------------------------- */

void update_macos_icon(const uint32_t *srcPixels, int srcWidth, int srcHeight, int srcBytesPerRow)
{
    CGColorSpaceRef   colorSpace = 0;
    CGDataProviderRef dataProvider = 0;
    CGImageRef        cgImage = 0;
    uint32_t         *dstPixels = 0;

    id autoreleasePool = 0;
    id app = 0;
    id image = 0;

    const int dstWidth = 512;
    const int dstHeight = 512;
    const int dstBytesPerRow = dstWidth * 4;
    const size_t dstSizeBytes = (size_t)dstBytesPerRow * (size_t)dstHeight;

    /*
     * srcBytesPerRow is currently only validated, not used.
     * That is fine if the caller guarantees tightly packed srcPixels.
     */
    if (!srcPixels || srcWidth <= 0 || srcHeight <= 0 || srcBytesPerRow <= 0)
        return;

    dstPixels = (uint32_t *)calloc(1, dstSizeBytes);
    if (!dstPixels)
        return;

    make_icon(srcPixels, srcWidth, srcHeight, dstPixels, dstWidth, dstHeight);

    colorSpace = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
    if (!colorSpace)
        goto done;

    /*
     * After this succeeds, CoreGraphics owns dstPixels and will free it
     * through free_data_callback when the provider is released.
     */
    dataProvider = CGDataProviderCreateWithData(
        NULL,
        dstPixels,
        dstSizeBytes,
        free_data_callback
    );
    if (!dataProvider)
        goto done;

    cgImage = CGImageCreate(
        (size_t)dstWidth,
        (size_t)dstHeight,
        8,
        32,
        (size_t)dstBytesPerRow,
        colorSpace,
        kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Little,
        dataProvider,
        NULL,
        0,
        kCGRenderingIntentDefault
    );
    if (!cgImage)
        goto done;

    /*
     * AppKit may create autoreleased temporaries internally. Since this code
     * runs from plain C, create an autorelease pool explicitly.
     */
    {
        Class NSAutoreleasePoolClass = (Class)objc_getClass("NSAutoreleasePool");
        if (NSAutoreleasePoolClass)
        {
            id poolAlloc = objc_msg_id((id)NSAutoreleasePoolClass, sel_registerName("alloc"));
            if (poolAlloc)
                autoreleasePool = objc_msg_id(poolAlloc, sel_registerName("init"));
        }
    }

    {
        Class NSAppClass   = (Class)objc_getClass("NSApplication");
        Class NSImageClass = (Class)objc_getClass("NSImage");

        if (!NSAppClass || !NSImageClass)
            goto done;

        /* Obtain the singleton NSApplication instance. */
        app = objc_msg_id((id)NSAppClass, sel_registerName("sharedApplication"));
        if (!app)
            goto done;

        /* Create an NSImage that wraps the CoreGraphics image. */
        image = objc_msg_id((id)NSImageClass, sel_registerName("alloc"));
        if (!image)
            goto done;

        {
            struct CGSizeC size;
            id initializedImage;

            size.width  = (double)dstWidth;
            size.height = (double)dstHeight;

            initializedImage = objc_msg_id_cgimage_size(
                image,
                sel_registerName("initWithCGImage:size:"),
                cgImage,
                size
            );
            if (!initializedImage)
                goto done;

            image = initializedImage;
        }

        /*
         * NSApplication retains the icon image we pass here.
         * We still release our own local ownership before returning.
         */
        objc_msg_void_id(app, sel_registerName("setApplicationIconImage:"), image);
    }

done:
    if (image)
        objc_msg_void(image, sel_registerName("release"));
    if (cgImage)
        CGImageRelease(cgImage);
    if (dataProvider)
        CGDataProviderRelease(dataProvider);
    if (colorSpace)
        CGColorSpaceRelease(colorSpace);

    /*
     * If provider creation failed, dstPixels never became owned by the
     * provider, so we must free it here.
     */
    if (!dataProvider && dstPixels)
        free(dstPixels);

    if (autoreleasePool)
        objc_msg_void(autoreleasePool, sel_registerName("drain"));
}

#else

#include "macos_icon.h"

void update_macos_icon(const uint32_t *pixels, int srcWidth, int srcHeight, int srcBytesPerRow)
{
    (void)pixels;
    (void)srcWidth;
    (void)srcHeight;
    (void)srcBytesPerRow;
}

#endif
