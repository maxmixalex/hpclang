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

/*
 * HP49g+/HP50g byte-stream storage policy.
 *
 * Application file I/O is served by hpclang's owned FatFs namespace for the
 * whole native-process lifetime. Stock KOS is intentionally retained only as
 * a host-coordination boundary: sample SD insertion/write-protect state before
 * taking direct controller ownership, then ResetSD after releasing it.
 */

enum {
    KOS_SYSCALL_GET_FUNC_ENTRY = 218u,
    KOS_MODULE_FILESYSTEM = 1u,
    KOS_FUNC_CHECKINSERT = 17u,
    KOS_FUNC_CHECKPROTECT = 18u
};

uint32_t hpclang_kos_swi3(
    uint32_t index,
    uint32_t arg1,
    uint32_t arg2,
    uint32_t arg3);

typedef uint32_t (*kos_fn0)(void);

enum stream_media_presence_state {
    STREAM_MEDIA_PRESENCE_UNKNOWN = 0,
    STREAM_MEDIA_PRESENCE_ABSENT = 1,
    STREAM_MEDIA_PRESENCE_PRESENT = 2
};

static uint8_t stream_media_presence;
static uint8_t stream_media_protect_known;
static uint8_t stream_media_protected;

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

static uintptr_t kos_entry(uint32_t index)
{
    if (!stream_storage_available())
        return 0u;

    return (uintptr_t)hpclang_kos_swi3(
        KOS_SYSCALL_GET_FUNC_ENTRY,
        KOS_MODULE_FILESYSTEM,
        index,
        0u);
}

static int stock_media_presence_query(void)
{
    if (stream_media_presence == STREAM_MEDIA_PRESENCE_PRESENT)
        return 1;
    if (stream_media_presence == STREAM_MEDIA_PRESENCE_ABSENT)
        return 0;

    uintptr_t addr = kos_entry(KOS_FUNC_CHECKINSERT);
    if (addr == 0u)
        return unsupported();

    int32_t result = (int32_t)((kos_fn0)addr)();
    if (result < 0) {
        errno = ENODEV;
        return -1;
    }

    stream_media_presence =
        result != 0
            ? STREAM_MEDIA_PRESENCE_PRESENT
            : STREAM_MEDIA_PRESENCE_ABSENT;
    return result != 0;
}

static int stock_media_read_only_query(void)
{
    if (stream_media_protect_known)
        return stream_media_protected != 0u;

    uintptr_t addr = kos_entry(KOS_FUNC_CHECKPROTECT);
    if (addr == 0u)
        return unsupported();

    int32_t result = (int32_t)((kos_fn0)addr)();
    if (result < 0) {
        errno = EIO;
        return -1;
    }

    stream_media_protect_known = 1u;
    stream_media_protected = result != 0;
    return stream_media_protected != 0u;
}

int hpclang_storage_stock_media_query(int *present, int *read_only)
{
    if (!stream_storage_available())
        return unsupported();
    if (present == NULL || read_only == NULL) {
        errno = EFAULT;
        return -1;
    }

    int is_present = stock_media_presence_query();
    if (is_present < 0)
        return -1;

    *present = is_present;
    if (!is_present) {
        *read_only = 0;
        return 0;
    }

    int is_read_only = stock_media_read_only_query();
    if (is_read_only < 0)
        return -1;

    *read_only = is_read_only;
    return 0;
}

void hpclang_storage_after_stock_reinit(void)
{
    stream_media_presence = STREAM_MEDIA_PRESENCE_UNKNOWN;
    stream_media_protect_known = 0u;
    stream_media_protected = 0u;
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

int unlink(const char *path)
{
    if (!stream_storage_available())
        return unsupported();
    return hpclang_namespace_unlink(path);
}

int hpclang_stream_path_query(
    const char *path,
    struct hpclang_stream_path_info *info)
{
    if (!stream_storage_available())
        return unsupported();
    return hpclang_namespace_path_query(path, info);
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
    if ((path_info.attributes &
         HPCLANG_STREAM_PATH_ATTRIBUTE_READ_ONLY) == 0u)
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

int hpclang_stream_storage_query(
    struct hpclang_stream_storage_info *info)
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
