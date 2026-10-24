#include "win-loop.h"
#include "win-timers.h"
#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>

static struct hpclang_win_events events;
static struct hpclang_win_handler handlers[8];
static struct hpclang_win_call snapshots[16];
static struct hpclang_win_loop loop;
static struct hpclang_win_timers timers;
static struct hpclang_win_timer slots[2];
static uint64_t now;
static char trace[64];
static size_t used;
static int nested, consumed;
static void record(char c)
{
    assert(used + 1 < sizeof(trace));
    trace[used++] = c;
    trace[used] = 0;
}
static int clock_read(void *data, uint64_t *value)
{
    (void)data;
    *value = now;
    return 0;
}
static int key(void *data, void *context)
{
    assert(!data && context == &loop);
    record('K');
    ++now;
    return consumed;
}
static int repaint(void *data, void *context)
{
    assert(!data && context == &loop);
    record('R');
    return 0;
}
static int timer_idle(void *data, void *context)
{
    assert(!data && context == &loop);
    record('T');
    assert(hpclang_win_timers_pump(&timers, SIZE_MAX) == 0);
    return 0;
}
static int finish(void *context)
{
    assert(context == &loop);
    record('Q');
    assert(!hpclang_win_loop_quit(&loop));
    return 1;
}
static int tail(void *data, void *context)
{
    assert(!data && context == &loop);
    record('A');
    return 0;
}
static int recurse(void *data, void *context)
{
    assert(!data && context == &loop);
    record('N');
    if (!nested) {
        nested = 1;
        assert(!hpclang_win_loop_run(&loop, 8));
        nested = 0;
    } else {
        assert(!hpclang_win_loop_quit(&loop));
    }
    return 0;
}
static void init(size_t snapshot_count)
{
    now = used = 0;
    nested = consumed = 0;
    trace[0] = 0;
    assert(!hpclang_win_events_init(&events, handlers, 8, snapshots, snapshot_count));
    assert(!hpclang_win_loop_init(&loop, &events, 0));
    assert(!hpclang_win_timers_init(&timers, slots, 2, clock_read, NULL, &events));
}
static void add(int priority, hpclang_win_event_fn function)
{
    assert(!hpclang_win_events_add(&events, 0, priority, function, &loop));
}
int main(void)
{
    init(16);
    add(2000000002, key);
    add(2000000001, repaint);
    add(2000000000, timer_idle);
    add(0, tail);
    assert(!hpclang_win_timers_add(&timers, finish, &loop, 3, 0));
    assert(!hpclang_win_loop_quit(&loop)); /* Run clears a prior quit. */
    assert(hpclang_win_loop_run(&loop, 2) == 1);
    assert(!strcmp(trace, "KRTAKRTA") && timers.head);
    assert(!hpclang_win_loop_run(&loop, 2));
    assert(!strcmp(trace, "KRTAKRTAKRTQA") && !timers.head);

    init(16);
    add(2, key);
    add(1, tail);
    consumed = -7; /* Consuming idle is not a quit request. */
    assert(hpclang_win_loop_run(&loop, 3) == 1);
    assert(!strcmp(trace, "KKK"));
    assert(hpclang_win_loop_run(&loop, 0) == 1 && now == 3);

    init(16);
    add(1, recurse);
    add(0, tail);
    assert(!hpclang_win_loop_run(&loop, 8));
    assert(!strcmp(trace, "NNAA") && !events.depth && !events.used);

    init(1);
    add(1, key);
    add(0, tail);
    assert(hpclang_win_loop_run(&loop, 8) == -1 && errno == ENOSPC);
    assert(!used && !events.depth && !events.used);
    assert(!hpclang_win_events_remove(&events, 0, tail, &loop));
    assert(hpclang_win_loop_run(&loop, 1) == 1 && !strcmp(trace, "K"));
    assert(hpclang_win_loop_run(NULL, 1) == -1 && errno == EINVAL);
    return 0;
}
