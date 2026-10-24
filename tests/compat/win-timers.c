#include "winpriv.h"
#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

struct win_eventqueue _win_sysqueue;
static struct win_eventhandler *rows[EQ_LEV1SIZE][EQ_LEV2SIZE];
static uint64_t now, times[16];
static int ids[16], used;
struct task
{
    int id, calls, stop, remove_self;
    uint64_t cost;
};

void win_init(void)
{
    for (unsigned i = 0; i < EQ_LEV1SIZE; ++i)
        _win_sysqueue.handler_chains[i] = rows[i];
}
void sys_updateTimer(sys_timer_t *timer)
{
    timer->current = now;
}
static void mark(int id)
{
    assert(used < 16);
    ids[used] = id;
    times[used++] = now;
    printf("timer %d %" PRIu64 "\n", id, now);
}
static int run(void *context)
{
    struct task *task = context;
    mark(task->id);
    ++task->calls;
    now += task->cost;
    if (task->remove_self)
        win_remove_timer(run, task);
    return task->calls >= task->stop;
}
static void clear(void)
{
    assert(!_win_timer_list);
    now = 0;
    used = 0;
}
static void pump(uint64_t milliseconds)
{
    now = milliseconds * UINT64_C(1000000);
    assert(!_win_timer_generator(NULL, NULL));
}
static void expect(int index, int id, uint64_t milliseconds)
{
    assert(index < used && ids[index] == id && times[index] == milliseconds * UINT64_C(1000000));
}
static int event_stop(void *data, void *context)
{
    assert(data == context);
    mark(*(int *)data);
    return -17;
}
static int nested(void *context)
{
    (void)context;
    mark(9);
    assert(!_win_timer_generator(NULL, NULL));
    return 1;
}

int main(void)
{
    clear();
    struct task periodic = {1, 0, 3, 0, 6000000};
    win_add_timer(run, &periodic, 10, 1);
    pump(35);
    assert(used == 3);
    expect(0, 1, 35);
    expect(1, 1, 41);
    expect(2, 1, 47);

    clear();
    struct task delay = {2, 0, 3, 0, 6000000};
    win_add_timer(run, &delay, 10, 0);
    pump(35);
    assert(used == 1 && _win_timer_list->next_time == UINT64_C(51000000));
    pump(50);
    assert(used == 1);
    pump(51);
    assert(used == 2 && _win_timer_list->next_time == UINT64_C(67000000));
    pump(67);
    assert(used == 3);
    expect(0, 2, 35);
    expect(1, 2, 51);
    expect(2, 2, 67);

    clear();
    struct task a = {3, 0, 1, 0, 0}, b = {4, 0, 1, 0, 0};
    win_add_timer(run, &a, 10, 1);
    win_add_timer(run, &b, 10, 1);
    pump(10);
    assert(used == 2);
    expect(0, 4, 10);
    expect(1, 3, 10);

    clear();
    struct task duplicate = {5, 0, 1, 0, 0};
    win_add_timer(run, &duplicate, 10, 1);
    win_add_timer(run, &duplicate, 20, 1);
    win_remove_timer(run, &duplicate);
    pump(10);
    assert(!used);
    pump(20);
    assert(used == 1);
    expect(0, 5, 20);

    clear();
    struct task self = {6, 0, 2, 1, 0};
    win_add_timer(run, &self, 10, 1);
    pump(10);
    assert(used == 1 && _win_timer_list);
    pump(20);
    assert(used == 2);
    expect(0, 6, 10);
    expect(1, 6, 20);

    clear();
    int event_data = 7;
    win_add_event_handler(3, 0, event_stop, &event_data);
    win_add_timer_event(3, &event_data, 10, 1);
    pump(10);
    assert(used == 1);
    expect(0, 7, 10);

    clear();
    win_add_timer_event(3, &event_data, 10, 0);
    win_add_timer_event(3, &event_data, 20, 0);
    win_remove_timer_event(3, &event_data);
    pump(10);
    assert(!used);
    pump(20);
    assert(used == 1);
    expect(0, 7, 20);
    win_remove_event_handler(3, event_stop, &event_data);

    clear();
    struct task inner = {10, 0, 1, 0, 0};
    win_add_timer(run, &inner, 10, 1);
    win_add_timer(nested, NULL, 10, 1);
    pump(10);
    assert(used == 2);
    expect(0, 9, 10);
    expect(1, 10, 10);
    clear();
    puts("PASS: eight historical timer scenarios");
    return 0;
}
