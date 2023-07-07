#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "stream_buffer.h"
#include "common.h"

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
    StreamBuffer* bs = (StreamBuffer*) malloc (sizeof (StreamBuffer));
    assert (bs);

    bs->len      = next_power_of_two (requestedLen);
    bs->itemSize = itemSize;
    bs->index    = 0;
    bs->counter  = 0;

    // double buffered to continuously store data in two places,
    // always getting a contigious chunk of data.
    size_t bytes = itemSize * bs->len * 2;

    bs->buf = malloc (bytes);
    assert (bs->buf);

    return bs;
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
}

int stream_buffer_destroy (StreamBuffer* bs)
{
    free (bs->buf);
    free (bs);
    return 0;
}

int stream_buffer_reset (StreamBuffer* bs)
{
    assert (bs);
    bs->index   = 0;
    bs->counter = 0;

    return 0;
}

int stream_buffer_insert (StreamBuffer* bs, void* src)
{
    uint32_t index0 = bs->index;
    uint32_t index1 = (index0 + bs->len) & (2 * bs->len - 1);

    //printf ("stream_buffer_insert: index0=%u, index1=%u  ", index0, index1);

    void* dst0 = (void*) & ((uint8_t *) bs->buf) [bs->itemSize * index0];
    void* dst1 = (void*) & ((uint8_t *) bs->buf) [bs->itemSize * index1];

    memcpy (dst0, src, bs->itemSize);
    memcpy (dst1, src, bs->itemSize);

    bs->index = (bs->index + 1) & (bs->len - 1);
    bs->counter++;

    return 0;
}

int stream_buffer_get (StreamBuffer *bs, void *_buf, uint32_t *len)
{
    assert (_buf);
    void **buf = (void **) _buf;
    *len = MIN (bs->counter, bs->len);
    uint32_t indexStop = ((bs->index - 1) & (bs->len - 1)) + bs->len;
    uint32_t indexStart = indexStop - *len + 1;
    *buf = (void*) & ((uint8_t *) bs->buf) [bs->itemSize * indexStart];

    return 0;
}

uint32_t stream_buffer_counter_to_index (StreamBuffer* bs, uint64_t counter)
{
    int len = MIN (bs->counter, bs->len);
    if (counter > bs->counter || (bs->counter - counter) >= len)
        return -1;

    uint32_t diff = (uint32_t) (bs->counter - counter);
    return len - diff - 1;
}

uint64_t stream_buffer_index_to_counter (StreamBuffer* bs, uint32_t index)
{
    int len = MIN (bs->counter, bs->len);
    int currentIndex = len - 1;
    int diff = currentIndex - index;
    if (diff > len || diff < 0)
        exit_error ("index %d out of range [0,%d)", index, len);

    return bs->counter - (uint64_t) diff;
}
