#include <errno.h>
#include <stddef.h>
#include <hpclang/stdio.h>
#include "stdio-routing.h"

static struct hpclang_stdio_callbacks binding;
static int bound, busy;

int hpclang_stdio_bind(const struct hpclang_stdio_callbacks *callbacks)
{
    if (busy || bound) {
        errno = EBUSY;
        return -1;
    }
    if (!callbacks || (!callbacks->read && !callbacks->write)) {
        errno = EINVAL;
        return -1;
    }
    binding = *callbacks;
    bound = 1;
    hpclang_stdio_clear_streams();
    return 0;
}

static int enter(void)
{
    if (busy) {
        errno = EBUSY;
        return -1;
    }
    busy = 1;
    return 0;
}

static int finish(int result, int saved_errno)
{
    busy = 0;
    if (result != 0) {
        if (!errno)
            errno = EIO;
        return -1;
    }
    errno = saved_errno;
    return 0;
}

int hpclang_stdio_route_write(unsigned stream, unsigned char byte)
{
    if (enter() != 0)
        return -1;
    int saved_errno = errno;
    errno = 0;
    int result = bound && binding.write ? binding.write(binding.context, stream, byte) : 0;
    return finish(result, saved_errno);
}

int hpclang_stdio_route_flush(unsigned stream)
{
    if (enter() != 0)
        return -1;
    int saved_errno = errno;
    errno = 0;
    int result = bound && binding.flush ? binding.flush(binding.context, stream) : 0;
    return finish(result, saved_errno);
}

int hpclang_stdio_route_read(void)
{
    if (enter() != 0)
        return HPCLANG_STDIO_ERROR;
    int saved_errno = errno;
    errno = 0;
    int result = bound && binding.read ? binding.read(binding.context) : HPCLANG_STDIO_EOF;
    if (result < HPCLANG_STDIO_ERROR || result > 255) {
        errno = EIO;
        result = HPCLANG_STDIO_ERROR;
    }
    if (result == HPCLANG_STDIO_ERROR) {
        finish(-1, saved_errno);
        return result;
    }
    finish(0, saved_errno);
    return result;
}

int hpclang_stdio_unbind(void)
{
    if (busy) {
        errno = EBUSY;
        return -1;
    }
    int saved_errno = errno, error = 0;
    if (hpclang_stdio_route_flush(1) != 0)
        error = errno;
    if (hpclang_stdio_route_flush(2) != 0 && !error)
        error = errno;
    binding = (struct hpclang_stdio_callbacks){0};
    bound = 0;
    hpclang_stdio_clear_streams();
    errno = error ? error : saved_errno;
    return error ? -1 : 0;
}
