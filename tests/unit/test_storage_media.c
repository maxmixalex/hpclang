#include <assert.h>
#include <errno.h>
#include <stddef.h>
#include <hpclang/device.h>
#include "storage-media.h"
#include "storage-namespace.h"

static struct hpclang_device_info device = {.family = HPCLANG_FAMILY_HP4950};
static int stock = 1;
static uint32_t gpfcon = 0x80u, gpfdat, gpdcon, gpddat = 8u;
static unsigned reads, protect_reads;

const struct hpclang_device_info *hpclang_device_info(void)
{
    return &device;
}
int hpclang_namespace_stock_active(void)
{
    return stock;
}
uint32_t hpclang_storage_media_read32(unsigned offset)
{
    ++reads;
    switch (offset) {
    case HPCLANG_STORAGE_GPFCON:
        return gpfcon;
    case HPCLANG_STORAGE_GPFDAT:
        return gpfdat;
    case HPCLANG_STORAGE_GPDCON:
        return gpdcon;
    case HPCLANG_STORAGE_GPDDAT:
        ++protect_reads;
        return gpddat;
    default:
        assert(0);
        return 0;
    }
}

static void fails(int error)
{
    int present = 7, read_only = 9;
    assert(hpclang_storage_stock_media_query(&present, &read_only) == -1 && errno == error);
    assert(present == 7 && read_only == 9);
}

int main(void)
{
    int present = 7, read_only = 9;
    assert(hpclang_storage_stock_media_query(&present, &read_only) == 0);
    assert(present == 0 && read_only == 0 && protect_reads == 0);
    gpfdat = 8;
    assert(hpclang_storage_stock_media_query(&present, &read_only) == 0);
    assert(present == 1 && read_only == 0 && protect_reads == 1);
    gpddat = 0;
    assert(hpclang_storage_stock_media_query(&present, &read_only) == 0);
    assert(present == 1 && read_only == 1);
    gpddat = 8;
    assert(hpclang_storage_stock_media_query(&present, &read_only) == 0 && read_only == 0);
    gpfdat = 0;
    unsigned probes = protect_reads;
    assert(hpclang_storage_stock_media_query(&present, &read_only) == 0 && present == 0);
    assert(protect_reads == probes);
    gpfdat = 8;
    gpfcon = 0x40;
    fails(ENOTSUP);
    gpfcon = 0xc0;
    fails(ENOTSUP);
    gpfcon = 0;
    gpdcon = 0x40;
    fails(ENOTSUP);
    gpdcon = 0;
    assert(hpclang_storage_stock_media_query(&present, &read_only) == 0 && present == 1);
    unsigned calls = reads;
    stock = 0;
    fails(EBUSY);
    stock = 1;
    device.family = HPCLANG_FAMILY_HP39;
    fails(ENOSYS);
    device.family = HPCLANG_FAMILY_HP4950;
    assert(hpclang_storage_stock_media_query(NULL, &read_only) == -1 && errno == EFAULT);
    assert(hpclang_storage_stock_media_query(&present, NULL) == -1 && errno == EFAULT);
    assert(reads == calls);
    return 0;
}
