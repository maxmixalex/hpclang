#include <winsoftmenu.h>
#include <win.h>
#include <hpclang/device.h>
#include <string.h>
#include <errno.h>
#include <wincore.h>
#include <winkeys.h>
#include <hpkeyb49.h>
#include "hpgcc-keyboard.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

/* Production event, focus and widget lifecycle; renderer calls are stubbed.
 * Independent production-pixel/native checks remain required. */
static uint64_t now;
static int screen_height = 64, fill_y, last_color, text_y;
static char last_label[128];
static int font_token;
static hpg_pattern_t *current_pattern;
hpg_t *hpg_stdscreen;
int hpclang_time_read_us(uint64_t *value)
{
    *value = now;
    now += 1000;
    return 0;
}
int hpclang_hpgcc_keymatrix_read(keymatrix *matrix)
{
    matrix->full = 0;
    return 0;
}
int hpclang_hpg_frame_begin(void *data)
{
    assert(!data);

    return 0;
}
int hpclang_hpg_frame_present(void *data)
{
    assert(!data);

    return 0;
}
int hpclang_hpg_indicator_set(unsigned char indicator, unsigned char color)
{
    (void)indicator;
    (void)color;
    return 0;
}
hpg_font_t *hpg_get_minifont(void)
{
    return (hpg_font_t *)&font_token;
}
void hpg_clip_reset(hpg_t *g)
{
    assert(g == hpg_stdscreen);
}
void hpg_clip(hpg_t *g, int x, int y, int right, int bottom)
{
    assert(g == hpg_stdscreen);
    (void)x;
    (void)y;
    (void)right;
    (void)bottom;
}
void hpg_set_color(hpg_t *g, unsigned char color)
{
    assert(g == hpg_stdscreen);
    last_color = color;
}
void hpg_fill_rect(int x, int y, int right, int bottom)
{
    (void)x;
    (void)right;
    (void)bottom;
    fill_y = y;
}
void hpg_draw_rect(int x, int y, int right, int bottom)
{
    (void)x;
    (void)y;
    (void)right;
    (void)bottom;
}
void hpg_set_font(hpg_t *g, hpg_font_t *font)
{
    assert(g == hpg_stdscreen && font == hpg_get_minifont());
}
void hpg_set_mode(hpg_t *g, unsigned char mode)
{
    assert(g == hpg_stdscreen);
    (void)mode;
}
hpg_pattern_t *hpg_alloc_pattern(char *buffer, int height, int fixed)
{
    (void)fixed;
    (void)height;
    (void)buffer;

    return (hpg_pattern_t *)malloc(1);
}
void hpg_free_pattern(hpg_pattern_t *pattern)
{
    free(pattern);
}
hpg_pattern_t *hpg_get_pattern(hpg_t *g)
{
    assert(g == hpg_stdscreen);
    return current_pattern;
}
void hpg_set_pattern(hpg_t *g, hpg_pattern_t *pattern)
{
    assert(g == hpg_stdscreen);
    current_pattern = pattern;
}

int hpg_font_get_advance(hpg_font_t *font)
{
    (void)font;
    return 4;
}
int hpg_font_get_height(hpg_font_t *font)
{
    (void)font;
    return 6;
}
const struct hpclang_device_info *hpclang_device_info(void)
{
    static struct hpclang_device_info device;
    device.display_width = 131;
    device.display_height = (unsigned)screen_height;
    return &device;
}
int hpg_get_width(hpg_t *image)
{
    assert(!image);
    return 131;
}
int hpg_get_height(hpg_t *image)
{
    assert(!image);
    return screen_height;
}
void hpg_draw_text(char *label, int x, int y)
{
    (void)x;
    text_y = y;
    strcpy(last_label, label);
}
void hpg_draw_letter(char code, int x, int y)
{
    (void)code;
    (void)x;
    (void)y;
}

extern win_widget_t *menu, *label, *field, *check, *slider1, *slider2, *slider3, *slider4;
int historical_main(void);
void __real_win_event_loop(void);
static unsigned phase;
static int scripting;
static void typed(unsigned code)
{
    win_keytyped_t key = {code, 0, 0, 0, 0};
    assert(win_post_event(WIN_KEY_TYPED, &key) == 1);
    assert(!hpclang_win_error());
}
static int script(void *event, void *context)
{
    (void)event;
    (void)context;
    /* Progress defaults its change event to IDLE: ignore nested notifications. */
    if (scripting)
        return 0;
    scripting = 1;
    switch (phase++) {
    case 0:
        assert(win_widget_has_focus(field));
        typed(KB_B);
        assert(!strcmp(win_text_get_text(field), "A"));
        break;
    case 1:
        typed(KB_C);
        assert(!strcmp(win_text_get_text(field), "B"));
        break;
    case 2:
        typed(KB_A);
        assert(!strcmp(win_text_get_text(field), ""));
        break;
    case 3:
        typed(KB_DN);
        assert(win_widget_has_focus(check));
        break;
    case 4:
        typed(KB_D);
        assert(win_checkbox_is_selected(check));
        break;
    case 5:
        win_focus_next();
        assert(win_widget_has_focus(slider1));
        break;
    case 6:
    case 7:
    case 8:
    case 9: {
        win_widget_t *sliders[] = {slider1, slider2, slider3, slider4};
        const unsigned keys[] = {KB_RT, KB_UP, KB_DN, KB_LF};
        unsigned i = phase - 7;
        assert(win_widget_has_focus(sliders[i]));
        assert(win_post_event(WIN_KEY_PRESSED, (void *)(uintptr_t)keys[i]) == 1);
        win_post_event(WIN_KEY_RELEASED, (void *)(uintptr_t)keys[i]);
        assert(win_progress_get_value(sliders[i]) == 10);
        win_focus_next();
        break;
    }
    case 10:
        typed(KB_F);
        break;
    default:
        assert(!"historical EXIT failed to stop the real loop");
    }
    scripting = 0;
    return 0;
}
void __wrap_win_event_loop(void)
{
    /* Instrument the original call; the production loop/idle dispatch still run. */
    win_add_event_handler(WIN_IDLE_EVENT, 100, script, NULL);
    __real_win_event_loop();
}
int main(void)
{
    assert(!historical_main() && phase == 11 && !hpclang_win_error());
    int x, y;
    win_widget_get_location(slider4, &x, &y);
    assert(x == 0 && y == 65); /* Preserve, and report, the original 80-line layout. */
    win_widget_t *widgets[] = {slider4, slider3, slider2, slider1, check, field, label, menu};
    /* Original process-exit-only ownership is cleaned by the test harness so
     * sanitizers can check adapter lifetime without attributing app leaks to it. */
    for (unsigned i = 0; i < sizeof(widgets) / sizeof(widgets[0]); ++i) {
        win_remove_widget(widgets[i]);
        win_widget_destroy(widgets[i]);
    }
    assert(!hpclang_win_error() && !hpclang_win_reset());
    puts("PASS original simple.c: A/B/CLEAR, checkbox menu, four sliders and EXIT in real event "
         "loop");
    return 0;
}
