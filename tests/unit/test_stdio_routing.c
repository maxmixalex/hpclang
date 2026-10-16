#include <assert.h>
#include <errno.h>
#include <stddef.h>
#include <hpclang/stdio.h>
#include "stdio-routing.h"

static int resets, reads, writes, flushes, fail_flush, reenter, result;
void hpclang_stdio_clear_streams(void)
{
    ++resets;
}
static int read_byte(void *context)
{
    assert(context == &result);
    ++reads;
    if (result == HPCLANG_STDIO_ERROR)
        errno = ECANCELED;
    return result;
}
static int write_byte(void *context, unsigned stream, unsigned char byte)
{
    assert(context == &result && (stream == 1 || stream == 2) && byte == 0xff);
    ++writes;
    if (reenter) {
        assert(hpclang_stdio_route_write(1, byte) == -1 && errno == EBUSY);
        assert(hpclang_stdio_route_read() == HPCLANG_STDIO_ERROR && errno == EBUSY);
        assert(hpclang_stdio_unbind() == -1 && errno == EBUSY);
        assert(hpclang_stdio_bind(NULL) == -1 && errno == EBUSY);
        assert(hpclang_stdio_set_default(NULL) == -1 && errno == EBUSY);
    }
    return 0;
}
static int flush(void *context, unsigned stream)
{
    assert(context == &result);
    ++flushes;
    if (fail_flush) {
        errno = stream == 1 ? ENOSPC : EIO;
        return -1;
    }
    return 0;
}
int main(void)
{
    assert(hpclang_stdio_route_read() == HPCLANG_STDIO_EOF);
    assert(hpclang_stdio_route_write(1, 0xff) == 0);
    assert(hpclang_stdio_bind(NULL) == -1 && errno == EINVAL);
    struct hpclang_stdio_callbacks callbacks = {0};
    assert(hpclang_stdio_bind(&callbacks) == -1 && errno == EINVAL);
    callbacks = (struct hpclang_stdio_callbacks){write_byte, read_byte, flush, &result};
    assert(hpclang_stdio_bind(&callbacks) == 0 && resets == 1);
    assert(hpclang_stdio_bind(&callbacks) == -1 && errno == EBUSY);
    callbacks.read = NULL; /* descriptor was copied */
    result = 255;
    errno = EDOM;
    assert(hpclang_stdio_route_read() == 255 && reads == 1 && errno == EDOM);
    reenter = 1;
    assert(hpclang_stdio_route_write(2, 0xff) == 0 && writes == 1 && errno == EDOM);
    result = HPCLANG_STDIO_ERROR;
    assert(hpclang_stdio_route_read() == HPCLANG_STDIO_ERROR && errno == ECANCELED);
    result = 256;
    assert(hpclang_stdio_route_read() == HPCLANG_STDIO_ERROR && errno == EIO);
    result = -3;
    assert(hpclang_stdio_route_read() == HPCLANG_STDIO_ERROR && errno == EIO);
    fail_flush = 1;
    assert(hpclang_stdio_unbind() == -1 && errno == ENOSPC && resets == 2 && flushes == 2);
    assert(hpclang_stdio_route_read() == HPCLANG_STDIO_EOF);
    assert(hpclang_stdio_route_write(1, 0xff) == 0 && writes == 1);
    assert(hpclang_stdio_unbind() == 0 && flushes == 2);
    callbacks = (struct hpclang_stdio_callbacks){.read = read_byte, .context = &result};
    assert(hpclang_stdio_bind(&callbacks) == 0);
    result = 0;
    assert(hpclang_stdio_route_read() == 0);
    assert(hpclang_stdio_route_write(1, 0xff) == 0 && writes == 1);
    assert(hpclang_stdio_unbind() == 0);

    /* A compatibility default is dormant until used and never supplements an
     * explicit binding's missing callbacks. Installing it must not clear the
     * current owner's stream state. */
    assert(hpclang_stdio_set_default(NULL) == -1 && errno == EINVAL);
    struct hpclang_stdio_callbacks empty = {0};
    assert(hpclang_stdio_set_default(&empty) == -1 && errno == EINVAL);
    assert(hpclang_stdio_bind(&callbacks) == 0); /* read-only application */
    int saved_resets = resets, saved_writes = writes, saved_reads = reads;
    int saved_flushes = flushes;
    struct hpclang_stdio_callbacks defaults = {write_byte, read_byte, flush, &result};
    assert(hpclang_stdio_set_default(&defaults) == 0);
    assert(resets == saved_resets && writes == saved_writes && reads == saved_reads &&
           flushes == saved_flushes);
    defaults = empty; /* registration copies the descriptor */
    assert(hpclang_stdio_set_default(&defaults) == -1 && errno == EBUSY);
    assert(hpclang_stdio_route_write(1, 0xff) == 0 && writes == saved_writes);
    assert(hpclang_stdio_route_flush(1) == 0 && flushes == saved_flushes);
    assert(hpclang_stdio_unbind() == 0 && flushes == saved_flushes);

    assert(hpclang_stdio_unbind() == 0 && flushes == saved_flushes);

    /* Releasing the application resumes the default; reentrancy protection is
     * identical for a default callback and an explicit callback. */
    fail_flush = 0;
    errno = EDOM;
    assert(hpclang_stdio_route_write(1, 0xff) == 0 && writes == saved_writes + 1 && errno == EDOM);
    result = 42;
    assert(hpclang_stdio_route_read() == 42);
    result = HPCLANG_STDIO_ERROR;
    assert(hpclang_stdio_route_read() == HPCLANG_STDIO_ERROR && errno == ECANCELED);
    fail_flush = 1;
    assert(hpclang_stdio_route_flush(2) == -1 && errno == EIO);
    fail_flush = 0;
    callbacks = (struct hpclang_stdio_callbacks){.write = write_byte, .context = &result};
    assert(hpclang_stdio_bind(&callbacks) == 0);
    saved_reads = reads;
    assert(hpclang_stdio_route_read() == HPCLANG_STDIO_EOF && reads == saved_reads);
    assert(hpclang_stdio_unbind() == 0);
    result = 99;
    assert(hpclang_stdio_route_read() == 99);
    return 0;
}
