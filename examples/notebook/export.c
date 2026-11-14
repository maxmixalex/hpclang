#include "store.h"
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

static int write_all(int fd, const char *bytes, size_t length)
{
    size_t offset = 0;
    while (offset < length) {
        ssize_t written = write(fd, bytes + offset, length - offset);
        if (written < 0 && errno == EINTR)
            continue;
        if (written <= 0) {
            if (!written)
                errno = EIO;
            return -1;
        }
        offset += (size_t)written;
    }
    return 0;
}

int notebook_export_file(const char *path, const char *text)
{
    if (!path || !*path) {
        errno = EINVAL;
        return -1;
    }
    size_t length;
    if (notebook_text_size(text, &length))
        return -1;
    int fd = open(path, O_WRONLY | O_CREAT | O_EXCL, 0600);
    if (fd < 0)
        return -1;
    int error = 0;
    if (write_all(fd, text, length) || write_all(fd, "\n", 1))
        error = errno;
    if (close(fd) && !error)
        error = errno;
    if (error) {
        errno = error;
        return -1;
    }
    return 0;
}
