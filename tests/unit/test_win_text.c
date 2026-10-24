#undef malloc
#include <stdlib.h>
#include <wintext.h>
#include <wincore.h>
#include <winkeys.h>
#include <hpkeyb49.h>
#include "hpgcc-keyboard.h"
#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>

static int fail_allocation;
void *hpclang_test_malloc(size_t size)
{
    if (fail_allocation)
        return NULL;
    return malloc(size);
}
static uint64_t now;
static int font_token, draws, glyph_x, glyph_y, glyph;
static char drawn[8][32];
static int drawn_x[8];
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
    (void)data;
    return 0;
}
int hpclang_hpg_frame_present(void *data)
{
    (void)data;
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
void hpg_clip_reset(hpg_t *g)
{
    (void)g;
}
void hpg_clip(hpg_t *g, int x, int y, int right, int bottom)
{
    (void)g;
    (void)x;
    (void)y;
    (void)right;
    (void)bottom;
}
void hpg_set_color(hpg_t *g, unsigned char color)
{
    (void)g;
    (void)color;
}
void hpg_fill_rect(int x, int y, int right, int bottom)
{
    (void)x;
    (void)y;
    (void)right;
    (void)bottom;
}
void hpg_set_font(hpg_t *g, hpg_font_t *font)
{
    (void)g;
    (void)font;
}
void hpg_set_pattern(hpg_t *g, hpg_pattern_t *pattern)
{
    (void)g;
    (void)pattern;
}
void hpg_set_mode(hpg_t *g, unsigned char mode)
{
    (void)g;
    (void)mode;
}
void hpg_draw_text(char *text, int x, int y)
{
    (void)y;
    assert(draws < 8 && strlen(text) < sizeof(drawn[0]));
    strcpy(drawn[draws], text);
    drawn_x[draws++] = x;
}
void hpg_draw_letter(char letter, int x, int y)
{
    glyph = (unsigned char)letter;
    glyph_x = x;
    glyph_y = y;
}
static void typed(int key, unsigned ascii, unsigned left, unsigned right)
{
    win_keytyped_t event = {key, ascii, left, right, 0};
    assert(win_post_event(WIN_KEY_TYPED, &event) == 1 && !hpclang_win_error());
}
static void cleanup(win_widget_t *widget)
{
    win_remove_widget(widget);
    win_widget_destroy(widget);
}
int main(void)
{
    win_widget_t *label = win_text_new_label("a\nbc\n");
    win_widget_t *entry = win_text_new_entry(3);
    assert(label && entry);
    int width, height;
    win_widget_get_size(label, &width, &height);
    assert(width == 9 && height == 13);
    win_widget_get_size(entry, &width, &height);
    assert(width == 17 && height == 7);
    win_add_widget(label);
    win_add_widget(entry);
    assert(win_widget_has_focus(entry));
    for (unsigned ch = 'a'; ch <= 'd'; ++ch)
        typed(KB_A, ch, 0, 0);
    assert(!strcmp(win_text_get_text(entry), "abc"));
    typed(KB_LF, 0, 0, 0);
    typed(KB_BKS, 0, 0, 0);
    assert(!strcmp(win_text_get_text(entry), "ac"));
    typed(KB_BKS, 0, 1, 0);
    assert(!strcmp(win_text_get_text(entry), "a"));
    typed(KB_BKS, 0, 1, 1);
    assert(!strcmp(win_text_get_text(entry), "a"));
    typed(KB_BKS, 0, 0, 1);
    assert(!strcmp(win_text_get_text(entry), ""));
    win_text_set_text(entry, "longer");
    typed(KB_A, 'X', 0, 0);
    assert(!strcmp(win_text_get_text(entry), "longer"));
    win_text_set_text(entry, win_text_get_text(entry) + 3);
    assert(!strcmp(win_text_get_text(entry), "ger"));
    typed(KB_RT, 0, 0, 0);
    typed(KB_RT, 0, 0, 0);
    win_text_get_text(entry)[1] = 0;
    typed(KB_A, 'X', 0, 0);
    assert(!strcmp(win_text_get_text(entry), "gX"));
    win_text_set_text(entry, "abc");
    typed(KB_LF, 0, 0, 0); /* Cursor 2 -> 1. */
    now = 500000;
    assert(hpclang_win_step(1) == 1);
    draws = 0;
    assert(!win_post_event(WIN_REPAINT_EVENT, NULL));
    assert(draws == 3 && !strcmp(drawn[0], "a") && !strcmp(drawn[1], "c"));
    assert(drawn_x[0] == 1 && drawn_x[1] == 9 && glyph == 28 && glyph_x == 5 && glyph_y == 1);
    assert(!strcmp(win_text_get_text(entry), "abc"));
    win_widget_t *other = win_text_new_entry(2);
    assert(other);
    win_add_widget(other);
    typed(KB_DN, 0, 0, 0);
    assert(win_widget_has_focus(other));
    typed(KB_UP, 0, 0, 0);
    assert(win_widget_has_focus(entry));
    cleanup(other);
    fail_allocation = 1;
    char *before = win_text_get_text(entry);
    win_text_set_text(entry, "this replacement must fail");
    assert(hpclang_win_error() == ENOMEM && win_text_get_text(entry) == before);
    assert(!strcmp(before, "abc"));
    cleanup(entry);
    cleanup(label);
    assert(!hpclang_win_reset());
    assert(!win_text_new_entry(4) && hpclang_win_error() == ENOMEM);
    assert(!hpclang_win_reset());
    fail_allocation = 0;
    label = win_text_new_label("");
    assert(label);
    win_widget_get_size(label, &width, &height);
    assert(width == 1 && height == 1);
    cleanup(label);
    assert(!win_text_new_entry(-1) && hpclang_win_error() == EINVAL);
    assert(!hpclang_win_reset());
    now = 2000000;
    draws = 0;
    assert(hpclang_win_step(1) == 1 && draws == 0);
    return 0;
}
