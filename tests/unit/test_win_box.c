#include <winsoftmenu.h>
#include <winbox.h>
#include "win-internal.h"
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
#include <limits.h>

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
struct observed
{
    int installs, removes, destroys, fail, width, height;
};
static void installed(win_widget_t *widget)
{
    struct observed *value = win_wcore_get_data(widget);
    ++value->installs;
    if (value->fail)
        hpclang_win_fail(EIO);
}
static void removed(win_widget_t *widget)
{
    struct observed *value = win_wcore_get_data(widget);
    ++value->removes;
}
static void destroyed(win_widget_t *widget)
{
    struct observed *value = win_wcore_get_data(widget);
    ++value->destroys;
}
static void preferred(win_widget_t *widget, int *width, int *height)
{
    struct observed *value = win_wcore_get_data(widget);
    *width = value->width;
    *height = value->height;
}
static win_widget_t *child(struct observed *value)
{
    return win_wcore_alloc(installed, removed, destroyed, preferred, 1, value);
}
static void rectangle(win_widget_t *widget, int ex, int ey, int ew, int eh)
{
    int x, y, w, h;
    win_widget_get_location(widget, &x, &y);
    win_widget_get_size(widget, &w, &h);
    assert(x == ex && y == ey && w == ew && h == eh);
}
int main(void)
{
    assert(!hpclang_win_reset());
    struct observed av = {.width = 10, .height = 6}, bv = {.width = 20, .height = 8},
                    cv = {.width = 4, .height = 5};
    win_widget_t *a = child(&av), *b = child(&bv), *c = child(&cv),
                 *box = win_box_new_padding(2, 3);
    assert(a && b && c && box);
    win_box_add_with(box, a, 0, 0, 1, 1, -1, -1, WIN_BOX_FILL, WIN_BOX_LEADING);
    win_box_add_with(box, b, 0, 1, 2, 1, -1, -1, WIN_BOX_FILL, WIN_BOX_CENTER);
    win_widget_pack(box);
    rectangle(box, 0, 0, 32, 8);
    win_widget_set_size(box, 50, 20);
    win_widget_set_location(box, 3, 4);
    win_box_layout(box);
    rectangle(a, 3, 4, 16, 6);
    rectangle(b, 21, 10, 32, 8);
    win_add_widget(box);
    assert(!hpclang_win_error() && av.installs == 1 && bv.installs == 1 && win_widget_has_focus(a));
    win_focus_next();
    assert(win_widget_has_focus(b));
    win_box_add_at(box, c, 1, 0);
    assert(cv.installs == 1 && hpclang_win_widget_installed(c));
    win_box_remove(box, c);
    assert(cv.removes == 1 && !hpclang_win_widget_installed(c));
    win_widget_destroy(c);
    assert(cv.destroys == 1);
    win_widget_destroy(a);
    assert(hpclang_win_error() == EBUSY && av.destroys == 0);
    win_remove_widget(box);
    assert(av.removes == 1 && bv.removes == 1);
    assert(!hpclang_win_reset());
    win_widget_destroy(box); /* release borrowed children, without destroying them */
    assert(!av.destroys && !bv.destroys);
    win_widget_destroy(a);
    win_widget_destroy(b);
    assert(av.destroys == 1 && bv.destroys == 1);
    assert(!hpclang_win_reset());
    /* A nested group installs/removes without recursive focus-ring mutation. */
    av = (struct observed){.width = 2, .height = 2};
    a = child(&av);
    box = win_box_new();
    win_widget_t *inner = win_box_new();
    win_box_add(inner, a);
    win_box_add(box, inner);
    win_box_add(inner, box);
    assert(hpclang_win_error() == EINVAL);
    assert(!hpclang_win_reset());
    win_add_widget(box);
    assert(av.installs == 1 && win_widget_has_focus(a));
    win_remove_widget(box);
    assert(av.removes == 1);
    win_widget_destroy(box);
    win_widget_destroy(inner);
    win_widget_destroy(a);
    assert(!hpclang_win_reset());
    /* Failed child installation rolls back every already-installed node. */
    av = (struct observed){.width = 2, .height = 2};
    bv = (struct observed){.width = 2, .height = 2, .fail = 1};
    a = child(&av);
    b = child(&bv);
    box = win_box_new();
    win_box_add(box, a);
    win_box_add(box, b);
    win_add_widget(box);
    assert(hpclang_win_error() == EIO && av.installs == 1 && bv.installs == 1);
    assert(av.removes == 1 && bv.removes == 1 && !hpclang_win_widget_installed(box));
    assert(!hpclang_win_widget_installed(a) && !hpclang_win_widget_installed(b));
    assert(!hpclang_win_reset());
    win_widget_destroy(box);
    win_widget_destroy(a);
    win_widget_destroy(b);
    /* Dynamic installation failure leaves the parent and existing child alone,
     * and releases the failed child's binding so it can be destroyed safely. */
    av = (struct observed){.width = 2, .height = 2};
    bv = (struct observed){.width = 2, .height = 2, .fail = 1};
    a = child(&av);
    b = child(&bv);
    box = win_box_new();
    win_box_add(box, a);
    win_add_widget(box);
    win_box_add(box, b);
    assert(hpclang_win_error() == EIO && hpclang_win_widget_installed(box));
    assert(av.installs == 1 && av.removes == 0 && bv.installs == 1 && bv.removes == 1);
    win_widget_destroy(b);
    assert(bv.destroys == 1);
    win_remove_widget(box);
    assert(!hpclang_win_reset());
    win_widget_destroy(box);
    win_widget_destroy(a);
    /* Layout failure must not publish even the already-computed first axis. */
    av = (struct observed){.width = INT_MAX, .height = 2};
    bv = (struct observed){.width = 1, .height = 2};
    a = child(&av);
    b = child(&bv);
    box = win_box_new();
    win_box_add_at(box, a, 0, 0);
    win_box_add_at(box, b, 0, 1);
    win_widget_set_size(box, 30, 20);
    win_box_layout(box);
    assert(hpclang_win_error() == EOVERFLOW);
    rectangle(a, 0, 0, INT_MAX, 2);
    rectangle(b, 0, 0, 1, 2);
    assert(!hpclang_win_reset());
    win_widget_destroy(box);
    win_widget_destroy(a);
    win_widget_destroy(b);
    /* Placeholders contribute preferred sizes and layout must terminate. */
    box = win_box_new_padding(3, 2);
    win_box_add_with(box, NULL, 0, 0, 0, 0, 7, 4, WIN_BOX_CENTER, WIN_BOX_CENTER);
    win_box_add_with(box, NULL, 1, 1, 0, 0, 9, 5, WIN_BOX_CENTER, WIN_BOX_CENTER);
    win_widget_pack(box);
    rectangle(box, 0, 0, 19, 11);
    win_box_layout(box);
    win_add_widget(box);
    win_remove_widget(box);
    win_box_remove(box, NULL);
    win_widget_pack(box);
    rectangle(box, 0, 0, 12, 7);
    win_widget_destroy(box);
    assert(!hpclang_win_error() && !hpclang_win_reset());
    return 0;
}
