#include <errno.h>
#include <hpclang/time.h>
#include "timer-engine.h"

int hpclang_timer_start(struct hpclang_timer *timer, uint64_t delay_us, uint64_t period_us)
{
    if (!timer) {
        errno = EINVAL;
        return -1;
    }
    uint64_t now;
    if (hpclang_time_read_us(&now) < 0)
        return -1;
    return hpclang_timer_start_at(timer, now, delay_us, period_us);
}

int hpclang_timer_poll(struct hpclang_timer *timer, uint64_t *count)
{
    if (!timer || !count) {
        errno = EINVAL;
        return -1;
    }
    uint64_t now = 0;
    if (timer->armed && hpclang_time_read_us(&now) < 0)
        return -1;
    return hpclang_timer_poll_at(timer, now, count);
}
