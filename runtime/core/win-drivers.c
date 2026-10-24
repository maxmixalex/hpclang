#include "win-drivers.h"
#include <hpclang/time.h>
#include <errno.h>

int hpclang_win_device_clock(void *context, uint64_t *value)
{
    (void)context;
    return hpclang_time_read_us(value);
}
static int ready(struct hpclang_win_drivers *drivers)
{
    if (!drivers || !drivers->loop) {
        errno = EINVAL;
        return 0;
    }
    if (drivers->loop->failure) {
        errno = drivers->loop->failure;
        return 0;
    }
    return 1;
}
int hpclang_win_keyboard_idle(void *event, void *data)
{
    (void)event;
    struct hpclang_win_drivers *drivers = data;
    if (!ready(drivers))
        return -1;
    if (!drivers->keyboard || drivers->keyboard->events != drivers->loop->events)
        return hpclang_win_loop_fail(drivers->loop, EINVAL);
    if (hpclang_win_keyboard_poll(drivers->keyboard))
        return hpclang_win_loop_fail(drivers->loop, errno);
    return 0;
}
int hpclang_win_timer_idle(void *event, void *data)
{
    (void)event;
    struct hpclang_win_drivers *drivers = data;
    if (!ready(drivers))
        return -1;
    if (!drivers->timers || drivers->timers->events != drivers->loop->events)
        return hpclang_win_loop_fail(drivers->loop, EINVAL);
    if (hpclang_win_timers_pump(drivers->timers, drivers->timer_budget) < 0)
        return hpclang_win_loop_fail(drivers->loop, errno);
    return 0;
}
