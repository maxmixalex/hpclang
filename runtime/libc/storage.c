#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
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

enum {
    HPCLANG_STREAM_FD_SLOTS = 8u
};

struct stream_fd_state {
    int32_t handle;
    int32_t position;
    int32_t extent;
    uint8_t used;
    uint8_t position_known;
    uint8_t extent_known;
};

static struct stream_fd_state stream_fds[HPCLANG_STREAM_FD_SLOTS];

static struct stream_fd_state *stream_fd_find(int fd)
{
    for (uint32_t i = 0u; i < HPCLANG_STREAM_FD_SLOTS; ++i) {
        if (stream_fds[i].used && stream_fds[i].handle == fd)
            return &stream_fds[i];
    }
    return NULL;
}

static struct stream_fd_state *stream_fd_claim(
    int32_t handle,
    int position_known,
    int extent_known)
{
    for (uint32_t i = 0u; i < HPCLANG_STREAM_FD_SLOTS; ++i) {
        if (!stream_fds[i].used) {
            stream_fds[i].handle = handle;
            stream_fds[i].position = 0;
            stream_fds[i].extent = 0;
            stream_fds[i].used = 1u;
            stream_fds[i].position_known = position_known != 0;
            stream_fds[i].extent_known = extent_known != 0;
            return &stream_fds[i];
        }
    }
    return NULL;
}

static void stream_fd_release(struct stream_fd_state *state)
{
    if (state != NULL)
        memset(state, 0, sizeof(*state));
}

static int stream_position_add(
    struct stream_fd_state *state,
    int32_t delta)
{
    if (state == NULL || !state->position_known)
        return 0;

    int64_t next = (int64_t)state->position + delta;
    if (next < 0 || next > INT32_MAX) {
        state->position_known = 0u;
        return 0;
    }

    state->position = (int32_t)next;
    if (state->extent_known && state->position > state->extent)
        state->extent = state->position;
    return 1;
}

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

    /*
     * KOS seek is a status-returning API, while POSIX lseek must return an
     * absolute file offset. Track the logical offset above the raw KOS handle.
     *
     * A truncated file has known extent zero. Append opens deliberately start
     * with an unknown position/extent because KOS owns the append cursor.
     */
    int append = (flags & O_APPEND) != 0;
    int truncated = (flags & O_TRUNC) != 0;
    struct stream_fd_state *state =
        stream_fd_claim(handle, !append, truncated);
    if (state == NULL) {
        uintptr_t close_addr = kos_entry(KOS_FUNC_FCLOSE);
        if (close_addr != 0u)
            (void)((kos_fn1)close_addr)((uint32_t)handle);
        errno = EMFILE;
        return -1;
    }

    return handle;
}

int close(int fd)
{
    struct stream_fd_state *state = stream_fd_find(fd);
    if (state == NULL) {
        errno = EBADF;
        return -1;
    }

    uintptr_t addr = kos_entry(KOS_FUNC_FCLOSE);
    if (addr == 0u)
        return unsupported();

    int32_t result = (int32_t)((kos_fn1)addr)((uint32_t)fd);
    if (result < 0)
        return set_kos_errno(result);

    stream_fd_release(state);
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

    struct stream_fd_state *state = stream_fd_find(fd);
    if (state == NULL) {
        errno = EBADF;
        return -1;
    }

    if (stream_position_add(state, result) &&
        (size_t)result < count) {
        state->extent = state->position;
        state->extent_known = 1u;
    }
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

    struct stream_fd_state *state = stream_fd_find(fd);
    if (state == NULL) {
        errno = EBADF;
        return -1;
    }

    (void)stream_position_add(state, result);
    return (ssize_t)result;
}

off_t lseek(int fd, off_t offset, int whence)
{
    if ((int64_t)offset < INT32_MIN || (int64_t)offset > INT32_MAX) {
        errno = EOVERFLOW;
        return (off_t)-1;
    }

    struct stream_fd_state *state = stream_fd_find(fd);
    if (state == NULL) {
        errno = EBADF;
        return (off_t)-1;
    }

    int64_t target;
    switch (whence) {
    case SEEK_SET:
        target = (int64_t)offset;
        break;
    case SEEK_CUR:
        if (!state->position_known) {
            errno = ENOTSUP;
            return (off_t)-1;
        }
        target = (int64_t)state->position + (int64_t)offset;
        break;
    case SEEK_END:
        if (!state->extent_known) {
            errno = ENOTSUP;
            return (off_t)-1;
        }
        target = (int64_t)state->extent + (int64_t)offset;
        break;
    default:
        errno = EINVAL;
        return (off_t)-1;
    }

    if (target < 0 || target > INT32_MAX) {
        errno = EINVAL;
        return (off_t)-1;
    }

    uintptr_t addr = kos_entry(KOS_FUNC_FSEEK);
    if (addr == 0u) {
        unsupported();
        return (off_t)-1;
    }

    /*
     * Always express the resolved logical target as KOS SEEK_SET. Historical
     * HPGCC exposes kos_fseek directly as fseek(), so its return value is a
     * success/error status rather than the POSIX absolute offset required by
     * lseek().
     */
    int32_t result = (int32_t)((kos_fn3)addr)(
        (uint32_t)fd,
        (uint32_t)(int32_t)target,
        KOS_SEEK_SET);
    if (result < 0) {
        set_kos_errno(result);
        return (off_t)-1;
    }

    state->position = (int32_t)target;
    state->position_known = 1u;
    return (off_t)target;
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
