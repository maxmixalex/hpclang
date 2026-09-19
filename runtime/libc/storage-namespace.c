#include <errno.h>
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

struct hpclang_stream_directory {
    DIR fatfs;
    uint32_t magic;
    uint8_t lease_ref;
};

#define DIRECTORY_MAGIC UINT32_C(0x48445031)

static FATFS namespace_fatfs;
static struct hpclang_namespace_lease namespace_lease;
static uint32_t namespace_refs;
static uint8_t namespace_ready;

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
    (void)context;
    return hpclang_storage_stock_idle();
}

static int lease_direct_acquire(void *context)
{
    (void)context;
    return hpclang_hp4950_fatfs_controller_acquire(0);
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

static int namespace_ref_acquire(void)
{
    const struct hpclang_device_info *device = hpclang_device_info();
    if (device == NULL || device->family != HPCLANG_FAMILY_HP4950) {
        errno = ENOSYS;
        return -1;
    }

    namespace_init();

    if (namespace_lease.state == HPCLANG_NAMESPACE_DIRECT) {
        ++namespace_refs;
        return 0;
    }
    if (namespace_lease.state != HPCLANG_NAMESPACE_STOCK) {
        errno = EIO;
        return -1;
    }

    int result = hpclang_namespace_lease_acquire(&namespace_lease);
    if (result != HPCLANG_NAMESPACE_LEASE_OK) {
        switch (result) {
        case HPCLANG_NAMESPACE_LEASE_BUSY:
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

    namespace_refs = 1u;
    return 0;
}

static int namespace_ref_release(void)
{
    if (!namespace_ready ||
        namespace_lease.state != HPCLANG_NAMESPACE_DIRECT ||
        namespace_refs == 0u) {
        errno = EIO;
        return -1;
    }

    --namespace_refs;
    if (namespace_refs != 0u)
        return 0;

    int result = hpclang_namespace_lease_release(&namespace_lease);
    if (result != HPCLANG_NAMESPACE_LEASE_OK) {
        errno = EIO;
        return -1;
    }
    return 0;
}

int hpclang_namespace_direct_active(void)
{
    return namespace_ready &&
           namespace_lease.state == HPCLANG_NAMESPACE_DIRECT;
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
    if ((attributes & AM_VOL) != 0u)
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
    return namespace_ref_acquire();
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

    int release_result = namespace_ref_release();
    return result != 0 ? result : release_result;
}

int hpclang_namespace_storage_query(
    struct hpclang_stream_storage_info *info)
{
    if (info == NULL) {
        errno = EFAULT;
        return -1;
    }
    if (namespace_ref_acquire() != 0)
        return -1;

    memset(info, 0, sizeof(*info));
    info->size = sizeof(*info);
    info->version = HPCLANG_STREAM_STORAGE_INFO_VERSION;
    info->flags = HPCLANG_STREAM_STORAGE_MEDIA_PRESENT;

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

        LBA_t sectors = 0;
        if (disk_ioctl(0u, GET_SECTOR_COUNT, &sectors) == RES_OK) {
            info->capacity_bytes = (uint64_t)sectors * UINT64_C(512);
            info->flags |= HPCLANG_STREAM_STORAGE_CAPACITY_BYTES_VALID;
        }
    }

    int release_result = namespace_ref_release();
    return result != 0 ? result : release_result;
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

    int release_result = namespace_ref_release();
    return result != 0 ? result : release_result;
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
    int release_result = namespace_ref_release();
    return result != 0 ? result : release_result;
}

int hpclang_namespace_rename(const char *old_path, const char *new_path)
{
    if (!path_valid(old_path) || !path_valid(new_path))
        return -1;
    if (namespace_ref_acquire() != 0)
        return -1;

    FRESULT fr = f_rename(old_path, new_path);
    int result = fr == FR_OK ? 0 : set_fatfs_errno(fr);
    int release_result = namespace_ref_release();
    return result != 0 ? result : release_result;
}

struct hpclang_stream_directory *hpclang_stream_directory_open(
    const char *path)
{
    if (!path_valid(path))
        return NULL;
    if (namespace_ref_acquire() != 0)
        return NULL;

    struct hpclang_stream_directory *directory =
        malloc(sizeof(*directory));
    if (directory == NULL) {
        (void)namespace_ref_release();
        errno = ENOMEM;
        return NULL;
    }
    memset(directory, 0, sizeof(*directory));

    FRESULT fr = f_opendir(&directory->fatfs, path);
    if (fr != FR_OK) {
        free(directory);
        (void)namespace_ref_release();
        set_fatfs_errno(fr);
        return NULL;
    }

    directory->magic = DIRECTORY_MAGIC;
    directory->lease_ref = 1u;
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
    if (directory->magic != DIRECTORY_MAGIC || !directory->lease_ref) {
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
    if (directory->magic != DIRECTORY_MAGIC || !directory->lease_ref) {
        errno = EBADF;
        return -1;
    }

    FRESULT fr = f_closedir(&directory->fatfs);
    directory->lease_ref = 0u;
    directory->magic = 0u;
    free(directory);

    int result = fr == FR_OK ? 0 : set_fatfs_errno(fr);
    int release_result = namespace_ref_release();
    return result != 0 ? result : release_result;
}

void hpclang_storage_shutdown(void)
{
    if (!namespace_ready ||
        namespace_lease.state != HPCLANG_NAMESPACE_DIRECT)
        return;

    namespace_refs = 1u;
    (void)namespace_ref_release();
}
