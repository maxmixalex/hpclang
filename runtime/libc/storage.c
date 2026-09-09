#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdarg.h>
#include <stdint.h>
#include <sys/types.h>
#include <unistd.h>

#include <hpclang/device.h>
#include <hpclang/target.h>

/*
 * Stock HP49g+/HP50g KOS filesystem bridge.
 *
 * The public application API remains normal picolibc/POSIX stdio. Raw KOS
 * module/function numbers and handles are runtime-private.
 */

enum {
    KOS_SYSCALL_GET_FUNC_ENTRY = 218u,
    KOS_MODULE_FILESYSTEM = 1u,
    KOS_FUNC_FOPEN = 0u,
    KOS_FUNC_FREAD = 1u,
    KOS_FUNC_FWRITE = 2u,
    KOS_FUNC_FCLOSE = 3u,
    KOS_FUNC_FSEEK = 4u,
    KOS_FUNC_FDELETE = 5u,

    KOS_FILE_READONLY = 1u,
    KOS_FILE_READWRITE = 2u,
    KOS_FILE_APPEND = 3u,

    KOS_SEEK_SET = 0u,
    KOS_SEEK_CUR = 1u,
    KOS_SEEK_END = 2u
};

uint32_t hpclang_kos_swi3(
    uint32_t index,
    uint32_t arg1,
    uint32_t arg2,
    uint32_t arg3);

typedef uint32_t (*kos_fn1)(uint32_t);
typedef uint32_t (*kos_fn2)(uint32_t, uint32_t);
typedef uint32_t (*kos_fn3)(uint32_t, uint32_t, uint32_t);

static int stream_storage_available(void)
{
    const struct hpclang_device_info *device = hpclang_device_info();
    return device != NULL && device->family == HPCLANG_FAMILY_HP4950;
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

static int set_kos_errno(int32_t error)
{
    switch (error) {
    case -1:
    case -4:
    case -5:
        errno = EINVAL;
        break;
    case -2:
        errno = EBUSY;
        break;
    case -3:
        errno = EBADF;
        break;
    case -6:
        errno = ENOSPC;
        break;
    case -9:
    case -15:
        errno = ENODEV;
        break;
    case -10:
        errno = ENOMEM;
        break;
    case -11:
        errno = ENOTSUP;
        break;
    case -13:
        errno = EACCES;
        break;
    case -14:
    case -16:
        errno = ENOENT;
        break;
    case -7:
    case -8:
    case -100:
    default:
        errno = EIO;
        break;
    }
    return -1;
}

static int unsupported(void)
{
    errno = ENOSYS;
    return -1;
}

int open(const char *path, int flags, ...)
{
    (void)path;

    if (!stream_storage_available())
        return unsupported();

#ifdef O_EXCL
    if ((flags & O_EXCL) != 0) {
        errno = ENOTSUP;
        return -1;
    }
#endif

    uint32_t kos_mode;
    if ((flags & O_APPEND) != 0) {
        kos_mode = KOS_FILE_APPEND;
    } else if ((flags & O_ACCMODE) == O_RDONLY &&
               (flags & (O_CREAT | O_TRUNC)) == 0) {
        kos_mode = KOS_FILE_READONLY;
    } else {
        kos_mode = KOS_FILE_READWRITE;
    }

    uintptr_t open_addr = kos_entry(KOS_FUNC_FOPEN);
    if (open_addr == 0u)
        return unsupported();

    if ((flags & O_TRUNC) != 0) {
        uintptr_t delete_addr = kos_entry(KOS_FUNC_FDELETE);
        if (delete_addr != 0u) {
            int32_t deleted = (int32_t)((kos_fn1)delete_addr)(
                (uint32_t)(uintptr_t)path);
            if (deleted < 0 && deleted != -14 && deleted != -16)
                return set_kos_errno(deleted);
        }
    }

    int32_t handle = (int32_t)((kos_fn2)open_addr)(
        (uint32_t)(uintptr_t)path,
        kos_mode);
    if (handle < 0)
        return set_kos_errno(handle);

    return handle;
}

int close(int fd)
{
    uintptr_t addr = kos_entry(KOS_FUNC_FCLOSE);
    if (addr == 0u)
        return unsupported();

    int32_t result = (int32_t)((kos_fn1)addr)((uint32_t)fd);
    if (result < 0)
        return set_kos_errno(result);
    return 0;
}

ssize_t read(int fd, void *buffer, size_t count)
{
    if (count > (size_t)INT32_MAX) {
        errno = EINVAL;
        return -1;
    }

    uintptr_t addr = kos_entry(KOS_FUNC_FREAD);
    if (addr == 0u)
        return unsupported();

    uint32_t raw = ((kos_fn3)addr)(
        (uint32_t)fd,
        (uint32_t)(uintptr_t)buffer,
        (uint32_t)count);
    int32_t result = (int32_t)raw;
    if (result < 0)
        return set_kos_errno(result);
    return (ssize_t)result;
}

ssize_t write(int fd, const void *buffer, size_t count)
{
    if (count > (size_t)INT32_MAX) {
        errno = EINVAL;
        return -1;
    }

    uintptr_t addr = kos_entry(KOS_FUNC_FWRITE);
    if (addr == 0u)
        return unsupported();

    uint32_t raw = ((kos_fn3)addr)(
        (uint32_t)fd,
        (uint32_t)(uintptr_t)buffer,
        (uint32_t)count);
    int32_t result = (int32_t)raw;
    if (result < 0)
        return set_kos_errno(result);
    return (ssize_t)result;
}

off_t lseek(int fd, off_t offset, int whence)
{
    if ((int64_t)offset < INT32_MIN || (int64_t)offset > INT32_MAX) {
        errno = EOVERFLOW;
        return (off_t)-1;
    }

    uint32_t kos_whence;
    switch (whence) {
    case SEEK_SET:
        kos_whence = KOS_SEEK_SET;
        break;
    case SEEK_CUR:
        kos_whence = KOS_SEEK_CUR;
        break;
    case SEEK_END:
        kos_whence = KOS_SEEK_END;
        break;
    default:
        errno = EINVAL;
        return (off_t)-1;
    }

    uintptr_t addr = kos_entry(KOS_FUNC_FSEEK);
    if (addr == 0u) {
        unsupported();
        return (off_t)-1;
    }

    int32_t result = (int32_t)((kos_fn3)addr)(
        (uint32_t)fd,
        (uint32_t)(int32_t)offset,
        kos_whence);
    if (result < 0) {
        set_kos_errno(result);
        return (off_t)-1;
    }
    return (off_t)result;
}

int unlink(const char *path)
{
    uintptr_t addr = kos_entry(KOS_FUNC_FDELETE);
    if (addr == 0u)
        return unsupported();

    int32_t result = (int32_t)((kos_fn1)addr)(
        (uint32_t)(uintptr_t)path);
    if (result < 0)
        return set_kos_errno(result);
    return 0;
}
