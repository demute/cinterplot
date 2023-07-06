#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "stream_buffer.h"
#include "common.h"

StreamBuffer* stream_buffer_create (uint32_t len, size_t itemSize)
{
    StreamBuffer* bs = (StreamBuffer*) malloc (sizeof (StreamBuffer));
    assert (bs);
    print_debug ("bs: %p", bs);

    bs->len     = len;
    bs->itemSize    = itemSize;
    bs->index   = 0;
    bs->counter = 0;

    // double buffered to continuously store data in two places,
    // always getting a contigious chunk of data.
    size_t bytes = itemSize * len * 2;

    if (len & (len - 1))
        exit_error ("stream buffer length must be a power of two");

    bs->buf = malloc (bytes);
    print_debug ("bs->buf: %p", bs->buf);
    assert (bs->buf);

    return bs;
}

int stream_buffer_delete (StreamBuffer* bs)
{
    free (bs->buf);
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

    void* dst0 = (void*) & ((uint8_t *) bs->buf) [bs->itemSize / sizeof (uint8_t) * index0];
    void* dst1 = (void*) & ((uint8_t *) bs->buf) [bs->itemSize / sizeof (uint8_t) * index1];

    memcpy (dst0, src, bs->itemSize);
    memcpy (dst1, src, bs->itemSize);

    bs->index++;
    bs->index &= 2 * bs->len - 1;
    bs->counter++;

    return 0;
}

int stream_buffer_get (StreamBuffer* bs, void* _buf, uint32_t* len)
{
    assert (_buf);
    void **buf = (void **) _buf;
    *len = MIN (bs->counter, bs->len);
    uint32_t indexStop = ((bs->index - 1) & (bs->len - 1)) + bs->len;
    uint32_t indexStart = indexStop - *len + 1;
    *buf = (void*) & ((uint8_t *) bs->buf) [bs->itemSize / sizeof (uint8_t) * indexStart];

    return 0;
}

uint32_t stream_buffer_counter_to_index (StreamBuffer* bs, uint64_t counter)
{
    int len = MIN (bs->counter, bs->len);
    if (counter > bs->counter || (bs->counter - counter) >= len)
        return -1;

    // FIXME: the signedness / unsignedness promoted variables here seem scary
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
