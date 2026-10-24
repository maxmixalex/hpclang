#include <winevent.h>
#include <wintimer.h>
#include "hpgcc-keyboard.h"
#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>

static uint64_t now;
static int calls, frames, paints, io_error;
int hpclang_hpg_frame_begin(void *context)
{
    assert(!context);
    ++frames;
    return 0;
}
int hpclang_hpg_frame_present(void *context)
{
    assert(!context && frames == paints);
    return 0;
}
int hpclang_hpgcc_keymatrix_read(keymatrix *matrix)
{
    if (io_error) {
        errno = EIO;
        return -1;
    }
    matrix->full = 0;
    now += 1000;
    return 0;
}
int hpclang_time_read_us(uint64_t *value)
{
    *value = now;
    return 0;
}
static int callback(void *event, void *data)
{
    assert(event == &calls && data == &calls);
    ++calls;
    assert(hpclang_win_reset() == -1 && errno == EBUSY);
    return -1;
}
static int paint(void *event, void *data)
{
    assert(!event && !data);
    ++paints;
    return 0;
}
static int timer(void *data)
{
    assert(data == &calls);
    ++calls;
    assert(hpclang_win_reset() == -1 && errno == EBUSY);
    if (calls == 3) {
        win_event_quit();
        return 1;
    }
    return 0;
}
int main(void)
{
    assert(!hpclang_win_error());
    int event = win_register_event();
    assert(event == 8 && win_register_event() == 9);
    win_add_event_handler(event, INT_MAX, callback, &calls);
    assert(win_post_event(event, &calls) == -1 && calls == 1 && !hpclang_win_error());
    win_remove_event_handler(event, callback, &calls);
    assert(!win_post_event(event, &calls) && calls == 1);
    assert(!hpclang_win_reset());
    calls = 0;
    win_add_event_handler(7, 0, paint, NULL);
    win_repaint();
    win_repaint();
    win_add_timer(timer, &calls, 2, 1);
    win_event_loop();
    assert(now == 6000 && calls == 3 && frames == 1 && paints == 1);
    assert(!hpclang_win_error());

    assert(!hpclang_win_reset());
    event = win_register_event();
    calls = 0;
    win_add_event_handler(event, 0, callback, &calls);
    win_add_timer_event(event, &calls, 1, 0);
    assert(hpclang_win_step(2) == 1 && calls == 1 && !hpclang_win_error());
    win_add_timer(timer, &calls, 0, 1);
    assert(hpclang_win_error() == EINVAL);
    assert(hpclang_win_step(1) == -1 && errno == EINVAL);
    assert(!hpclang_win_reset());
    win_add_timer(timer, &calls, INT_MAX, 1);
    assert(hpclang_win_step(1) == 1 && !hpclang_win_error());
    assert(!hpclang_win_reset());
    for (int i = 0; i < 32; ++i)
        win_add_timer(timer, &calls, 1, 1);
    assert(!hpclang_win_error());
    win_add_timer(timer, &calls, 1, 1);
    assert(hpclang_win_error() == ENOSPC);

    assert(!hpclang_win_reset());
    event = win_register_event();
    for (int i = 0; i < 61; ++i)
        win_add_event_handler(event, i, callback, &calls);
    assert(!hpclang_win_error());
    win_add_event_handler(event, 0, callback, &calls);
    assert(hpclang_win_error() == ENOSPC);
    assert(!hpclang_win_reset());
    assert(win_post_event(8, NULL) == -1 && hpclang_win_error() == EINVAL);
    assert(!hpclang_win_reset());
    io_error = 1;
    win_event_loop();
    assert(hpclang_win_error() == EIO);
    return 0;
}
