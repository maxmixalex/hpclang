#include <winsoftmenu.h>
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
static unsigned deliveries, left, right;
static uintptr_t extra;
static int receive(void *event, void *context)
{
    assert(context == &deliveries);
    win_softmenu_event_t *payload = event;
    ++deliveries;
    left = payload->lshift;
    right = payload->rshift;
    extra = (uintptr_t)payload->extra_data;
    printf("event %u %u %lu\n", left, right, (unsigned long)extra);
    return 71;
}
static int press(unsigned key, unsigned lshift, unsigned rshift, unsigned alpha)
{
    win_keytyped_t event = {key, 0, lshift, rshift, alpha};
    return win_post_event(WIN_KEY_TYPED, &event);
}
int main(void)
{
    screen_height = 80;
    for (unsigned count = 1; count <= 30; ++count) {
        assert(!hpclang_win_reset());
        int event = win_register_event();
        win_add_event_handler(event, 0, receive, &deliveries);
        win_widget_t *menu = win_softmenu_new();
        assert(menu);
        char label[] = "ABCDEF";
        for (unsigned i = 0; i < count; ++i)
            assert(win_softmenu_add_item(menu, label, event, (void *)(uintptr_t)(i + 1)) == (int)i);
        win_add_widget(menu);
        const unsigned keys[] = {KB_A, KB_B, KB_C, KB_D, KB_E, KB_F};
        for (unsigned page = 0; page < 5; ++page) {
            assert(!win_post_event(WIN_REPAINT_EVENT, NULL));
            for (unsigned i = 0; i < 6; ++i)
                printf("key %u %d\n", keys[i], press(keys[i], 1, 2, 0));
            assert(press(KB_L, 0, 0, 0) == 1);
        }
        /* Return to a known first page without depending on unused pages. */
        win_remove_widget(menu);
        win_widget_destroy(menu);
        assert(!hpclang_win_reset());
        event = win_register_event();
        win_add_event_handler(event, 0, receive, &deliveries);
        menu = win_softmenu_new();
        for (unsigned i = 0; i < 30; ++i)
            assert(win_softmenu_add_item(menu, label, event, (void *)(uintptr_t)(i + 1)) == (int)i);
        win_softmenu_insert_item(menu, 0, "new", event, (void *)(uintptr_t)99);
        win_softmenu_delete_item(menu, (int)count - 1, 29);
        win_softmenu_set_colors(menu, 0, 17, 238);
        win_softmenu_set_disable_colors(menu, 0, 51, 102);
        win_softmenu_set_enabled(menu, 0, 0);
        win_add_widget(menu);
        assert(!win_post_event(WIN_REPAINT_EVENT, NULL));
        assert(!press(KB_A, 0, 0, 0));
        assert(press(KB_L, 0, 0, 0) == 1);
        assert(press(KB_L, 1, 0, 0) == 1); /* safe nonzero backward start */
        assert(!press(KB_A, 0, 0, 1));
        assert(!press(KB_L, 0, 1, 0));
        win_softmenu_set_enabled(menu, 0, 1);
        assert(press(KB_A, 1, 2, 0) == 71);
        win_remove_widget(menu);
        assert(!press(KB_A, 0, 0, 0));
        win_widget_destroy(menu);
        assert(!hpclang_win_error());
    }
    assert(!hpclang_win_reset());
    return 0;
}
