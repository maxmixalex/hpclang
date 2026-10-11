#include <hpclang/abi.h>
#include <setjmp.h>
#include <stdlib.h>
#include "clock-internal.h"

static const struct hpclang_boot_info *boot_info;
static jmp_buf exit_context;
static int exit_status;

extern unsigned char __tls_base[];

void _set_tls(void *tls);
void __libc_init_array(void);
void hpclang_time_init(void);
void hpclang_storage_shutdown(void);
void hpclang_display_end(void);

int main(void);

int hpclang_start(const struct hpclang_boot_info *boot)
{
    boot_info = boot;

    _set_tls(__tls_base);
    hpclang_time_init();
    /* Picolibc exit runs its fini array (including atexit callbacks). _Exit
     * skips that array. Both reach _exit and leave through this boundary, so
     * hardware cleanup and the launcher's ordinary return path stay shared.
     * Capture before constructors, which may themselves terminate the app. */
    if (setjmp(exit_context) == 0) {
        __libc_init_array();
        exit(main());
    }
    hpclang_sound_restore();
    hpclang_display_end();
    hpclang_power_restore();
    hpclang_storage_shutdown();
    return exit_status;
}

__attribute__((noreturn)) void _exit(int status)
{
    /* Keep status separately: longjmp maps a zero argument to one. */
    exit_status = status;
    longjmp(exit_context, 1);
}

const struct hpclang_boot_info *hpclang_boot_info(void)
{
    return boot_info;
}
