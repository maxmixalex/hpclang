#include <stdint.h>
#include <hpclang/abi.h>

volatile uint32_t hpclang_smoke_data = 7;
volatile uint32_t hpclang_smoke_bss;

static const char banner[] = "hpclang-v1";

int main(void)
{
    const struct hpclang_boot_info *boot = hpclang_boot_info();

    hpclang_smoke_bss = hpclang_smoke_data + (uint32_t)banner[0];

    if (boot != (const struct hpclang_boot_info *)HPCLANG_BOOT_BASE)
        return 1;

    return hpclang_smoke_bss == 111u ? 0 : 2;
}
