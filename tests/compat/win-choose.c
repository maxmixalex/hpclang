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
static int press(unsigned code, unsigned left, unsigned right, unsigned alpha)
{
    win_keytyped_t key = {code, 0, left, right, alpha};
    return win_post_event(WIN_KEY_TYPED, &key);
}

void hpg_clip_set(hpg_t *g, int x, int y, int right, int bottom)
{
    hpg_clip(g, x, y, right, bottom);
}
void hpg_draw_line(int x, int y, int right, int bottom)
{
    (void)x;
    (void)y;
    (void)right;
    (void)bottom;
}
static void snapshot(win_widget_t *widget)
{
    int index = win_choose_get_index(widget);
    char *choice = win_choose_get_choice(widget);
    printf("state %d %s\n", index, choice ? choice : "(none)");
}
int main(void)
{
    assert(!hpclang_win_reset());
    char *options[] = {"zero", "one", "two", "three", "one", "five"};
    char *labels[] = {"", "A", "BC", "D\nEF", "G\n", "HIJ"};
    for (unsigned count = 0; count <= 6; ++count) {
        printf("case %u\n", count);
        win_widget_t *a = win_choose_new(), *b = win_choose_new(), *menu = win_softmenu_new();
        assert(a && b && menu);
        snapshot(a);
        for (unsigned i = 0; i < count; ++i) {
            if (i & 1)
                win_choose_add_with_label(a, options[i], labels[i]);
            else
                win_choose_add(a, options[i]);
            int w, h;
            win_widget_pack(a);
            win_widget_get_size(a, &w, &h);
            printf("size %d %d\n", w, h);
            snapshot(a);
        }
        for (int i = -2; i <= (int)count + 2; ++i) {
            win_choose_set_index(a, i);
            snapshot(a);
        }
        for (unsigned i = 0; i < 6; ++i) {
            win_choose_set_choice(a, options[i]);
            snapshot(a);
        }
        win_choose_set_choice(a, NULL);
        snapshot(a);
        win_choose_set_choice(a, "absent");
        snapshot(a);
        win_choose_set_index(a, 0);
        win_add_widget(a);
        win_add_widget(b);
        for (unsigned iteration = 0; iteration < 80; ++iteration) {
            unsigned modifiers = iteration % 8;
            printf("key %u %d\n", iteration,
                   press(KB_W, modifiers & 1, !!(modifiers & 2), !!(modifiers & 4)));
            snapshot(a);
        }
        /* Both implementations support established focus/menu binding. The
         * original first-install and popup lifecycle bugs are tested separately. */
        win_choose_set_menu(a, menu, 0);
        win_focus_next();
        win_focus_next();
        win_choose_set_menu(a, NULL, 0);
        win_remove_widget(b);
        win_remove_widget(a);
        /* Removal's numeric selection is compatible. Original label shifting
         * is defective, so repaired post-removal painting is covered by pixels. */
        for (unsigned i = 0; i < 6; ++i) {
            win_choose_set_index(a, (int)(i % 3));
            win_choose_remove(a, options[i]);
            snapshot(a);
        }
        win_choose_remove(a, "absent");
        snapshot(a);
        win_widget_destroy(a);
        win_widget_destroy(b);
        win_widget_destroy(menu);
        assert(!hpclang_win_error());
    }
    assert(!hpclang_win_reset());
    return 0;
}
