#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <hpclang/device.h>
#include <hpclang/storage.h>

#include "storage-namespace.h"
#include "storage-media.h"

/*
 * HP49g+/HP50g byte-stream storage policy.
 *
 * Application file I/O is served by hpclang's owned FatFs namespace for the
 * whole native-process lifetime. Media pins are sampled before taking direct
 * controller ownership; Stock ResetSD runs only after releasing it.
 */

static int stream_storage_available(void)
{
    const struct hpclang_device_info *device = hpclang_device_info();
    return device != NULL && device->family == HPCLANG_FAMILY_HP4950;
}

static int unsupported(void)
{
    errno = ENOSYS;
    return -1;
}

uint32_t hpclang_storage_media_read32(unsigned offset)
{
    return *(volatile uint32_t *)(uintptr_t)(UINT32_C(0x07a00000) + offset);
}

int open(const char *path, int flags, ...)
{
    if (!stream_storage_available())
        return unsupported();
    return hpclang_namespace_open(path, flags);
}

int close(int fd)
{
    return hpclang_namespace_close(fd);
}

ssize_t read(int fd, void *buffer, size_t count)
{
    return hpclang_namespace_read(fd, buffer, count);
}

ssize_t write(int fd, const void *buffer, size_t count)
{
    return hpclang_namespace_write(fd, buffer, count);
}

off_t lseek(int fd, off_t offset, int whence)
{
    return hpclang_namespace_lseek(fd, offset, whence);
}

int ftruncate(int fd, off_t length)
{
    if (!stream_storage_available())
        return unsupported();
    return hpclang_namespace_ftruncate(fd, length);
}

int unlink(const char *path)
{
    if (!stream_storage_available())
        return unsupported();
    return hpclang_namespace_unlink(path);
}

int hpclang_stream_path_query(const char *path, struct hpclang_stream_path_info *info)
{
    if (!stream_storage_available())
        return unsupported();
    return hpclang_namespace_path_query(path, info);
}

int hpclang_stream_path_set_attributes(const char *path, uint32_t attributes, uint32_t mask)
{
    if (!stream_storage_available())
        return unsupported();
    return hpclang_namespace_path_set_attributes(path, attributes, mask);
}

int stat(const char *path, struct stat *info)
{
    if (info == NULL) {
        errno = EFAULT;
        return -1;
    }

    struct hpclang_stream_path_info path_info;
    if (hpclang_stream_path_query(path, &path_info) != 0)
        return -1;

    memset(info, 0, sizeof(*info));
    if (path_info.type == HPCLANG_STREAM_PATH_TYPE_DIRECTORY)
        info->st_mode = S_IFDIR;
    else
        info->st_mode = S_IFREG;

    info->st_mode |= S_IRUSR | S_IRGRP | S_IROTH;
    if ((path_info.attributes & HPCLANG_STREAM_PATH_ATTRIBUTE_READ_ONLY) == 0u)
        info->st_mode |= S_IWUSR;

    info->st_nlink = 1;
    info->st_size = (off_t)path_info.byte_size;
    return 0;
}

int rename(const char *old_path, const char *new_path)
{
    if (!stream_storage_available())
        return unsupported();
    return hpclang_namespace_rename(old_path, new_path);
}

int mkdir(const char *path, mode_t mode)
{
    (void)mode;
    if (!stream_storage_available())
        return unsupported();
    return hpclang_namespace_mkdir(path);
}

int rmdir(const char *path)
{
    if (!stream_storage_available())
        return unsupported();
    return hpclang_namespace_rmdir(path);
}

int hpclang_stream_storage_query(struct hpclang_stream_storage_info *info)
{
    if (!stream_storage_available())
        return unsupported();
    if (info == NULL) {
        errno = EFAULT;
        return -1;
    }

    if (!hpclang_namespace_direct_active()) {
        int present = 0;
        int read_only = 0;
        if (hpclang_storage_stock_media_query(&present, &read_only) != 0)
            return -1;

        if (!present) {
            memset(info, 0, sizeof(*info));
            info->size = sizeof(*info);
            info->version = HPCLANG_STREAM_STORAGE_INFO_VERSION;
            return 0;
        }
    }

    return hpclang_namespace_storage_query(info);
}

int hpclang_stream_storage_free_bytes(uint64_t *bytes)
{
    if (bytes == NULL) {
        errno = EFAULT;
        return -1;
    }

    struct hpclang_stream_storage_info info;
    if (hpclang_stream_storage_query(&info) != 0)
        return -1;

    if ((info.flags & HPCLANG_STREAM_STORAGE_MEDIA_PRESENT) == 0u) {
        errno = ENODEV;
        return -1;
    }
    if ((info.flags & HPCLANG_STREAM_STORAGE_FREE_BYTES_VALID) == 0u) {
        errno = ENOTSUP;
        return -1;
    }

    *bytes = info.free_bytes;
    return 0;
}
