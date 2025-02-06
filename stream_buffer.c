#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "cinterplot_common.h"
#include "stream_buffer.h"

static uint32_t next_power_of_two (uint32_t len)
{
    if (len && !(len & (len - 1)))
        return len;

    uint32_t count = 0;

    while (len != 0) {
        len >>= 1;
        count += 1;
    }

    return 1 << count;
}

StreamBuffer* stream_buffer_create (uint32_t requestedLen, size_t itemSize)
{
    StreamBuffer* sb = (StreamBuffer*) malloc (sizeof (StreamBuffer));
    assert (sb);

    sb->len      = next_power_of_two (requestedLen);
    sb->itemSize = itemSize;
    sb->index    = 0;
    sb->counter  = 0;

    // double buffered to continuously store data in two places,
    // always getting a contigious chunk of data.
    size_t bytes = itemSize * sb->len * 2;

    sb->buf = malloc (bytes);
    assert (sb->buf);

    return sb;
}

int stream_buffer_resize (StreamBuffer *sb, uint32_t newLen)
{
    if (newLen == sb->len)
        return 0;

    size_t bytes = sb->itemSize * newLen * 2;
    void *newBuf = malloc (bytes);
    assert (newBuf);


    void* dst0 = newBuf;
    void* dst1 = (void*) & ((uint8_t *) newBuf) [sb->itemSize * newLen];

    uint32_t oldLen;
    void *oldBuf;
    stream_buffer_get (sb, & oldBuf, & oldLen);
    uint32_t copyLen;
    if (oldLen <= newLen)
    {
        copyLen = oldLen;
        // the content of old buffer fits into the new buffer. Copy everything.
        memcpy (dst0, oldBuf, copyLen * sb->itemSize);
        memcpy (dst1, oldBuf, copyLen * sb->itemSize);
    }
    else
    {
        // the content of old buffer is larger than the  new buffer. Copy the last
        // data
        copyLen = newLen;
        void *src = (void*) & ((uint8_t *) oldBuf) [sb->itemSize * (oldLen - newLen)];
        memcpy (dst0, src, newLen * sb->itemSize);
        memcpy (dst1, src, newLen * sb->itemSize);
    }

    free (sb->buf);
    sb->buf     = newBuf;
    sb->len     = newLen;
    sb->index   = copyLen;
    sb->counter = copyLen;
    return 0;
}

int stream_buffer_destroy (StreamBuffer* sb)
{
    free (sb->buf);
    free (sb);
    return 0;
}

int stream_buffer_reset (StreamBuffer* sb)
{
    assert (sb);
    sb->index   = 0;
    sb->counter = 0;

    return 0;
}

int stream_buffer_insert (StreamBuffer* sb, void* src)
{
    uint32_t index0 = sb->index;
    uint32_t index1 = (index0 + sb->len) & (2 * sb->len - 1);

    //printf ("stream_buffer_insert: index0=%u, index1=%u  ", index0, index1);

    void* dst0 = (void*) & ((uint8_t *) sb->buf) [sb->itemSize * index0];
    void* dst1 = (void*) & ((uint8_t *) sb->buf) [sb->itemSize * index1];

    memcpy (dst0, src, sb->itemSize);
    memcpy (dst1, src, sb->itemSize);

    sb->index = (sb->index + 1) & (sb->len - 1);
    sb->counter++;

    return 0;
}

int stream_buffer_get (StreamBuffer *sb, void *_buf, uint32_t *len)
{
    assert (_buf);
    void **buf = (void **) _buf;
    *len = (uint32_t) MIN (sb->counter, sb->len);
    uint32_t indexStop = ((sb->index - 1) & (sb->len - 1)) + sb->len;
    uint32_t indexStart = indexStop - *len + 1;
    *buf = (void*) & ((uint8_t *) sb->buf) [sb->itemSize * indexStart];

    return 0;
}

int stream_buffer_counter_to_index (StreamBuffer* sb, uint64_t counter)
{
    uint32_t len = (uint32_t) MIN (sb->counter, sb->len);
    if (counter > sb->counter || (sb->counter - counter) >= len)
        return -1;

    uint32_t diff = (uint32_t) (sb->counter - counter);
    return (int) (len - diff - 1);
}

uint64_t stream_buffer_index_to_counter (StreamBuffer* sb, uint32_t index)
{
    int len = (int) MIN (sb->counter, sb->len);
    int currentIndex = len - 1;
    int diff = currentIndex - (int) index;
    if (diff > len || diff < 0)
        exit_error ("index %d out of range [0,%d)", index, len);

    return sb->counter - (uint64_t) diff;
}
