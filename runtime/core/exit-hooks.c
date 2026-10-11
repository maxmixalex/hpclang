#include "exit-hooks.h"
#include "clock-internal.h"
#include "../libc/stdio-lifetime.h"

struct hpclang_exit_hooks hpclang_exit_hooks;

void hpclang_power_restore(void)
{
    if (hpclang_exit_hooks.power)
        hpclang_exit_hooks.power();
}

void hpclang_stdio_exit_cleanup(void)
{
    if (hpclang_exit_hooks.stdio)
        hpclang_exit_hooks.stdio();
}
