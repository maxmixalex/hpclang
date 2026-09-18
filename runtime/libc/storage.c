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
    KOS_FUNC_FRENAME = 6u,

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

static int kos_open_error_is_missing(int32_t error)
{
    /*
     * On stock HP49g+/HP50g, a valid FILE_READONLY open of a missing path
     * returns -200. Historical HPGCC named that value KOS_ERR_INVALIDPTR but
     * explicitly marked the name as a random guess. Stock E2E verifies -200
     * both before any writes and after delete, so interpret it as "missing"
     * only in path-opening/existence contexts rather than in the generic KOS
     * error table.
     */
    return error == -14 || error == -16 || error == -200;
}

static int set_kos_open_errno(int32_t error)
{
    if (kos_open_error_is_missing(error)) {
        errno = ENOENT;
        return -1;
    }
    return set_kos_errno(error);
}

static int unsupported(void)
{
    errno = ENOSYS;
    return -1;
}

int open(const char *path, int flags, ...)
{
    if (!stream_storage_available())
        return unsupported();

#ifdef O_EXCL
    /*
     * Stock KOS has no atomic create-exclusive primitive. Do not emulate
     * O_EXCL with a racy probe/create pair and pretend it is POSIX-atomic.
     */
    if ((flags & O_EXCL) != 0) {
        errno = ENOTSUP;
        return -1;
    }
#endif

    int access = flags & O_ACCMODE;
    int wants_create = (flags & O_CREAT) != 0;
    int wants_truncate = (flags & O_TRUNC) != 0;
    int wants_append = (flags & O_APPEND) != 0;

    if (wants_truncate && access == O_RDONLY) {
        errno = EINVAL;
        return -1;
    }

    uintptr_t open_addr = kos_entry(KOS_FUNC_FOPEN);
    if (open_addr == 0u)
        return unsupported();

    uintptr_t close_addr = kos_entry(KOS_FUNC_FCLOSE);
    int exists_known = 0;
    int exists = 0;
    int created_empty = 0;

    /*
     * KOS FILE_READWRITE can create a missing file. Probe only when POSIX
     * semantics require us to distinguish an existing file from a missing
     * one, or when working around KOS's broken append-create behavior.
     */
    int need_probe =
        wants_append ||
        (access != O_RDONLY && !wants_create) ||
        (access == O_RDONLY && wants_create);

    if (need_probe) {
        int32_t probe = (int32_t)((kos_fn2)open_addr)(
            (uint32_t)(uintptr_t)path,
            KOS_FILE_READONLY);
        exists_known = 1;

        if (probe >= 0) {
            exists = 1;
            if (close_addr == 0u)
                return unsupported();
            int32_t closed = (int32_t)((kos_fn1)close_addr)(
                (uint32_t)probe);
            if (closed < 0)
                return set_kos_errno(closed);
        } else if (!kos_open_error_is_missing(probe)) {
            return set_kos_open_errno(probe);
        }

        if (!exists && !wants_create) {
            errno = ENOENT;
            return -1;
        }
    }

    if (wants_truncate) {
        uintptr_t delete_addr = kos_entry(KOS_FUNC_FDELETE);
        if (delete_addr == 0u)
            return unsupported();

        int32_t deleted = (int32_t)((kos_fn1)delete_addr)(
            (uint32_t)(uintptr_t)path);
        if (deleted < 0) {
            if (!(wants_create && kos_open_error_is_missing(deleted)))
                return set_kos_errno(deleted);
        }
        exists_known = 1;
        exists = 0;
        created_empty = 1;
    }

    /*
     * Historical HPGCC 2.0 carries PATCH_BROKEN_KOS_FOPEN_APPEND because
     * FILE_APPEND does not create a missing file. For O_CREAT|O_APPEND,
     * create once with FILE_READWRITE, close it, then reopen in FILE_APPEND so
     * subsequent writes retain genuine append semantics.
     */
    if (wants_append && exists_known && !exists && wants_create) {
        int32_t created = (int32_t)((kos_fn2)open_addr)(
            (uint32_t)(uintptr_t)path,
            KOS_FILE_READWRITE);
        if (created < 0)
            return set_kos_open_errno(created);

        if (close_addr == 0u)
            return unsupported();
        int32_t closed = (int32_t)((kos_fn1)close_addr)(
            (uint32_t)created);
        if (closed < 0)
            return set_kos_errno(closed);

        exists = 1;
        created_empty = 1;
    }

    /*
     * O_RDONLY|O_CREAT is uncommon but valid. KOS has no read-only create
     * mode, so materialize an empty file first and reopen it read-only.
     */
    if (access == O_RDONLY && wants_create &&
        exists_known && !exists) {
        int32_t created = (int32_t)((kos_fn2)open_addr)(
            (uint32_t)(uintptr_t)path,
            KOS_FILE_READWRITE);
        if (created < 0)
            return set_kos_open_errno(created);

        if (close_addr == 0u)
            return unsupported();
        int32_t closed = (int32_t)((kos_fn1)close_addr)(
            (uint32_t)created);
        if (closed < 0)
            return set_kos_errno(closed);

        exists = 1;
        created_empty = 1;
    }

    uint32_t kos_mode;
    if (wants_append) {
        kos_mode = KOS_FILE_APPEND;
    } else if (access == O_RDONLY) {
        kos_mode = KOS_FILE_READONLY;
    } else {
        kos_mode = KOS_FILE_READWRITE;
    }

    int32_t handle = (int32_t)((kos_fn2)open_addr)(
        (uint32_t)(uintptr_t)path,
        kos_mode);
    if (handle < 0)
        return set_kos_open_errno(handle);

    /*
     * KOS seek is a status-returning API, while POSIX lseek must return an
     * absolute file offset. Track the logical offset above the raw KOS handle.
     *
     * Existing append files start at an unknown extent because the six basic
     * KOS stream calls expose no tell/length primitive. A newly created or
     * truncated append file is known to start at offset/extent zero.
     */
    int append_existing = wants_append && !created_empty;
    struct stream_fd_state *state =
        stream_fd_claim(
            handle,
            !append_existing,
            created_empty);
    if (state == NULL) {
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
    if (result < 0) {
        /*
         * Stock KOS uses the same undocumented -200 sentinel for a missing
         * path in delete as in fopen. Keep that interpretation local to the
         * path operation; -200 is not a generic KOS errno.
         */
        if (kos_open_error_is_missing(result)) {
            errno = ENOENT;
            return -1;
        }
        return set_kos_errno(result);
    }
    return 0;
}

int rename(const char *old_path, const char *new_path)
{
    if (!stream_storage_available())
        return unsupported();

    if (old_path == NULL || new_path == NULL) {
        errno = EFAULT;
        return -1;
    }

    uintptr_t addr = kos_entry(KOS_FUNC_FRENAME);
    if (addr == 0u)
        return unsupported();

    int32_t result = (int32_t)((kos_fn2)addr)(
        (uint32_t)(uintptr_t)old_path,
        (uint32_t)(uintptr_t)new_path);
    if (result < 0) {
        /*
         * Historical HPGCC 1.1 explicitly warns that kos_frename() appeared
         * to return -200 unconditionally on the firmware tested at the time.
         * Do not reinterpret that undocumented value as ENOENT here: the
         * Stock E2E probe must first establish whether the 2.15 HP49g+/50g
         * implementation actually performs a rename.
         */
        if (result == -200) {
            errno = EIO;
            return -1;
        }
        return set_kos_errno(result);
    }
    return 0;
}
