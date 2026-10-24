#include <wintext.h>
#include <wincore.h>
#include <winkeys.h>
#include <hpkeyb49.h>
#include "hpgcc-keyboard.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

/* The production window/widget/event/timer foundation is shared by candidate
 * and reference. Only text-control functions are exchanged. Device calls
 * record their arguments; production renderer pixels have separate tests. */
static uint64_t now;
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
    printf("color %u\n", color);
}
void hpg_fill_rect(int x, int y, int right, int bottom)
{
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
static void bytes(const char *text)
{
    for (const unsigned char *p = (const unsigned char *)text; *p; ++p)
        printf("%02x", *p);
}
void hpg_draw_text(char *text, int x, int y)
{
    printf("text %d %d ", x, y);
    bytes(text);
    puts("");
}
void hpg_draw_letter(char letter, int x, int y)
{
    printf("letter %u %d %d\n", (unsigned char)letter, x, y);
}
static win_widget_t *entry, *other;
static void state(void)
{
    printf("state %d ", win_widget_has_focus(entry));
    bytes(win_text_get_text(entry));
    printf(" %d ", win_widget_has_focus(other));
    bytes(win_text_get_text(other));
    puts("");
    assert(!hpclang_win_error());
    assert(!win_post_event(WIN_REPAINT_EVENT, NULL));
}
static void typed(unsigned keycode, unsigned ascii, unsigned left, unsigned right)
{
    win_keytyped_t event = {keycode, (char)ascii, left, right, 0};
    int result = win_post_event(WIN_KEY_TYPED, &event);
    printf("key %u %u %u %u %d\n", keycode, ascii, left, right, result);
    state();
}
static void dispose(win_widget_t *widget)
{
    win_remove_widget(widget);
    win_widget_destroy(widget);
}
int main(void)
{
    char *labels[] = {"", "\n", "AB", "A\nBC", "AB\n", "\n\n"};
    for (unsigned i = 0; i < sizeof(labels) / sizeof(labels[0]); ++i) {
        win_widget_t *label = win_text_new_label(labels[i]);
        assert(label);
        int w, h;
        win_widget_get_size(label, &w, &h);
        printf("label %u %d %d\n", i, w, h);
        win_add_widget(label);
        assert(!win_post_event(WIN_REPAINT_EVENT, NULL));
        dispose(label);
        assert(!hpclang_win_reset());
    }
    const int limits[] = {0, 1, 3, 8};
    for (unsigned scenario = 0; scenario < 4; ++scenario) {
        now = 0;
        printf("entry %d\n", limits[scenario]);
        entry = win_text_new_entry(limits[scenario]);
        other = win_text_new_entry(5);
        assert(entry && other);
        win_widget_set_location(entry, 2, 3);
        win_widget_set_location(other, 8, 12);
        win_add_widget(entry);
        win_add_widget(other);
        int w, h;
        win_widget_get_size(entry, &w, &h);
        printf("entry-size %d %d\n", w, h);
        for (unsigned ch = 'A'; ch < 'G'; ++ch)
            typed(KB_A, ch, 0, 0);
        typed(KB_LF, 0, 0, 0);
        typed(KB_BKS, 0, 0, 0);
        typed(KB_BKS, 0, 1, 1);
        typed(KB_BKS, 0, 0, 1);
        typed(KB_A, 0x80, 0, 0);
        typed(KB_A, 0xff, 0, 0);
        win_text_set_text(entry, "longer than the keyboard limit");
        state();
        typed(KB_A, 'Z', 0, 0);
        win_text_set_text(entry, "");
        state();
        now = 500000;
        assert(hpclang_win_step(1) == 1);
        state();
        uint32_t random = 9817 + scenario;
        for (unsigned i = 0; i < 128; ++i) {
            random = random * UINT32_C(1664525) + UINT32_C(1013904223);
            const unsigned keys[] = {KB_LF, KB_RT, KB_UP, KB_DN, KB_BKS, KB_A};
            unsigned action = (random >> 16) % 6;
            typed(keys[action], action == 5 ? (random % 255) + 1 : 0, (random >> 9) & 3,
                  (random >> 11) & 3);
            if (i == 64) {
                now = 1000000;
                assert(hpclang_win_step(1) == 1);
            }
        }
        dispose(other);
        dispose(entry);
        assert(!hpclang_win_reset());
    }
    return 0;
}
