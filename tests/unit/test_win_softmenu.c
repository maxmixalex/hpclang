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
    puts("clip-reset");
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
    return 71;
}
static int press(unsigned key, unsigned lshift, unsigned rshift, unsigned alpha)
{
    win_keytyped_t event = {key, 0, lshift, rshift, alpha};
    return win_post_event(WIN_KEY_TYPED, &event);
}
int main(void)
{
    for (screen_height = 64; screen_height <= 80; screen_height += 16) {
        assert(!hpclang_win_reset());
        int event = win_register_event();
        win_add_event_handler(event, 0, receive, &deliveries);
        win_widget_t *menu = win_softmenu_new();
        assert(menu);
        int w, h;
        win_widget_get_size(menu, &w, &h);
        assert(w == 131 && h == screen_height);
        char label[] = "123456789";
        for (int i = 0; i < 30; ++i)
            assert(win_softmenu_add_item(menu, label, event, (void *)(uintptr_t)(i + 1)) == i);
        assert(win_softmenu_add_item(menu, label, event, NULL) == -1 && !hpclang_win_error());
        label[0] = 'X';
        win_add_widget(menu);
        assert(!win_post_event(WIN_REPAINT_EVENT, NULL));
        assert(!strcmp(last_label, "12345") && fill_y == screen_height - 7 &&
               text_y == screen_height - 6);
        const unsigned keys[] = {KB_A, KB_B, KB_C, KB_D, KB_E, KB_F};
        for (unsigned page = 0; page < 5; ++page) {
            for (unsigned i = 0; i < 6; ++i) {
                assert(press(keys[i], 1, 2, 0) == 71);
                assert(extra == page * 6 + i + 1 && left == 1 && right == 2);
            }
            assert(press(KB_L, 0, 0, 0) == 1);
        }
        assert(press(KB_A, 0, 0, 0) == 71 && extra == 1);
        assert(press(KB_L, 1, 0, 0) == 1);
        assert(press(KB_A, 0, 0, 0) == 71 && extra == 25);
        assert(press(KB_L, 0, 0, 0) == 1);
        assert(press(KB_A, 0, 0, 1) == 0 && press(KB_L, 0, 1, 0) == 0);
        win_softmenu_set_enabled(menu, 0, 0);
        unsigned before = deliveries;
        assert(press(KB_A, 0, 0, 0) == 0 && deliveries == before);
        win_softmenu_delete_item(menu, 0, 0);
        win_softmenu_delete_item(menu, 2, 0);
        win_softmenu_delete_item(menu, 1, 29);
        assert(press(KB_B, 0, 0, 0) == 0); /* gap must shift, not inherit occupied bit */
        assert(press(KB_C, 0, 0, 0) == 71 && extra == 4);
        win_softmenu_insert_item(menu, 2, "NEW", event, (void *)(uintptr_t)99);
        assert(press(KB_C, 0, 0, 0) == 71 && extra == 99);
        assert(press(KB_D, 0, 0, 0) == 71 && extra == 4);
        win_remove_widget(menu);
        assert(press(KB_C, 0, 0, 0) == 0);
        win_softmenu_set_item(menu, -1, "bad", event, NULL);
        assert(hpclang_win_error() == EINVAL);
        win_widget_destroy(menu);
        assert(!hpclang_win_reset());
        menu = win_softmenu_new();
        win_softmenu_set_item(menu, 0, "ONE", event, NULL);
        win_softmenu_set_colors(menu, 0, 17, 238);
        win_softmenu_set_disable_colors(menu, 0, 51, 102);
        win_add_widget(menu);
        assert(!win_post_event(WIN_REPAINT_EVENT, NULL) && last_color == 17);
        win_softmenu_set_enabled(menu, 0, 0);
        assert(!win_post_event(WIN_REPAINT_EVENT, NULL) && last_color == 51);
        assert(press(KB_L, 0, 0, 0) == 1); /* wraps when no later occupied item */
        assert(!win_post_event(WIN_REPAINT_EVENT, NULL) && !strcmp(last_label, "ONE"));
        win_remove_widget(menu);
        win_widget_destroy(menu);
        assert(!hpclang_win_reset());
    }
    return 0;
}
