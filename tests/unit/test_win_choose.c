#include <winsoftmenu.h>
#include <winchoose.h>
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
void hpg_draw_letter(char code, int x, int y)
{
    (void)code;
    (void)x;
    (void)y;
}
static int press(unsigned code, unsigned left, unsigned right, unsigned alpha)
{
    win_keytyped_t key = {code, 0, left, right, alpha};
    return win_post_event(WIN_KEY_TYPED, &key);
}

static int unrelated;
static int other_action(void *event, void *context)
{
    (void)event;
    (void)context;
    ++unrelated;
    return 1;
}
int main(void)
{
    assert(!hpclang_win_reset());
    win_widget_t *menu = win_softmenu_new();
    win_widget_t *a = win_choose_new(), *b = win_choose_new();
    assert(menu && a && b);
    assert(win_choose_get_index(a) == -1 && !win_choose_get_choice(a));
    win_choose_add_with_label(a, "one", "A");
    win_choose_add_with_label(a, "two", "B\nCC");
    win_choose_add(a, "three");
    win_widget_pack(a);
    int width, height;
    win_widget_get_size(a, &width, &height);
    assert(width == 21 && height == 13);
    win_choose_set_menu(a, menu, 0);
    win_choose_set_menu(b, menu, 0);
    int event = win_register_event();
    win_add_event_handler(event, 0, other_action, NULL);
    win_softmenu_set_item(menu, 1, "OTHER", event, NULL);
    win_add_widget(menu);
    win_add_widget(a);
    win_add_widget(b);
    assert(win_widget_has_focus(a));
    assert(press(KB_W, 1, 0, 0) == 0 && win_choose_get_index(a) == 0);
    assert(press(KB_W, 0, 0, 0) == 1 && win_choose_get_index(a) == 1);
    assert(press(KB_A, 0, 0, 0) == 1);               /* open at two */
    assert(press(KB_B, 0, 0, 0) == 1 && !unrelated); /* modal isolates menu */
    assert(press(KB_DN, 0, 0, 0) == 1 && win_widget_has_focus(a));
    assert(win_choose_get_index(a) == 1); /* pending selection not committed */
    assert(press(KB_E, 0, 0, 0) == 1 && win_choose_get_index(a) == 1);
    assert(press(KB_B, 0, 0, 0) == 1 && unrelated == 1);
    assert(press(KB_A, 0, 0, 0) == 1);
    assert(press(KB_DN, 0, 0, 0) == 1);
    assert(press(KB_F, 0, 0, 0) == 1 && win_choose_get_index(a) == 2);
    assert(!strcmp(win_choose_get_choice(a), "three"));
    assert(press(KB_A, 0, 0, 0) == 1);
    assert(press(KB_DN, 0, 0, 0) == 1);
    assert(press(KB_F, 0, 0, 0) == 1 && win_choose_get_index(a) == 0);
    assert(press(KB_A, 0, 0, 0) == 1);
    assert(press(KB_UP, 0, 0, 0) == 1);
    assert(press(KB_F, 0, 0, 0) == 1 && win_choose_get_index(a) == 2);
    win_choose_set_index(a, -1);
    assert(!win_choose_get_choice(a));
    assert(press(KB_A, 0, 0, 0) == 1);
    assert(press(KB_F, 0, 0, 0) == 1 && win_choose_get_index(a) == 0);
    assert(press(KB_A, 0, 0, 0) == 1);
    win_choose_remove(a, "one"); /* mutation cancels popup; labels stay paired */
    assert(!strcmp(win_choose_get_choice(a), "two"));
    assert(press(KB_B, 0, 0, 0) == 1 && unrelated == 2);
    assert(press(KB_A, 0, 0, 0) == 1);
    win_focus_next(); /* programmatic focus loss closes popup */
    assert(win_widget_has_focus(b));
    assert(press(KB_A, 0, 0, 0) == 1); /* empty popup */
    assert(press(KB_UP, 0, 0, 0) == 1 && press(KB_DN, 0, 0, 0) == 1);
    assert(press(KB_F, 0, 0, 0) == 1 && win_choose_get_index(b) == -1);
    assert(press(KB_A, 0, 0, 0) == 1);
    win_remove_widget(b); /* open popup removal must not mutate ring recursively */
    assert(!hpclang_win_error() && win_widget_has_focus(a));
    assert(press(KB_A, 0, 0, 0) == 1);
    assert(press(KB_E, 0, 0, 0) == 1);
    win_choose_set_menu(a, menu, 2);
    assert(press(KB_A, 0, 0, 0) == 0);
    assert(press(KB_C, 0, 0, 0) == 1);
    win_choose_set_menu(a, NULL, 0);
    assert(press(KB_B, 0, 0, 0) == 1 && unrelated == 3);
    win_remove_widget(a);
    win_remove_widget(menu);
    assert(!hpclang_win_reset());
    assert(win_register_event() >= 0);
    win_choose_set_menu(a, menu, 0);
    win_add_widget(a);
    win_add_widget(menu);
    assert(press(KB_A, 0, 0, 0) == 1);
    assert(press(KB_E, 0, 0, 0) == 1);
    /* Long lists must render on both hardware heights without invalid access. */
    for (int i = 0; i < 40; ++i)
        win_choose_add(a, "long");
    for (int high = 64; high <= 80; high += 16) {
        screen_height = high;
        win_choose_set_index(a, 41);
        assert(press(KB_A, 0, 0, 0) == 1);
        win_post_event(WIN_REPAINT_EVENT, NULL);
        assert(!hpclang_win_error());
        assert(press(KB_E, 0, 0, 0) == 1);
    }
    win_remove_widget(a);
    win_remove_widget(menu);
    win_widget_destroy(a);
    win_widget_destroy(b);
    win_widget_destroy(menu);
    assert(!hpclang_win_reset());
    return 0;
}
