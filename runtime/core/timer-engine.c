#include <errno.h>
#include "timer-engine.h"

int hpclang_timer_start_at(struct hpclang_timer *timer, uint64_t now, uint64_t delay,
                           uint64_t period)
{
    if (!timer) {
        errno = EINVAL;
        return -1;
    }
    if (delay > UINT64_MAX - now) {
        errno = EOVERFLOW;
        return -1;
    }
    *timer = (struct hpclang_timer){now + delay, period, 1};
    return 0;
}

int hpclang_timer_poll_at(struct hpclang_timer *timer, uint64_t now, uint64_t *count)
{
    if (!timer || !count) {
        errno = EINVAL;
        return -1;
    }
    if (!timer->armed || now < timer->deadline_us) {
        *count = 0;
        return 0;
    }
    if (!timer->period_us) {
        timer->armed = 0;
        *count = 1;
        return 0;
    }
    uint64_t missed = (now - timer->deadline_us) / timer->period_us;
    if (missed == UINT64_MAX || missed + 1 > (UINT64_MAX - timer->deadline_us) / timer->period_us) {
        errno = EOVERFLOW;
        return -1;
    }
    timer->deadline_us += (missed + 1) * timer->period_us;
    *count = missed + 1;
    return 0;
}

void hpclang_timer_cancel(struct hpclang_timer *timer)
{
    if (timer)
        timer->armed = 0;
}
