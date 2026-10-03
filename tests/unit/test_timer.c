#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stddef.h>
#include "timer-engine.h"

static int clock_ok = 1;
static uint64_t clock_now;
int hpclang_time_read_us(uint64_t *now)
{
    if (!clock_ok) {
        errno = EIO;
        return -1;
    }
    *now = clock_now;
    return 0;
}

int main(void)
{
    struct hpclang_timer timer = {0};
    uint64_t count = 99;
    assert(hpclang_timer_poll(&timer, &count) == 0 && count == 0);
    clock_now = 100;
    assert(hpclang_timer_start(&timer, 20, 10) == 0);
    assert(hpclang_timer_poll_at(&timer, 119, &count) == 0 && count == 0);
    assert(hpclang_timer_poll_at(&timer, 120, &count) == 0 && count == 1);
    assert(timer.deadline_us == 130);
    assert(hpclang_timer_poll_at(&timer, 157, &count) == 0 && count == 3);
    assert(timer.deadline_us == 160);
    assert(hpclang_timer_poll_at(&timer, 157, &count) == 0 && count == 0);

    struct hpclang_timer saved = timer;
    clock_ok = 0;
    count = 99;
    assert(hpclang_timer_start(&timer, 0, 0) == -1 && errno == EIO);
    assert(hpclang_timer_poll(&timer, &count) == -1 && errno == EIO && count == 99);
    assert(saved.deadline_us == timer.deadline_us && saved.period_us == timer.period_us &&
           saved.armed == timer.armed);
    assert(hpclang_timer_start_at(&timer, UINT64_MAX, 1, 0) == -1 && errno == EOVERFLOW);
    assert(saved.deadline_us == timer.deadline_us && saved.period_us == timer.period_us &&
           saved.armed == timer.armed);
    hpclang_timer_cancel(&timer);
    assert(hpclang_timer_poll(&timer, &count) == 0 && count == 0);
    clock_ok = 1;
    assert(hpclang_timer_start(&timer, 0, 0) == 0);
    assert(hpclang_timer_poll(&timer, &count) == 0 && count == 1);
    assert(hpclang_timer_poll(&timer, &count) == 0 && count == 0);
    assert(hpclang_timer_start_at(&timer, 0, 0, 1) == 0);
    saved = timer;
    count = 99;
    assert(hpclang_timer_poll_at(&timer, UINT64_MAX, &count) == -1 && errno == EOVERFLOW);
    assert(saved.deadline_us == timer.deadline_us && saved.period_us == timer.period_us &&
           saved.armed == timer.armed && count == 99);
    assert(hpclang_timer_start_at(&timer, UINT64_MAX, 0, 0) == 0);
    assert(hpclang_timer_poll_at(&timer, UINT64_MAX, &count) == 0 && count == 1);
    assert(hpclang_timer_poll(NULL, &count) == -1 && errno == EINVAL);
    assert(hpclang_timer_poll(&timer, NULL) == -1 && errno == EINVAL);
    assert(hpclang_timer_start(NULL, 0, 0) == -1 && errno == EINVAL);
    hpclang_timer_cancel(NULL);
    return 0;
}
