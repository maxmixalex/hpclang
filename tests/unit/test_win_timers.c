#include "win-timers.h"
#include <assert.h>
#include <errno.h>

static struct hpclang_win_timers timers;
static struct hpclang_win_timer slots[2];
static uint64_t now;
static int clock_failure, calls, stop_at, fail_after, backwards, try_add;
static int read_clock(void *context, uint64_t *value)
{
    (void)context;
    if (clock_failure) {
        errno = EIO;
        return -1;
    }
    *value = now;
    return 0;
}
static int callback(void *data)
{
    (void)data;
    ++calls;
    assert(hpclang_win_timers_reset(&timers) == -1 && errno == EBUSY);
    if (try_add)
        assert(hpclang_win_timers_add(&timers, callback, NULL, 1, 1) == -1 && errno == ENOSPC);
    if (fail_after)
        clock_failure = 1;
    if (backwards)
        --now;
    return calls >= stop_at;
}
static void init(size_t count)
{
    now = 0;
    clock_failure = calls = fail_after = backwards = try_add = 0;
    stop_at = 1;
    assert(!hpclang_win_timers_init(&timers, slots, count, read_clock, NULL, NULL));
}
static void budgets(void)
{
    init(1);
    stop_at = 4;
    try_add = 1;
    assert(!hpclang_win_timers_add(&timers, callback, NULL, 1, 1));
    assert(hpclang_win_timers_add(&timers, callback, NULL, 1, 1) == -1 && errno == ENOSPC);
    now = 10;
    assert(hpclang_win_timers_pump(&timers, 2) == 1 && calls == 2);
    assert(timers.head->deadline == 3 && !timers.depth);
    assert(hpclang_win_timers_pump(&timers, 0) == 1 && calls == 2);
    assert(!hpclang_win_timers_pump(&timers, 2) && calls == 4 && !timers.head);
    assert(!slots[0].state && !timers.depth);
}
static void failures(void)
{
    init(2);
    assert(hpclang_win_timers_add(&timers, callback, NULL, 0, 1) == -1 && errno == EINVAL);
    assert(hpclang_win_timers_add(&timers, NULL, NULL, 1, 1) == -1 && errno == EINVAL);
    assert(hpclang_win_timers_add_event(&timers, 0, NULL, 1, 1) == -1 && errno == ENOSYS);
    now = UINT64_MAX - 1;
    assert(hpclang_win_timers_add(&timers, callback, NULL, 2, 1) == -1 && errno == EOVERFLOW);
    assert(!timers.head && !slots[0].state);

    init(2);
    assert(!hpclang_win_timers_add(&timers, callback, NULL, 1, 1));
    now = 1;
    clock_failure = 1;
    assert(hpclang_win_timers_pump(&timers, 1) == -1 && errno == EIO);
    assert(!calls && timers.head && slots[0].state == 1);
    clock_failure = 0;
    assert(!hpclang_win_timers_pump(&timers, 1) && calls == 1 && !timers.head);

    init(2);
    stop_at = 10;
    fail_after = 1;
    assert(!hpclang_win_timers_add(&timers, callback, NULL, 1, 0));
    assert(!hpclang_win_timers_add(&timers, callback, &calls, 3, 0));
    now = 1;
    assert(hpclang_win_timers_pump(&timers, 1) == -1 && errno == EIO);
    assert(calls == 1 && !slots[0].state && timers.head == slots + 1 && !timers.depth);
    clock_failure = 0;
    assert(!hpclang_win_timers_pump(&timers, 1) && calls == 1);

    init(1);
    stop_at = 10;
    backwards = 1;
    assert(!hpclang_win_timers_add(&timers, callback, NULL, 1, 1));
    now = 2;
    assert(hpclang_win_timers_pump(&timers, 1) == -1 && errno == EIO);
    assert(calls == 1 && !timers.head && !slots[0].state);

    init(2);
    stop_at = 10;
    now = UINT64_MAX - 2;
    assert(!hpclang_win_timers_add(&timers, callback, NULL, 1, 0));
    assert(!hpclang_win_timers_add(&timers, callback, &calls, 2, 1));
    now = UINT64_MAX;
    assert(hpclang_win_timers_pump(&timers, 1) == -1 && errno == EOVERFLOW);
    assert(calls == 1 && !slots[0].state && timers.head == slots + 1 && !timers.depth);
}
static int event_callback(void *data, void *context)
{
    (void)data;
    (void)context;
    ++calls;
    return 9;
}
static void event_retry(void)
{
    init(1);
    struct hpclang_win_events events;
    struct hpclang_win_handler handlers[2];
    struct hpclang_win_call snapshots[1];
    assert(!hpclang_win_events_init(&events, handlers, 2, snapshots, 1));
    assert(!hpclang_win_events_add(&events, 0, 0, event_callback, NULL));
    assert(!hpclang_win_events_add(&events, 0, 0, event_callback, NULL));
    timers.events = &events;
    assert(!hpclang_win_timers_add_event(&timers, 0, NULL, 1, 1));
    now = 1;
    assert(hpclang_win_timers_pump(&timers, 1) == -1 && errno == ENOSPC);
    assert(!calls && timers.head->deadline == 1 && slots[0].state == 1 && !timers.depth);
    assert(!hpclang_win_events_remove(&events, 0, event_callback, NULL));
    assert(!hpclang_win_timers_pump(&timers, 1) && calls == 1 && !timers.head);
    assert(!events.used && !events.depth);
    timers.events = NULL;
}
int main(void)
{
    budgets();
    failures();
    event_retry();
    return 0;
}
