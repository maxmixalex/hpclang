#include <winevent.h>
#include <wintimer.h>
#include <winkeys.h>
#include "win-key-state.h"
#include "win-drivers.h"
#include "win-repaint.h"
#include "hpg-internal.h"
#include <errno.h>
#include <limits.h>

static struct hpclang_win_events events;
static struct hpclang_win_handler handlers[64];
static struct hpclang_win_call snapshots[256];
static struct hpclang_win_timer slots[32];
static struct hpclang_win_loop loop;
static struct hpclang_win_keyboard keyboard;
static struct hpclang_win_key_state key_state;
static struct hpclang_win_timers timers;
static struct hpclang_win_repaint repaint;
static struct hpclang_win_drivers drivers;
static int initialized, next_event;

static int fail(int error)
{
    return hpclang_win_loop_fail(&loop, error);
}
static int ready(void)
{
    win_init();
    if (loop.failure) {
        errno = loop.failure;
        return 0;
    }
    return 1;
}
/* Keep the public bitfield payload separate from the internal state machine.
 * Each recursive dispatch owns its payload until its callbacks return. */
static int key_post(void *context, int event, void *data, int *result)
{
    (void)context;
    win_keytyped_t typed;
    if (event == WIN_KEY_TYPED) {
        const struct hpclang_win_typed_key *key = data;
        typed = (win_keytyped_t){key->keycode, key->ascii, key->lshift, key->rshift, key->alpha};
        data = &typed;
    }
    if (hpclang_win_events_post(&events, event, data, result))
        return fail(errno);
    return loop.failure ? fail(loop.failure) : 0;
}
static int key_translate(void *event, void *pressed)
{
    if (loop.failure)
        return fail(loop.failure);
    uintptr_t key = (uintptr_t)event;
    if (key >= 64)
        return fail(EINVAL);
    int result;
    if (hpclang_win_key_state_feed(&key_state, (unsigned)key, pressed != NULL, &result))
        return fail(errno);
    return result;
}
void win_init(void)
{
    if (initialized)
        return;
    /* Fixed valid storage and constants: initialization allocates nothing. */
    hpclang_win_events_init(&events, handlers, 64, snapshots, 256);
    hpclang_win_loop_init(&loop, &events, 0);
    hpclang_win_keyboard_init(&keyboard, &events, 1, 2);
    hpclang_win_timers_init(&timers, slots, 32, hpclang_win_device_clock, NULL, &events);
    hpclang_win_repaint_init(&repaint, &loop, 7, hpclang_hpg_frame_begin, hpclang_hpg_frame_present,
                             NULL);
    drivers = (struct hpclang_win_drivers){&loop, &keyboard, &timers, SIZE_MAX};
    hpclang_win_events_add(&events, 0, 2000000002, hpclang_win_keyboard_idle, &drivers);
    hpclang_win_events_add(&events, 0, 2000000001, hpclang_win_repaint_idle, &repaint);
    hpclang_win_events_add(&events, 0, 2000000000, hpclang_win_timer_idle, &drivers);
    hpclang_win_key_state_init(&key_state, key_post, NULL);
    hpclang_win_events_add(&events, WIN_KEY_PRESSED, 2000000000, key_translate,
                           (void *)(uintptr_t)1);
    hpclang_win_events_add(&events, WIN_KEY_RELEASED, 2000000000, key_translate, NULL);
    next_event = 8;
    initialized = 1;
}
static int event_valid(int event)
{
    if (event < 0 || event >= next_event) {
        fail(EINVAL);
        return 0;
    }
    return 1;
}
int win_register_event(void)
{
    if (!ready())
        return -1;
    if (next_event == INT_MAX)
        return fail(EOVERFLOW);
    return next_event++;
}
int win_post_event(int event, void *data)
{
    if (!ready() || !event_valid(event))
        return -1;
    int result;
    if (hpclang_win_events_post(&events, event, data, &result))
        return fail(errno);
    if (loop.failure)
        return fail(loop.failure);
    return result;
}
void win_add_event_handler(int event, int priority, win_eventhandler handler, void *data)
{
    if (ready() && event_valid(event) &&
        hpclang_win_events_add(&events, event, priority, handler, data))
        fail(errno);
}
void win_remove_event_handler(int event, win_eventhandler handler, void *data)
{
    if (ready() && event_valid(event) && hpclang_win_events_remove(&events, event, handler, data))
        fail(errno);
}
int hpclang_win_error(void)
{
    return initialized ? loop.failure : 0;
}
int hpclang_win_step(size_t budget)
{
    if (!ready())
        return -1;
    int result = hpclang_win_loop_run(&loop, budget);
    return result < 0 ? fail(errno) : result;
}
void win_event_loop(void)
{
    while (hpclang_win_step(SIZE_MAX) > 0) {
    }
}
void win_event_quit(void)
{
    win_init();
    hpclang_win_loop_quit(&loop);
}
int hpclang_win_reset(void)
{
    if (initialized && (events.depth || timers.depth)) {
        errno = EBUSY;
        return -1;
    }
    initialized = 0;
    win_init();
    return 0;
}
void win_repaint(void)
{
    if (ready() && hpclang_win_repaint_request(&repaint))
        fail(errno);
}
void win_add_timer(win_timerfunc_t function, void *data, int ms, int periodic)
{
    if (!ready())
        return;
    if (ms <= 0) {
        fail(EINVAL);
        return;
    }
    if (hpclang_win_timers_add(&timers, function, data, (uint64_t)ms * 1000, periodic))
        fail(errno);
}
void win_add_timer_event(int event, void *data, int ms, int periodic)
{
    if (!ready() || !event_valid(event))
        return;
    if (ms <= 0) {
        fail(EINVAL);
        return;
    }
    if (hpclang_win_timers_add_event(&timers, event, data, (uint64_t)ms * 1000, periodic))
        fail(errno);
}
void win_remove_timer(win_timerfunc_t function, void *data)
{
    if (ready() && hpclang_win_timers_remove(&timers, function, data))
        fail(errno);
}
void win_remove_timer_event(int event, void *data)
{
    if (ready() && event_valid(event) && hpclang_win_timers_remove_event(&timers, event, data))
        fail(errno);
}
