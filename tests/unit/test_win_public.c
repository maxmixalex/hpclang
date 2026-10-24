#include <winevent.h>
#include <wintimer.h>
#include <winkeys.h>
#include <hpkeyb49.h>
#include "hpgcc-keyboard.h"
#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>

static uint64_t now, physical_matrix;
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
    matrix->full = physical_matrix;
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
static int typed_calls, nested, broken, changes, repaint_posts;
static win_keytyped_t captured[4];
static int typed(void *event, void *data)
{
    assert(!data);
    win_keytyped_t *key = event;
    unsigned index = (unsigned)typed_calls++;
    assert(index < 4);
    captured[index] = *key;
    assert(hpclang_win_reset() == -1 && errno == EBUSY);
    if (nested && index == 0) {
        assert(win_post_event(WIN_KEY_PRESSED, (void *)(uintptr_t)KB_B) == -1);
        assert(!hpclang_win_error());
        assert(key->ascii == 'A' && key->alpha == WIN_KEYSTATE_ON);
    }
    if (broken)
        win_post_event(-1, NULL);
    return -1;
}
static int modifier(void *event, void *data)
{
    (void)data;
    const unsigned expected[] = {WIN_KEYSTATE_PRESSED, WIN_KEYSTATE_ON, WIN_KEYSTATE_OFF};
    assert(changes < 3 && (uintptr_t)event == expected[changes++]);
    return 0;
}
static int key_repaint(void *event, void *data)
{
    assert(!event && !data);
    ++repaint_posts;
    return 0;
}
static int consume(void *event, void *data)
{
    (void)event;
    (void)data;
    return 17;
}
static void check_keys(void)
{
    assert(!hpclang_win_reset());
    win_add_event_handler(WIN_KEY_TYPED, 0, typed, NULL);
    win_add_event_handler(WIN_ALPHA_CHANGED, 0, modifier, NULL);
    win_add_event_handler(7, 0, key_repaint, NULL);
    assert(!win_post_event(WIN_KEY_PRESSED, (void *)(uintptr_t)KB_ALPHA));
    assert(!win_post_event(WIN_KEY_RELEASED, (void *)(uintptr_t)KB_ALPHA));
    nested = 1;
    assert(win_post_event(WIN_KEY_PRESSED, (void *)(uintptr_t)KB_A) == -1);
    assert(!hpclang_win_error() && typed_calls == 2);
    assert(captured[0].ascii == 'A' && captured[0].keycode == KB_A);
    assert(captured[1].ascii == 0 && captured[1].alpha == WIN_KEYSTATE_OFF);
    assert(changes == 3 && repaint_posts == 3);
    assert(!win_post_event(WIN_KEY_RELEASED, (void *)(uintptr_t)KB_A));
    assert(typed_calls == 2);
    win_add_event_handler(WIN_KEY_PRESSED, INT_MAX, consume, NULL);
    assert(win_post_event(WIN_KEY_PRESSED, (void *)(uintptr_t)KB_ALPHA) == 17);
    assert(changes == 3);
    win_remove_event_handler(WIN_KEY_PRESSED, consume, NULL);
    assert(win_post_event(WIN_KEY_PRESSED, (void *)(uintptr_t)KB_1) == -1);
    assert(captured[2].ascii == '1' && captured[2].alpha == WIN_KEYSTATE_OFF);
    broken = 1;
    assert(win_post_event(WIN_KEY_PRESSED, (void *)(uintptr_t)KB_A) == -1);
    assert(hpclang_win_error() == EINVAL && typed_calls == 4);
    assert(!hpclang_win_reset());
    assert(win_post_event(WIN_KEY_PRESSED, (void *)(uintptr_t)UINTPTR_MAX) == -1);
    assert(hpclang_win_error() == EINVAL);
    assert(!hpclang_win_reset());
    typed_calls = nested = broken = changes = repaint_posts = 0;
    win_add_event_handler(WIN_KEY_TYPED, 0, typed, NULL);
    win_add_event_handler(WIN_ALPHA_CHANGED, 0, modifier, NULL);
    win_add_event_handler(7, 0, key_repaint, NULL);
    physical_matrix = UINT64_C(1) << KB_ALPHA;
    assert(hpclang_win_step(1) == 1);
    physical_matrix = 0;
    assert(hpclang_win_step(1) == 1);
    physical_matrix = UINT64_C(1) << KB_A;
    assert(hpclang_win_step(1) == 1);
    assert(typed_calls == 1 && captured[0].ascii == 'A');
    assert(changes == 3 && repaint_posts == 3 && !hpclang_win_error());
    physical_matrix = 0;
    assert(hpclang_win_step(1) == 1 && typed_calls == 1);
    assert(!hpclang_win_reset());
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
    for (int i = 0; i < 59; ++i)
        win_add_event_handler(event, i, callback, &calls);
    assert(!hpclang_win_error());
    win_add_event_handler(event, 0, callback, &calls);
    assert(hpclang_win_error() == ENOSPC);
    assert(!hpclang_win_reset());
    assert(win_post_event(8, NULL) == -1 && hpclang_win_error() == EINVAL);
    assert(!hpclang_win_reset());
    check_keys();
    io_error = 1;
    win_event_loop();
    assert(hpclang_win_error() == EIO);
    return 0;
}
