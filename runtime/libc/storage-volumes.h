#ifndef HPCLANG_STORAGE_VOLUMES_H
#define HPCLANG_STORAGE_VOLUMES_H

#include "../../third_party/fatfs/ff.h"

#define HPCLANG_STORAGE_VOLUME_LIMIT 4u

struct hpclang_storage_volumes
{
    FATFS filesystems[HPCLANG_STORAGE_VOLUME_LIMIT];
    BYTE partitions[HPCLANG_STORAGE_VOLUME_LIMIT];
    unsigned count;
};

/* Dense logical volumes in MBR order; a superfloppy has one volume, partition 0.
 * Volume zero preserves the existing first-usable-FAT default. */
FRESULT hpclang_storage_volumes_mount(struct hpclang_storage_volumes *volumes);
void hpclang_storage_volumes_unmount(struct hpclang_storage_volumes *volumes);
int hpclang_storage_volume_object_valid(const struct hpclang_storage_volumes *volumes,
                                        const FFOBJID *object);

#endif
