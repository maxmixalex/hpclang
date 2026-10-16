#include <errno.h>
#include <hpsys.h>
#include <hpclang/device.h>
#include "rtc-tick.h"
#include "irq.h"
#include "exit-hooks.h"

static struct hpclang_rtc_tick_saved saved;
static unsigned char period = 15;

static void restore_tick(void)
{
    uint32_t irq = hpclang_irq_save_disable();
    hpclang_rtc_tick_release(&hpclang_rtc_tick_bus, &saved);
    hpclang_exit_hooks.rtc_tick = 0;
    period = 15;
    hpclang_irq_restore(irq);
}

static int acquire(void)
{
    if (hpclang_device_info()->family == HPCLANG_FAMILY_UNKNOWN) {
        errno = ENOSYS;
        return -1;
    }
    if (!saved.owned) {
        if (hpclang_rtc_tick_acquire(&hpclang_rtc_tick_bus, &saved, period))
            return -1;
        hpclang_exit_hooks.rtc_tick = restore_tick;
    }
    return 0;
}

unsigned char sys_setRTCTickPeriod(unsigned char selector)
{
    if (!selector || selector > 127)
        return period;
    uint32_t irq = hpclang_irq_save_disable();
    unsigned char previous = period;
    if (acquire() || hpclang_rtc_tick_set(&hpclang_rtc_tick_bus, &saved, selector))
        previous = 0;
    else
        period = selector;
    hpclang_irq_restore(irq);
    return previous;
}

int sys_isRTCTick(void)
{
    uint32_t irq = hpclang_irq_save_disable();
    int result = acquire();
    if (!result)
        result = hpclang_rtc_tick_poll(&hpclang_rtc_tick_bus, &saved);
    hpclang_irq_restore(irq);
    return result;
}

void sys_waitRTCTicks(int count)
{
    while (count > 0) {
        int result = sys_isRTCTick();
        if (result < 0)
            return;
        count -= result;
    }
}
