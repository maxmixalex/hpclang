#include "store.h"
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

int notebook_export_file(const char *path, const char *text)
{
    if (!path || !*path) {
        errno = EINVAL;
        return -1;
    }
    size_t length;
    if (notebook_text_size(text, &length))
        return -1;
    char bytes[NOTEBOOK_CAPACITY];
    memcpy(bytes, text, length);
    bytes[length++] = '\n';
    int fd = open(path, O_WRONLY | O_CREAT | O_EXCL, 0600);
    if (fd < 0)
        return -1;
    int error = 0;
    size_t offset = 0;
    while (offset < length) {
        ssize_t written = write(fd, bytes + offset, length - offset);
        if (written < 0 && errno == EINTR)
            continue;
        if (written <= 0) {
            error = written < 0 ? errno : EIO;
            break;
        }
        offset += (size_t)written;
    }
    if (close(fd) && !error)
        error = errno;
    if (error) {
        errno = error;
        return -1;
    }
    return 0;
}
