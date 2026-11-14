#include <hpclang/abi.h>
void hpclang_cache_clean(const void *, unsigned);
int app(void)
{
    const volatile struct hpclang_boot_info *boot = (const void *)HPCLANG_BOOT_BASE;
    volatile unsigned *data = (void *)HPCLANG_DATA_BASE;
    data[0] = boot->model;
    data[1] = boot->rw_bytes;
    data[2] = 0x31435048u;
    hpclang_cache_clean((const void *)data, 12);
    return boot->model == HPCLANG_PROBE_MODEL && boot->rw_bytes == 16384 ? 42 : -1;
}
