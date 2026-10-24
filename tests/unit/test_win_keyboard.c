#include "win-keyboard.h"
#include "hpgcc-keyboard.h"
#include "win-drivers.h"
#include <hpclang/abi.h>
#include <hpclang/keyboard.h>
#include <assert.h>
#include <errno.h>

static struct hpclang_boot_info boot;
static struct hpclang_win_events events;
static struct hpclang_win_handler handlers[8];
static struct hpclang_win_call snapshots[8];
static struct hpclang_win_keyboard keyboard;
static uint64_t physical;
static unsigned reads, count;
static int trace[256], nesting, exhaust;
static uint64_t clock_now;
static int clock_error;
static struct hpclang_win_loop loop;
static struct hpclang_win_timers timers;
static struct hpclang_win_timer timer_slots[2];
static struct hpclang_win_drivers drivers;
int hpclang_time_read_us(uint64_t *value)
{
    if (clock_error) {
        errno = clock_error;
        return -1;
    }
    *value = clock_now;
    return 0;
}
static int repaint(void *event, void *data)
{
    assert(!event && data == &drivers);
    ++clock_now;
    return 0;
}
static int timed_quit(void *data)
{
    assert(data == &loop);
    assert(!hpclang_win_loop_quit(&loop));
    return 1;
}
const struct hpclang_boot_info *hpclang_boot_info(void)
{
    return &boot;
}
void hpclang_keyboard_read(struct hpclang_keyboard_state *state)
{
    ++reads;
    state->bits = physical;
}
void hpclang_delay_us(uint64_t us)
{
    (void)us;
    assert(0); /* Polling never waits. */
}
static int capture(void *data, void *context)
{
    int key = (int)(uintptr_t)data;
    assert(count < 256);
    trace[count++] = context ? -(key + 1) : key + 1;
    if (nesting && !context && key == 1) {
        nesting = 0;
        assert(!hpclang_win_keyboard_feed(&keyboard, 0));
    }
    if (exhaust && !context) {
        exhaust = 0;
        assert(!hpclang_win_events_add(&events, 1, 0, capture, NULL));
    }
    return 7; /* Consumption is per key, not the whole snapshot. */
}
static void init(size_t capacity)
{
    count = reads = 0;
    nesting = exhaust = 0;
    physical = 0;
    assert(!hpclang_win_events_init(&events, handlers, 8, snapshots, capacity));
    assert(!hpclang_win_events_add(&events, 1, 0, capture, NULL));
    assert(!hpclang_win_events_add(&events, 2, 0, capture, &keyboard));
    assert(!hpclang_win_keyboard_init(&keyboard, &events, 1, 2));
}
static void init_drivers(void)
{
    init(8);
    boot.model = 1;
    clock_now = 0;
    clock_error = 0;
    assert(!hpclang_win_loop_init(&loop, &events, 0));
    assert(
        !hpclang_win_timers_init(&timers, timer_slots, 2, hpclang_win_device_clock, NULL, &events));
    drivers = (struct hpclang_win_drivers){&loop, &keyboard, &timers, SIZE_MAX};
    assert(!hpclang_win_events_add(&events, 0, 2000000002, hpclang_win_keyboard_idle, &drivers));
    assert(!hpclang_win_events_add(&events, 0, 2000000001, repaint, &drivers));
    assert(!hpclang_win_events_add(&events, 0, 2000000000, hpclang_win_timer_idle, &drivers));
}
int main(void)
{
    for (boot.model = 1; boot.model <= 6; ++boot.model) {
        init(8);
        for (unsigned bit = 0; bit < 60; ++bit) {
            physical = UINT64_C(1) << bit;
            unsigned code = bit < 56 ? (bit / 7) * 8 + bit % 7 + 1 : bit == 56 ? 63 : bit + 3;
            unsigned before = count;
            assert(!hpclang_win_keyboard_poll(&keyboard));
            assert(trace[count - 1] == (int)code + 1);
            assert(count == before + 1);
            assert(!hpclang_win_keyboard_poll(&keyboard) && count == before + 1);
            physical = 0;
            assert(!hpclang_win_keyboard_poll(&keyboard));
            assert(trace[count - 1] == -(int)(code + 1));
        }
        assert(count == 120 && reads == 180);
    }
    init(8);
    keymatrix matrix = {.full = 123};
    boot.model = 0;
    assert(hpclang_hpgcc_keymatrix_read(&matrix) == -1 && errno == ENOSYS);
    assert(matrix.full == 123 && !reads);
    assert(hpclang_win_keyboard_poll(&keyboard) == -1 && errno == ENOSYS);
    assert(!keyboard.previous && !keyboard.failure && !count);
    boot.model = 1;
    errno = EDOM;
    assert(!hpclang_hpgcc_keymatrix_read(&matrix) && matrix.full == 0 && errno == EDOM);
    assert(!hpclang_win_keyboard_feed(&keyboard, UINT64_C(1) << 63));
    assert(trace[0] == 64);
    assert(!hpclang_win_keyboard_feed(&keyboard, 3));
    assert(count == 4 && trace[1] == 1 && trace[2] == 2 && trace[3] == -64);

    init(8);
    nesting = 1;
    assert(!hpclang_win_keyboard_feed(&keyboard, 6));
    assert(count == 4 && trace[0] == 2 && trace[1] == -2 && trace[2] == -3 && trace[3] == 3);
    assert(!keyboard.previous && !events.depth && !events.used);

    init(1);
    exhaust = 1;
    assert(hpclang_win_keyboard_feed(&keyboard, 6) == -1 && errno == ENOSPC);
    assert(count == 1 && trace[0] == 2 && keyboard.failure == ENOSPC);
    assert(hpclang_win_keyboard_poll(&keyboard) == -1 && errno == ENOSPC && !reads);
    assert(count == 1 && !events.depth && !events.used);
    assert(hpclang_win_keyboard_feed(NULL, 0) == -1 && errno == EINVAL);

    init_drivers();
    physical = UINT64_C(1) << 56;
    assert(!hpclang_win_timers_add(&timers, timed_quit, &loop, 2, 0));
    assert(!hpclang_win_loop_run(&loop, 8));
    assert(clock_now == 2 && reads == 2 && count == 1 && trace[0] == 64);
    assert(!timers.head && !loop.failure);

    init_drivers();
    boot.model = 0;
    assert(hpclang_win_loop_run(&loop, 8) == -1 && errno == ENOSYS);
    assert(!clock_now && !reads && !count); /* Keyboard failure consumes idle. */
    boot.model = 1;
    assert(hpclang_win_loop_run(&loop, 8) == -1 && errno == ENOSYS && !reads);

    init_drivers();
    assert(!hpclang_win_timers_add(&timers, timed_quit, &loop, 2, 0));
    clock_error = EIO;
    assert(hpclang_win_loop_run(&loop, 8) == -1 && errno == EIO);
    assert(clock_now == 1 && reads == 1 && timers.head && !timers.depth);
    assert(!events.depth && !events.used && loop.failure == EIO);

    init_drivers();
    events.snapshot_capacity = 3; /* Idle fits; nested key dispatch does not. */
    physical = 1;
    assert(hpclang_win_loop_run(&loop, 8) == -1 && errno == ENOSPC);
    assert(keyboard.failure == ENOSPC && loop.failure == ENOSPC);
    assert(reads == 1 && !count && !clock_now && !events.depth && !events.used);
    return 0;
}
