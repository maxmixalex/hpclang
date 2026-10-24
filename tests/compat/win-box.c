#include <winsoftmenu.h>
#include <winbox.h>
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

struct child_size
{
    int width, height;
};
static void no_action(win_widget_t *widget)
{
    (void)widget;
}
static void preferred(win_widget_t *widget, int *width, int *height)
{
    struct child_size *size = win_wcore_get_data(widget);
    *width = size->width;
    *height = size->height;
}
static void snapshot(win_widget_t *widget)
{
    int x, y, w, h;
    win_widget_get_location(widget, &x, &y);
    win_widget_get_size(widget, &w, &h);
    printf("rect %d %d %d %d\n", x, y, w, h);
}
int main(void)
{
    assert(!hpclang_win_reset());
    for (int rows = 1; rows <= 3; ++rows)
        for (int cols = 1; cols <= 3; ++cols)
            for (int align = -2; align <= 1; ++align)
                for (int padding = 0; padding <= 3; ++padding) {
                    printf("case %d %d %d %d\n", rows, cols, align, padding);
                    win_widget_t *box =
                        padding ? win_box_new_padding(padding, padding + 1) : win_box_new();
                    win_widget_t *children[9];
                    struct child_size sizes[9];
                    int count = rows * cols;
                    for (int i = 0; i < count; ++i) {
                        sizes[i] = (struct child_size){3 + i * 2, 2 + i};
                        children[i] = win_wcore_alloc(no_action, no_action, no_action, preferred, 1,
                                                      &sizes[i]);
                        assert(children[i]);
                        if (!i)
                            win_box_add(box, children[i]);
                        else if (i == 1)
                            win_box_add_at(box, children[i], i / cols, i % cols);
                        else
                            win_box_add_with(box, children[i], i / cols, i % cols, (i % 3) + 1,
                                             (i % 2) + 1, i % 2 ? -1 : 5 + i, i % 2 ? 3 + i : -1,
                                             align, 1 - align - 2);
                    }
                    win_widget_pack(box);
                    snapshot(box);
                    for (int width = 1; width <= 80; width += 7)
                        for (int height = 1; height <= 60; height += 9) {
                            win_widget_set_location(box, -3, 5);
                            win_widget_set_size(box, width, height);
                            win_box_layout(box);
                            for (int i = 0; i < count; ++i)
                                snapshot(children[i]);
                            assert(!hpclang_win_error());
                        }
                    /* Last-cell removal can leave trailing zero-weight tracks in the
                     * original. Preferred sizing remains well-defined without layout. */
                    win_box_remove(box, children[count - 1]);
                    win_widget_pack(box);
                    snapshot(box);
                    win_widget_destroy(box);
                    for (int i = 0; i < count; ++i)
                        win_widget_destroy(children[i]);
                    assert(!hpclang_win_error() && !hpclang_win_reset());
                }
    return 0;
}
