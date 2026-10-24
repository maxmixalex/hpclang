#include <winsoftmenu.h>
#include <wincheckbox.h>
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

/* The production window/widget/event/timer foundation is shared by candidate
 * and reference. Only image-control functions are exchanged. Device calls
 * record their arguments; production renderer pixels have separate tests. */
static uint64_t now;
static int screen_height = 64, fill_y, last_color, text_y;
static char last_label[6];
static int font_token;
static hpg_pattern_t *current_pattern;
hpg_t *hpg_stdscreen;
int hpclang_time_read_us(uint64_t *value)
{
    *value = now;
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
    puts("begin");
    return 0;
}
int hpclang_hpg_frame_present(void *data)
{
    assert(!data);
    puts("present");
    return 0;
}
int hpclang_hpg_indicator_set(unsigned char indicator, unsigned char color)
{
    printf("indicator %u %u\n", indicator, color);
    return 0;
}
hpg_font_t *hpg_get_minifont(void)
{
    return (hpg_font_t *)&font_token;
}
void hpg_clip_reset(hpg_t *g)
{
    assert(g == hpg_stdscreen);
    /* Clip reset is redundant at the default full-screen geometry. */
}
void hpg_clip(hpg_t *g, int x, int y, int right, int bottom)
{
    assert(g == hpg_stdscreen);
    printf("clip %d %d %d %d\n", x, y, right, bottom);
}
void hpg_set_color(hpg_t *g, unsigned char color)
{
    assert(g == hpg_stdscreen);
    last_color = color;
    printf("color %u\n", color);
}
void hpg_fill_rect(int x, int y, int right, int bottom)
{
    (void)x;
    (void)right;
    (void)bottom;
    fill_y = y;
    printf("fill %d %d %d %d\n", x, y, right, bottom);
}
void hpg_draw_rect(int x, int y, int right, int bottom)
{
    printf("border %d %d %d %d\n", x, y, right, bottom);
}
void hpg_set_font(hpg_t *g, hpg_font_t *font)
{
    assert(g == hpg_stdscreen && font == hpg_get_minifont());
    puts("font");
}
void hpg_set_mode(hpg_t *g, unsigned char mode)
{
    assert(g == hpg_stdscreen);
    printf("mode %u\n", mode);
}
hpg_pattern_t *hpg_alloc_pattern(char *buffer, int height, int fixed)
{
    printf("pattern %d %d", height, fixed);
    for (int i = 0; i < height; ++i)
        printf(" %02x", (unsigned char)buffer[i]);
    puts("");
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
    printf("pattern-set %d\n", !!pattern);
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
    printf("text %s %d %d\n", label, x, y);
}
static int press(unsigned key, unsigned lshift, unsigned rshift, unsigned alpha)
{
    win_keytyped_t event = {key, 0, lshift, rshift, alpha};
    return win_post_event(WIN_KEY_TYPED, &event);
}
static void state(win_widget_t *a, win_widget_t *b)
{
    printf("state %s %d %d / %s %d %d\n", win_checkbox_get_label(a), win_checkbox_is_selected(a),
           win_widget_has_focus(a), win_checkbox_get_label(b), win_checkbox_is_selected(b),
           win_widget_has_focus(b));
    assert(!win_post_event(WIN_REPAINT_EVENT, NULL));
    assert(!hpclang_win_error());
}
int main(void)
{
    screen_height = 80;
    char *labels[] = {"", "A", "AB", "A\nBC", "AB\n", "\n\n"};
    assert(!hpclang_win_reset());
    for (unsigned scenario = 0; scenario < 6; ++scenario) {
        win_widget_t *menu = win_softmenu_new();
        win_widget_t *a = win_checkbox_new(labels[scenario]);
        win_widget_t *b = win_checkbox_new("B");
        assert(menu && a && b);
        int w, h;
        win_widget_get_size(a, &w, &h);
        printf("size %u %d %d\n", scenario, w, h);
        win_widget_set_location(a, 2, 3);
        win_widget_set_location(b, 24, 3);
        win_add_widget(menu);
        win_add_widget(a);
        win_add_widget(b);
        win_checkbox_set_menu(a, menu, 0);
        win_checkbox_set_menu(b, menu, 0);
        /* Compare established bindings, not the original first-install bug. */
        win_focus_next();
        win_focus_next();
        for (int selected = -1; selected <= 1; ++selected) {
            win_checkbox_set_selected(a, selected);
            state(a, b);
            for (unsigned modifier = 0; modifier < 4; ++modifier) {
                printf("toggle %u %d\n", modifier,
                       press(KB_W, modifier == 1, modifier == 2, modifier == 3));
                state(a, b);
            }
            printf("menu %d\n", press(KB_A, 0, 0, 0));
            state(a, b);
            assert(press(KB_DN, 0, 0, 0) == 1);
            state(a, b);
            printf("menu %d\n", press(KB_A, 0, 0, 0));
            state(a, b);
            assert(press(KB_UP, 0, 0, 0) == 1);
            state(a, b);
        }
        char replacement[] = "NEW";
        win_checkbox_set_label(a, replacement);
        assert(win_checkbox_get_label(a) == replacement);
        win_widget_pack(a);
        state(a, b);
        win_remove_widget(b);
        win_widget_destroy(b);
        win_remove_widget(a);
        win_widget_destroy(a);
        win_remove_widget(menu);
        win_widget_destroy(menu);
        assert(!hpclang_win_error());
    }
    assert(!hpclang_win_reset());
    return 0;
}
