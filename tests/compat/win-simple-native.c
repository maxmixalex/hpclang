/* External instrumentation for the archive-extracted simple.c application.
 * The application retains its event loop, callbacks, focus and ownership. */
#include <win.h>
#include <hpgraphics.h>
#include <hpkeyb49.h>
#include <hpclang/time.h>
#include <limits.h>
#include <stdint.h>
#include <string.h>

extern win_widget_t *menu, *label, *field, *check, *slider1, *slider2, *slider3, *slider4;
int historical_main(void);
void __real_win_event_loop(void);
volatile uint32_t hpclang_e2e_result;
static unsigned failure, action, waiting, edges, expected_key, expected_down, begun, done, busy;
static uint64_t started;
static const unsigned keys[] = {KB_B,  KB_C,  KB_A,  KB_DN, KB_D,  KB_DN, KB_RT, KB_DN,
                                KB_UP, KB_RT, KB_DN, KB_RT, KB_LF, KB_DN, KB_F};
static void fail(unsigned code)
{
    if (!failure)
        failure = code;
    win_event_quit();
}
static int handshake(uint32_t request, uint32_t ack)
{
    uint64_t start = hpclang_time_monotonic_us();
    hpclang_e2e_result = request;
    while (hpclang_e2e_result != ack)
        if (hpclang_time_monotonic_us() - start > 2000000) {
            fail(1);
            return -1;
        }
    return 0;
}
static int frame(unsigned id)
{
    return handshake(UINT32_C(0x46520000) | id, UINT32_C(0x464b0000) | id);
}
static int gpio(unsigned key, unsigned down)
{
    expected_key = key;
    expected_down = down;
    unsigned scan = key >= 60 ? key - 3 : (key / 8) * 7 + key % 8 - 1;
    unsigned argument = scan | (down << 8);
    return handshake(UINT32_C(0x4b490000) | argument, UINT32_C(0x4b4a0000) | argument);
}
static int observe(void *event, void *context)
{
    if ((uintptr_t)event != expected_key || (uintptr_t)context != expected_down)
        fail(2);
    ++edges;
    return 0;
}
static int state(unsigned step)
{
    switch (step) {
    case 0:
        return !strcmp(win_text_get_text(field), "A");
    case 1:
        return !strcmp(win_text_get_text(field), "B");
    case 2:
        return !strcmp(win_text_get_text(field), "");
    case 3:
        return win_widget_has_focus(check);
    case 4:
        return win_checkbox_is_selected(check);
    case 5:
        return win_widget_has_focus(slider1);
    case 6:
        return win_progress_get_value(slider1) == 10;
    case 7:
        return win_widget_has_focus(slider2);
    case 8:
        return win_progress_get_value(slider2) == 10;
    case 9:
        return win_widget_has_focus(slider3);
    case 10:
        return win_progress_get_value(slider3) == 10;
    case 11:
        return win_widget_has_focus(slider4);
    case 12:
        return win_progress_get_value(slider4) == 10;
    case 13:
        return win_widget_has_focus(field);
    default:
        return 0;
    }
}
static int drive(void *event, void *context)
{
    (void)event;
    (void)context;
    if (busy || failure || done)
        return 0;
    busy = 1;
    if (hpclang_win_error() || hpclang_time_monotonic_us() - started > 10000000) {
        fail(3);
        goto out;
    }
    if (!begun) {
        if (!win_widget_has_focus(field) || win_checkbox_is_selected(check) ||
            strcmp(win_text_get_text(field), "")) {
            fail(4);
            goto out;
        }
        begun = 1;
        if (frame(2100))
            goto out;
    }
    if (!waiting) {
        waiting = 1;
        (void)gpio(keys[action], 1);
    } else if (waiting == 1 && edges == action * 2 + 1) {
        if (action == sizeof(keys) / sizeof(keys[0]) - 1) {
            done = 1; /* EXIT stops the real loop; release its GPIO afterward. */
        } else {
            waiting = 2;
            (void)gpio(keys[action], 0);
        }
    } else if (waiting == 2 && edges == action * 2 + 2) {
        if (!state(action)) {
            fail(16 + action);
            goto out;
        }
        if (frame(2101 + action))
            goto out;
        ++action;
        waiting = 0;
    }
out:
    busy = 0;
    return 0;
}
void __wrap_win_event_loop(void)
{
    if (frame(50))
        return;
    uint64_t start = hpclang_time_monotonic_us();
    while (keyb_isAnyKeyPressed())
        if (hpclang_time_monotonic_us() - start > 2000000) {
            fail(5);
            return;
        }
    win_add_event_handler(WIN_KEY_PRESSED, INT_MAX, observe, (void *)(uintptr_t)1);
    win_add_event_handler(WIN_KEY_RELEASED, INT_MAX, observe, NULL);
    win_add_event_handler(WIN_IDLE_EVENT, -100, drive, NULL);
    started = hpclang_time_monotonic_us();
    __real_win_event_loop();
    if (!done || edges != 29 || hpclang_win_error())
        fail(6);
    if (!failure && (gpio(KB_F, 0) || keyb_isAnyKeyPressed()))
        fail(7);
}
int main(void)
{
    if (historical_main())
        fail(8);
    if (!done || !win_checkbox_is_selected(check) || win_progress_get_value(slider1) != 10 ||
        win_progress_get_value(slider2) != 10 || win_progress_get_value(slider3) != 10 ||
        win_progress_get_value(slider4) != 10 || strcmp(win_text_get_text(field), ""))
        fail(9);
    hpclang_e2e_result = failure ? UINT32_C(0x46417d00) | failure : UINT32_C(0x50415353);
    if (failure)
        for (;;)
            __asm__ volatile("nop");
    for (volatile uint32_t n = 12000000; n; --n)
        __asm__ volatile("nop");
    win_widget_t *widgets[] = {slider4, slider3, slider2, slider1, check, field, label, menu};
    for (unsigned i = 0; i < sizeof(widgets) / sizeof(widgets[0]); ++i) {
        win_remove_widget(widgets[i]);
        win_widget_destroy(widgets[i]);
    }
    (void)hpclang_win_reset();
    hpg_cleanup();
    return 0;
}
