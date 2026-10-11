#include <stdint.h>
#include <string.h>
#include "storage-volumes.h"
#include "../../third_party/fatfs/diskio.h"

_Static_assert(FF_VOLUMES == HPCLANG_STORAGE_VOLUME_LIMIT, "logical volume limit mismatch");
PARTITION VolToPart[FF_VOLUMES];

static uint32_t little32(const BYTE *bytes)
{
    return (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8) | ((uint32_t)bytes[2] << 16) |
           ((uint32_t)bytes[3] << 24);
}

void hpclang_storage_volumes_unmount(struct hpclang_storage_volumes *volumes)
{
    for (unsigned i = 0; i < HPCLANG_STORAGE_VOLUME_LIMIT; ++i) {
        char root[] = "0:";
        root[0] += (char)i;
        (void)f_mount(NULL, root, 0);
    }
    memset(volumes, 0, sizeof(*volumes));
}

static int fits(const FATFS *fs, uint32_t start, uint32_t sectors)
{
    if (fs->volbase != start || fs->database < start || fs->n_fatent < 2 || !fs->csize)
        return 0;
    uint64_t size = (uint64_t)(fs->database - start) + (uint64_t)(fs->n_fatent - 2u) * fs->csize;
    return size <= sectors;
}

FRESULT hpclang_storage_volumes_mount(struct hpclang_storage_volumes *volumes)
{
    hpclang_storage_volumes_unmount(volumes);
    memset(VolToPart, 0, sizeof VolToPart);
    FRESULT result = f_mount(&volumes->filesystems[0], "0:", 1);
    if (result != FR_OK && result != FR_NO_FILESYSTEM)
        goto fail;
    if (result == FR_OK && !volumes->filesystems[0].volbase) {
        volumes->count = 1;
        return FR_OK;
    }
    (void)f_mount(NULL, "0:", 0);

    BYTE sector[512];
    LBA_t capacity;
    if (disk_read(0, sector, 0, 1) != RES_OK ||
        disk_ioctl(0, GET_SECTOR_COUNT, &capacity) != RES_OK) {
        result = FR_DISK_ERR;
        goto fail;
    }
    if (sector[510] != 0x55 || sector[511] != 0xaa) {
        result = FR_NO_FILESYSTEM;
        goto fail;
    }
    uint32_t starts[4], sizes[4];
    for (unsigned i = 0; i < 4; ++i) {
        const BYTE *entry = sector + 446 + 16 * i;
        starts[i] = entry[4] ? little32(entry + 8) : 0;
        sizes[i] = entry[4] ? little32(entry + 12) : 0;
        if (entry[4] && (!starts[i] || !sizes[i] || (uint64_t)starts[i] + sizes[i] > capacity)) {
            result = FR_NO_FILESYSTEM;
            goto fail;
        }
        for (unsigned j = 0; j < i; ++j)
            if (sizes[i] && sizes[j] && starts[i] < (uint64_t)starts[j] + sizes[j] &&
                starts[j] < (uint64_t)starts[i] + sizes[i]) {
                result = FR_NO_FILESYSTEM;
                goto fail;
            }
    }
    for (unsigned partition = 0; partition < 4; ++partition) {
        if (!sizes[partition])
            continue;
        unsigned index = volumes->count;
        char root[] = "0:";
        root[0] += (char)index;
        VolToPart[index].pt = (BYTE)(partition + 1);
        result = f_mount(&volumes->filesystems[index], root, 1);
        if (result == FR_NO_FILESYSTEM) {
            (void)f_mount(NULL, root, 0);
            continue;
        }
        if (result != FR_OK)
            goto fail;
        if (!fits(&volumes->filesystems[index], starts[partition], sizes[partition])) {
            result = FR_NO_FILESYSTEM;
            goto fail;
        }
        volumes->partitions[index] = (BYTE)(partition + 1);
        ++volumes->count;
    }
    if (volumes->count)
        return FR_OK;
    result = FR_NO_FILESYSTEM;
fail:
    hpclang_storage_volumes_unmount(volumes);
    return result;
}

int hpclang_storage_volume_object_valid(const struct hpclang_storage_volumes *volumes,
                                        const FFOBJID *object)
{
    for (unsigned i = 0; i < volumes->count; ++i)
        if (object->fs == &volumes->filesystems[i])
            return object->fs->fs_type && object->id == object->fs->id;
    return 0;
}
