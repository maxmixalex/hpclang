#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <hpclang/device.h>
#include <hpclang/storage.h>
#include <hpclang/target.h>

#include "../../third_party/fatfs/ff.h"
#include "../../third_party/fatfs/diskio.h"
#include "hp4950-fatfs-diskio.h"
#include "storage-namespace-lease.h"
#include "storage-namespace.h"

enum {
    KOS_SYSCALL_RESET_SD = 19u
};

uint32_t hpclang_kos_swi3(
    uint32_t index,
    uint32_t arg1,
    uint32_t arg2,
    uint32_t arg3);

#define FAT_ATTRIBUTE_VOLUME UINT8_C(0x08)

struct hpclang_stream_directory {
    DIR fatfs;
    uint32_t magic;
};

#define DIRECTORY_MAGIC UINT32_C(0x48445031)
#define DIRECT_FD_BASE  UINT32_C(0x4000)
#define DIRECT_FD_SLOTS 8u

struct direct_fd_state {
    FIL file;
    uint32_t position;
    uint8_t used;
    uint8_t append;
};

static FATFS namespace_fatfs;
static struct hpclang_namespace_lease namespace_lease;
static struct direct_fd_state direct_fds[DIRECT_FD_SLOTS];
static uint8_t namespace_ready;
static uint8_t namespace_media_read_only;

static int set_fatfs_errno(FRESULT result)
{
    switch (result) {
    case FR_OK:
        return 0;
    case FR_NOT_READY:
    case FR_INVALID_DRIVE:
    case FR_NOT_ENABLED:
    case FR_NO_FILESYSTEM:
        errno = ENODEV;
        break;
    case FR_NO_FILE:
    case FR_NO_PATH:
        errno = ENOENT;
        break;
    case FR_INVALID_NAME:
    case FR_INVALID_PARAMETER:
        errno = EINVAL;
        break;
    case FR_DENIED:
        errno = EACCES;
        break;
    case FR_EXIST:
        errno = EEXIST;
        break;
    case FR_INVALID_OBJECT:
        errno = EBADF;
        break;
    case FR_WRITE_PROTECTED:
        errno = EROFS;
        break;
    case FR_TIMEOUT:
    case FR_LOCKED:
        errno = EBUSY;
        break;
    case FR_NOT_ENOUGH_CORE:
        errno = ENOMEM;
        break;
    case FR_TOO_MANY_OPEN_FILES:
        errno = EMFILE;
        break;
    case FR_DISK_ERR:
    case FR_INT_ERR:
    case FR_MKFS_ABORTED:
    default:
        errno = EIO;
        break;
    }
    return -1;
}

static int path_valid(const char *path)
{
    if (path == NULL) {
        errno = EFAULT;
        return 0;
    }
    if (path[0] == '\0' || strchr(path, ':') != NULL) {
        errno = EINVAL;
        return 0;
    }
    return 1;
}

static int lease_stock_idle(void *context)
{
    /*
     * All hpclang byte-stream descriptors use the direct namespace now, so
     * there are no runtime-owned Stock KOS file handles to drain before
     * takeover. The callback remains part of the generic lease state machine.
     */
    (void)context;
    return 1;
}

static int lease_direct_acquire(void *context)
{
    (void)context;
    return hpclang_hp4950_fatfs_controller_acquire(
        namespace_media_read_only != 0u);
}

static int lease_direct_mount(void *context)
{
    (void)context;
    memset(&namespace_fatfs, 0, sizeof(namespace_fatfs));
    return f_mount(&namespace_fatfs, "", 1u) == FR_OK ? 0 : -1;
}

static int lease_direct_unmount(void *context)
{
    (void)context;
    return f_mount(NULL, "", 0u) == FR_OK ? 0 : -1;
}

static int lease_direct_release(void *context)
{
    (void)context;
    hpclang_hp4950_fatfs_controller_release();
    return 0;
}

static int lease_stock_reinit(void *context)
{
    (void)context;
    (void)hpclang_kos_swi3(KOS_SYSCALL_RESET_SD, 0u, 0u, 0u);
    hpclang_storage_after_stock_reinit();
    return 0;
}

static const struct hpclang_namespace_lease_ops lease_ops = {
    .stock_idle = lease_stock_idle,
    .direct_acquire = lease_direct_acquire,
    .direct_mount = lease_direct_mount,
    .direct_unmount = lease_direct_unmount,
    .direct_release = lease_direct_release,
    .stock_reinit = lease_stock_reinit,
};

static void namespace_init(void)
{
    if (namespace_ready)
        return;
    hpclang_namespace_lease_init(&namespace_lease, &lease_ops, NULL);
    namespace_ready = 1u;
}

static int namespace_acquire(void)
{
    const struct hpclang_device_info *device = hpclang_device_info();
    if (device == NULL || device->family != HPCLANG_FAMILY_HP4950) {
        errno = ENOSYS;
        return -1;
    }

    namespace_init();

    if (namespace_lease.state == HPCLANG_NAMESPACE_DIRECT)
        return 0;
    if (namespace_lease.state != HPCLANG_NAMESPACE_STOCK) {
        errno = EIO;
        return -1;
    }

    /*
     * KOS participates only at the ownership boundary: sample insertion and
     * write-protect state before touching the SD controller. All namespace
     * and byte-stream operations after this point are FatFs-backed.
     */
    int present = 0;
    int read_only = 0;
    if (hpclang_storage_stock_media_query(&present, &read_only) != 0)
        return -1;
    if (!present) {
        errno = ENODEV;
        return -1;
    }
    namespace_media_read_only = read_only != 0;

    int result = hpclang_namespace_lease_acquire(&namespace_lease);
    if (result != HPCLANG_NAMESPACE_LEASE_OK) {
        switch (result) {
        case HPCLANG_NAMESPACE_LEASE_BUSY:
        case HPCLANG_NAMESPACE_LEASE_ACQUIRE_FAILED:
            errno = EBUSY;
            break;
        case HPCLANG_NAMESPACE_LEASE_MOUNT_FAILED:
            errno = ENODEV;
            break;
        default:
            errno = EIO;
            break;
        }
        return -1;
    }

    return 0;
}

int hpclang_namespace_direct_active(void)
{
    return namespace_ready &&
           namespace_lease.state == HPCLANG_NAMESPACE_DIRECT;
}

static struct direct_fd_state *direct_fd_from_number(int fd)
{
    if (fd < (int)DIRECT_FD_BASE ||
        fd >= (int)(DIRECT_FD_BASE + DIRECT_FD_SLOTS))
        return NULL;

    struct direct_fd_state *slot =
        &direct_fds[(uint32_t)fd - DIRECT_FD_BASE];
    return slot->used ? slot : NULL;
}

static struct direct_fd_state *direct_fd_claim(int *fd)
{
    for (uint32_t i = 0u; i < DIRECT_FD_SLOTS; ++i) {
        if (!direct_fds[i].used) {
            memset(&direct_fds[i], 0, sizeof(direct_fds[i]));
            direct_fds[i].used = 1u;
            *fd = (int)(DIRECT_FD_BASE + i);
            return &direct_fds[i];
        }
    }

    errno = EMFILE;
    return NULL;
}

int hpclang_namespace_open(const char *path, int flags)
{
    if (!path_valid(path))
        return -1;
    if (namespace_acquire() != 0)
        return -1;

    int access = flags & O_ACCMODE;
    BYTE mode;
    if (access == O_RDONLY) {
        mode = FA_READ;
    } else if (access == O_WRONLY) {
        mode = FA_WRITE;
    } else if (access == O_RDWR) {
        mode = FA_READ | FA_WRITE;
    } else {
        errno = EINVAL;
        return -1;
    }

    int wants_create = (flags & O_CREAT) != 0;
    int wants_truncate = (flags & O_TRUNC) != 0;
    int wants_append = (flags & O_APPEND) != 0;
#ifdef O_EXCL
    int wants_exclusive = wants_create && (flags & O_EXCL) != 0;
#else
    int wants_exclusive = 0;
#endif

    if (wants_truncate && access == O_RDONLY) {
        errno = EINVAL;
        return -1;
    }

    int fd;
    struct direct_fd_state *slot = direct_fd_claim(&fd);
    if (slot == NULL)
        return -1;

    BYTE method = FA_OPEN_EXISTING;
    if (wants_exclusive)
        method = FA_CREATE_NEW;
    else if (wants_create && wants_truncate)
        method = FA_CREATE_ALWAYS;
    else if (wants_create)
        method = FA_OPEN_ALWAYS;

    /*
     * O_RDONLY|O_CREAT on an existing read-only file must remain a read open,
     * not an overwrite attempt. Probe only this special case before selecting
     * FatFs' create method.
     */
    if (access == O_RDONLY && wants_create && !wants_exclusive) {
        FILINFO info;
        FRESULT stat_result = f_stat(path, &info);
        if (stat_result == FR_OK)
            method = FA_OPEN_EXISTING;
        else if (stat_result != FR_NO_FILE && stat_result != FR_NO_PATH) {
            memset(slot, 0, sizeof(*slot));
            return set_fatfs_errno(stat_result);
        }
    }

    FRESULT fr = f_open(&slot->file, path, mode | method);
    if (fr != FR_OK) {
        memset(slot, 0, sizeof(*slot));
        return set_fatfs_errno(fr);
    }

    if (wants_truncate && !wants_create) {
        fr = f_truncate(&slot->file);
        if (fr != FR_OK) {
            (void)f_close(&slot->file);
            memset(slot, 0, sizeof(*slot));
            return set_fatfs_errno(fr);
        }
    }

    slot->append = wants_append && access != O_RDONLY;
    slot->position = slot->append ? (uint32_t)f_size(&slot->file) : 0u;
    return fd;
}

int hpclang_namespace_close(int fd)
{
    struct direct_fd_state *slot = direct_fd_from_number(fd);
    if (slot == NULL) {
        errno = EBADF;
        return -1;
    }

    FRESULT fr = f_close(&slot->file);
    memset(slot, 0, sizeof(*slot));
    return fr == FR_OK ? 0 : set_fatfs_errno(fr);
}

ssize_t hpclang_namespace_read(int fd, void *buffer, size_t count)
{
    struct direct_fd_state *slot = direct_fd_from_number(fd);
    if (slot == NULL) {
        errno = EBADF;
        return -1;
    }
    if (buffer == NULL && count != 0u) {
        errno = EFAULT;
        return -1;
    }
    if (count > (size_t)INT32_MAX) {
        errno = EINVAL;
        return -1;
    }
    if (count == 0u)
        return 0;

    uint32_t extent = (uint32_t)f_size(&slot->file);
    if (slot->position >= extent)
        return 0;

    FRESULT fr = f_lseek(&slot->file, slot->position);
    if (fr != FR_OK)
        return set_fatfs_errno(fr);

    UINT got = 0u;
    fr = f_read(&slot->file, buffer, (UINT)count, &got);
    if (fr != FR_OK)
        return set_fatfs_errno(fr);

    slot->position += got;
    return (ssize_t)got;
}

ssize_t hpclang_namespace_write(int fd, const void *buffer, size_t count)
{
    struct direct_fd_state *slot = direct_fd_from_number(fd);
    if (slot == NULL) {
        errno = EBADF;
        return -1;
    }
    if (buffer == NULL && count != 0u) {
        errno = EFAULT;
        return -1;
    }
    if (count > (size_t)INT32_MAX) {
        errno = EINVAL;
        return -1;
    }
    if (count == 0u)
        return 0;

    uint32_t target = slot->append
        ? (uint32_t)f_size(&slot->file)
        : slot->position;
    FRESULT fr = f_lseek(&slot->file, target);
    if (fr != FR_OK)
        return set_fatfs_errno(fr);

    UINT written = 0u;
    fr = f_write(&slot->file, buffer, (UINT)count, &written);
    if (fr != FR_OK)
        return set_fatfs_errno(fr);

    slot->position = target + written;
    return (ssize_t)written;
}

off_t hpclang_namespace_lseek(int fd, off_t offset, int whence)
{
    struct direct_fd_state *slot = direct_fd_from_number(fd);
    if (slot == NULL) {
        errno = EBADF;
        return (off_t)-1;
    }

    int64_t base;
    switch (whence) {
    case SEEK_SET:
        base = 0;
        break;
    case SEEK_CUR:
        base = slot->position;
        break;
    case SEEK_END:
        base = (uint32_t)f_size(&slot->file);
        break;
    default:
        errno = EINVAL;
        return (off_t)-1;
    }

    int64_t target = base + (int64_t)offset;
    if (target < 0 || target > INT32_MAX) {
        errno = EINVAL;
        return (off_t)-1;
    }

    /*
     * Keep POSIX lseek logical: moving beyond EOF must not grow the FAT file.
     * The physical FatFs pointer is synchronized lazily by read/write.
     */
    slot->position = (uint32_t)target;
    return (off_t)target;
}

int hpclang_namespace_ftruncate(int fd, off_t length)
{
    static const BYTE zeroes[512] = { 0 };

    struct direct_fd_state *slot = direct_fd_from_number(fd);
    if (slot == NULL) {
        errno = EBADF;
        return -1;
    }
    if ((int64_t)length < 0 || (uint64_t)length > UINT32_MAX) {
        errno = EINVAL;
        return -1;
    }

    FSIZE_t target = (FSIZE_t)length;
    FSIZE_t original_size = f_size(&slot->file);
    FRESULT fr;

    if (target <= original_size) {
        fr = f_lseek(&slot->file, target);
        if (fr != FR_OK)
            return set_fatfs_errno(fr);

        fr = f_truncate(&slot->file);
        if (fr != FR_OK)
            return set_fatfs_errno(fr);
    } else {
        /*
         * FatFs can stretch a writable file by seeking beyond EOF, but the
         * newly allocated range is not specified to be zero-filled. POSIX
         * ftruncate() requires reads from the extended range to return zero,
         * so materialize that range explicitly.
         */
        fr = f_lseek(&slot->file, original_size);
        if (fr != FR_OK)
            return set_fatfs_errno(fr);

        FSIZE_t remaining = target - original_size;
        while (remaining != 0u) {
            UINT chunk = remaining > sizeof(zeroes)
                ? (UINT)sizeof(zeroes)
                : (UINT)remaining;
            UINT written = 0u;
            fr = f_write(&slot->file, zeroes, chunk, &written);
            if (fr != FR_OK || written != chunk) {
                /*
                 * Best-effort rollback keeps an ENOSPC/I/O failure from
                 * silently leaving a successful-looking larger file.
                 */
                (void)f_lseek(&slot->file, original_size);
                (void)f_truncate(&slot->file);
                if (fr != FR_OK)
                    return set_fatfs_errno(fr);
                errno = ENOSPC;
                return -1;
            }
            remaining -= written;
        }
    }

    /*
     * POSIX ftruncate() does not change the logical file offset. FatFs had to
     * move its internal pointer; read/write resynchronize it from slot->position.
     */
    return 0;
}


static void export_fat_time(
    struct hpclang_stream_civil_datetime *dst,
    WORD date,
    WORD time)
{
    memset(dst, 0, sizeof(*dst));
    dst->year = (uint16_t)(1980u + ((date >> 9) & 0x7fu));
    dst->month = (uint8_t)((date >> 5) & 0x0fu);
    dst->day = (uint8_t)(date & 0x1fu);
    dst->hour = (uint8_t)((time >> 11) & 0x1fu);
    dst->minute = (uint8_t)((time >> 5) & 0x3fu);
    dst->second = (uint8_t)((time & 0x1fu) * 2u);
}

static uint32_t export_attributes(BYTE attributes)
{
    uint32_t out = 0u;
    if ((attributes & AM_RDO) != 0u)
        out |= HPCLANG_STREAM_PATH_ATTRIBUTE_READ_ONLY;
    if ((attributes & AM_HID) != 0u)
        out |= HPCLANG_STREAM_PATH_ATTRIBUTE_HIDDEN;
    if ((attributes & AM_SYS) != 0u)
        out |= HPCLANG_STREAM_PATH_ATTRIBUTE_SYSTEM;
    if ((attributes & AM_ARC) != 0u)
        out |= HPCLANG_STREAM_PATH_ATTRIBUTE_ARCHIVE;
    return out;
}

static uint32_t export_type(BYTE attributes)
{
    if ((attributes & AM_DIR) != 0u)
        return HPCLANG_STREAM_PATH_TYPE_DIRECTORY;
    if ((attributes & FAT_ATTRIBUTE_VOLUME) != 0u)
        return HPCLANG_STREAM_PATH_TYPE_VOLUME_LABEL;
    return HPCLANG_STREAM_PATH_TYPE_FILE;
}

static void export_info(
    struct hpclang_stream_path_info *dst,
    const FILINFO *src)
{
    memset(dst, 0, sizeof(*dst));
    dst->size = sizeof(*dst);
    dst->version = HPCLANG_STREAM_PATH_INFO_VERSION;
    dst->byte_size = (uint64_t)src->fsize;
    dst->type = export_type(src->fattrib);
    dst->attributes = export_attributes(src->fattrib);
    export_fat_time(&dst->last_modified, src->fdate, src->ftime);
    dst->time_validity = HPCLANG_STREAM_PATH_LAST_MODIFIED_VALID;
}

static int one_shot_begin(const char *path)
{
    if (!path_valid(path))
        return -1;
    return namespace_acquire();
}

int hpclang_namespace_path_query(
    const char *path,
    struct hpclang_stream_path_info *info)
{
    if (info == NULL) {
        errno = EFAULT;
        return -1;
    }
    if (one_shot_begin(path) != 0)
        return -1;

    int result = 0;
    if (strcmp(path, "/") == 0) {
        memset(info, 0, sizeof(*info));
        info->size = sizeof(*info);
        info->version = HPCLANG_STREAM_PATH_INFO_VERSION;
        info->type = HPCLANG_STREAM_PATH_TYPE_DIRECTORY;
    } else {
        FILINFO fat_info;
        FRESULT fr = f_stat(path, &fat_info);
        if (fr != FR_OK) {
            result = set_fatfs_errno(fr);
        } else {
            export_info(info, &fat_info);
        }
    }

    return result;
}

int hpclang_namespace_storage_query(
    struct hpclang_stream_storage_info *info)
{
    if (info == NULL) {
        errno = EFAULT;
        return -1;
    }
    if (namespace_acquire() != 0)
        return -1;

    memset(info, 0, sizeof(*info));
    info->size = sizeof(*info);
    info->version = HPCLANG_STREAM_STORAGE_INFO_VERSION;
    info->flags = HPCLANG_STREAM_STORAGE_MEDIA_PRESENT;

    if ((disk_status(0u) & STA_PROTECT) != 0u)
        info->flags |= HPCLANG_STREAM_STORAGE_MEDIA_READ_ONLY;

    DWORD free_clusters = 0u;
    FATFS *fs = NULL;
    FRESULT fr = f_getfree("/", &free_clusters, &fs);
    int result = 0;
    if (fr != FR_OK) {
        result = set_fatfs_errno(fr);
    } else if (fs == NULL) {
        errno = EIO;
        result = -1;
    } else {
        info->free_bytes =
            (uint64_t)free_clusters * (uint64_t)fs->csize * UINT64_C(512);
        info->flags |= HPCLANG_STREAM_STORAGE_FREE_BYTES_VALID;

        /*
         * Report the mounted FAT volume, not the entire physical SD card.
         * FatFs exposes the volume base, data base, cluster size and cluster
         * count after mount. Reconstruct the addressable volume sectors from
         * those fields so a partitioned card does not inflate FSGetVolumeSize.
         */
        if (fs->n_fatent >= 2u && fs->csize != 0u &&
            fs->database >= fs->volbase) {
            uint64_t metadata_sectors =
                (uint64_t)(fs->database - fs->volbase);
            uint64_t data_sectors =
                (uint64_t)(fs->n_fatent - 2u) * (uint64_t)fs->csize;
            info->capacity_bytes =
                (metadata_sectors + data_sectors) * UINT64_C(512);
            info->flags |= HPCLANG_STREAM_STORAGE_CAPACITY_BYTES_VALID;
        }
    }

    return result;
}

static int fatfs_unlink(const char *path, int require_directory)
{
    if (one_shot_begin(path) != 0)
        return -1;

    int result = 0;
    if (require_directory) {
        FILINFO info;
        FRESULT fr = f_stat(path, &info);
        if (fr != FR_OK) {
            result = set_fatfs_errno(fr);
        } else if ((info.fattrib & AM_DIR) == 0u) {
            errno = ENOTDIR;
            result = -1;
        }
    }

    if (result == 0) {
        FRESULT fr = f_unlink(path);
        if (fr != FR_OK)
            result = set_fatfs_errno(fr);
    }

    return result;
}

int hpclang_namespace_unlink(const char *path)
{
    return fatfs_unlink(path, 0);
}

int hpclang_namespace_rmdir(const char *path)
{
    return fatfs_unlink(path, 1);
}

int hpclang_namespace_mkdir(const char *path)
{
    if (one_shot_begin(path) != 0)
        return -1;

    FRESULT fr = f_mkdir(path);
    int result = fr == FR_OK ? 0 : set_fatfs_errno(fr);
    return result;
}

int hpclang_namespace_rename(const char *old_path, const char *new_path)
{
    if (!path_valid(old_path) || !path_valid(new_path))
        return -1;
    if (namespace_acquire() != 0)
        return -1;

    FRESULT fr = f_rename(old_path, new_path);
    int result = fr == FR_OK ? 0 : set_fatfs_errno(fr);
    return result;
}

struct hpclang_stream_directory *hpclang_stream_directory_open(
    const char *path)
{
    if (!path_valid(path))
        return NULL;
    if (namespace_acquire() != 0)
        return NULL;

    struct hpclang_stream_directory *directory =
        malloc(sizeof(*directory));
    if (directory == NULL) {
        errno = ENOMEM;
        return NULL;
    }
    memset(directory, 0, sizeof(*directory));

    FRESULT fr = f_opendir(&directory->fatfs, path);
    if (fr != FR_OK) {
        free(directory);
        set_fatfs_errno(fr);
        return NULL;
    }

    directory->magic = DIRECTORY_MAGIC;
    return directory;
}

int hpclang_stream_directory_read(
    struct hpclang_stream_directory *directory,
    struct hpclang_stream_directory_entry *entry)
{
    if (directory == NULL || entry == NULL) {
        errno = EFAULT;
        return -1;
    }
    if (directory->magic != DIRECTORY_MAGIC) {
        errno = EBADF;
        return -1;
    }

    FILINFO info;
    FRESULT fr = f_readdir(&directory->fatfs, &info);
    if (fr != FR_OK)
        return set_fatfs_errno(fr);
    if (info.fname[0] == '\0')
        return 0;

    size_t name_length = strlen(info.fname);
    if (name_length >= HPCLANG_STREAM_NAME_CAPACITY) {
        errno = EOVERFLOW;
        return -1;
    }

    memset(entry, 0, sizeof(*entry));
    entry->size = sizeof(*entry);
    entry->version = HPCLANG_STREAM_DIRECTORY_ENTRY_VERSION;
    entry->byte_size = (uint64_t)info.fsize;
    entry->type = export_type(info.fattrib);
    entry->attributes = export_attributes(info.fattrib);
    export_fat_time(&entry->last_modified, info.fdate, info.ftime);
    entry->time_validity = HPCLANG_STREAM_PATH_LAST_MODIFIED_VALID;
    memcpy(entry->name, info.fname, name_length + 1u);
    return 1;
}

int hpclang_stream_directory_close(
    struct hpclang_stream_directory *directory)
{
    if (directory == NULL) {
        errno = EFAULT;
        return -1;
    }
    if (directory->magic != DIRECTORY_MAGIC) {
        errno = EBADF;
        return -1;
    }

    FRESULT fr = f_closedir(&directory->fatfs);
    directory->magic = 0u;
    free(directory);

    return fr == FR_OK ? 0 : set_fatfs_errno(fr);
}

void hpclang_storage_shutdown(void)
{
    if (!namespace_ready ||
        namespace_lease.state != HPCLANG_NAMESPACE_DIRECT)
        return;

    for (uint32_t i = 0u; i < DIRECT_FD_SLOTS; ++i) {
        if (direct_fds[i].used) {
            (void)f_close(&direct_fds[i].file);
            memset(&direct_fds[i], 0, sizeof(direct_fds[i]));
        }
    }

    (void)hpclang_namespace_lease_release(&namespace_lease);
    namespace_media_read_only = 0u;
}
