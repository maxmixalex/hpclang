#include <hpclang/abi.h>

static const struct hpclang_boot_info *boot_info;

int main(void);

int hpclang_start(const struct hpclang_boot_info *boot)
{
    boot_info = boot;
    return main();
}

const struct hpclang_boot_info *hpclang_boot_info(void)
{
    return boot_info;
}
