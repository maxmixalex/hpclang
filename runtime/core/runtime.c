#include <hpclang/abi.h>

static const struct hpclang_boot_info *boot_info;

extern unsigned char __tls_base[];

void _set_tls(void *tls);
void __libc_init_array(void);
void __libc_fini_array(void);
void hpclang_time_init(void);
void hpclang_storage_shutdown(void);
void hpclang_display_end(void);
void hpclang_power_restore(void);

int main(void);

int hpclang_start(const struct hpclang_boot_info *boot)
{
    boot_info = boot;

    _set_tls(__tls_base);
    hpclang_time_init();
    __libc_init_array();

    int result = main();

    __libc_fini_array();
    hpclang_display_end();
    hpclang_power_restore();
    hpclang_storage_shutdown();
    return result;
}

const struct hpclang_boot_info *hpclang_boot_info(void)
{
    return boot_info;
}
