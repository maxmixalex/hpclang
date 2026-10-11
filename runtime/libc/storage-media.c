#include <errno.h>
#include <stddef.h>
#include <hpclang/device.h>
#include "storage-media.h"
#include "storage-namespace.h"

int hpclang_storage_media_query(int *present, int *read_only)
{
    const struct hpclang_device_info *device = hpclang_device_info();
    if (!device || device->family != HPCLANG_FAMILY_HP4950) {
        errno = ENOSYS;
        return -1;
    }
    if (!present || !read_only) {
        errno = EFAULT;
        return -1;
    }
    if (!hpclang_namespace_stock_active() && !hpclang_namespace_direct_active()) {
        errno = EBUSY;
        return -1;
    }
    /* GPF3 is card detect (active high), GPD3 is write protect (active low).
     * Read Stock's configured inputs without re-entering KOS CHECKINSERT or
     * changing pin functions, interrupt routing or pull-ups. */
    unsigned detect_mode = (hpclang_storage_media_read32(HPCLANG_STORAGE_GPFCON) >> 6) & 3u;
    if (detect_mode != 0u && detect_mode != 2u) {
        errno = ENOTSUP;
        return -1;
    }
    int inserted = (hpclang_storage_media_read32(HPCLANG_STORAGE_GPFDAT) & 8u) != 0;
    int write_protected = 0;
    if (inserted) {
        if ((hpclang_storage_media_read32(HPCLANG_STORAGE_GPDCON) & 0xc0u) != 0) {
            errno = ENOTSUP;
            return -1;
        }
        write_protected = (hpclang_storage_media_read32(HPCLANG_STORAGE_GPDDAT) & 8u) == 0;
    }
    *present = inserted;
    *read_only = write_protected;
    return 0;
}

int hpclang_storage_stock_media_query(int *present, int *read_only)
{
    if (!hpclang_namespace_stock_active()) {
        errno = EBUSY;
        return -1;
    }
    return hpclang_storage_media_query(present, read_only);
}
