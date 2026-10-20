#include <errno.h>
#include <hpclang/stdio.h>
#include "hpgcc-line.h"

static int checked(int result)
{
    if (!result)
        return 0;
    if (!errno)
        errno = EIO;
    return -1;
}
char *hpclang_hpgcc_read_line(char *buffer, size_t capacity,
                              const struct hpclang_hpgcc_line_io *callbacks)
{
    if (!buffer || !capacity || !callbacks || !callbacks->read || !callbacks->echo ||
        !callbacks->erase) {
        errno = EINVAL;
        return NULL;
    }
    const struct hpclang_hpgcc_line_io io = *callbacks;
    int saved_errno = errno;
    size_t length = 0;
    buffer[0] = '\0';
    for (;;) {
        errno = 0;
        int byte = io.read(io.context);
        if (byte == HPCLANG_STDIO_ERROR) {
            if (!errno)
                errno = EIO;
            return NULL;
        }
        if (byte == HPCLANG_STDIO_EOF) {
            errno = saved_errno;
            return length ? buffer : NULL;
        }
        if (byte < 0 || byte > 255) {
            errno = EIO;
            return NULL;
        }
        errno = 0;
        if (byte == '\n' || byte == '\r') {
            if (checked(io.echo(io.context, '\n')))
                return NULL;
            errno = saved_errno;
            return buffer;
        }
        if (byte == '\b') {
            if (length) {
                if (checked(io.erase(io.context)))
                    return NULL;
                buffer[--length] = '\0';
            }
        } else if (byte >= 0x20) {
            if (length == capacity - 1) {
                if (io.full && checked(io.full(io.context)))
                    return NULL;
                continue;
            }
            if (checked(io.echo(io.context, (unsigned char)byte)))
                return NULL;
            buffer[length++] = (char)byte;
            buffer[length] = '\0';
        }
    }
}
