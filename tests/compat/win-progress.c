#include <winprogress.h>
#include <wincore.h>
#include <winkeys.h>
#include <hpkeyb49.h>
#include "hpgcc-keyboard.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

/* The production window/widget/event/timer foundation is shared by candidate
 * and reference. Only progress-control functions are exchanged. Device calls
 * record their arguments; production renderer pixels have separate tests. */
static uint64_t now;
static int font_token;
static hpg_pattern_t *current_pattern;
static win_widget_t *bar, *other;
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
static int change(void *event, void *data)
{
    assert(!data && (event == bar || event == other));
    printf("change %d %.6f\n", event == bar ? 1 : 2, win_progress_get_value(event));
    return 0;
}
static void status(void)
{
    printf("state %.6f %d", win_progress_get_value(bar), win_widget_has_focus(bar));
    if (other)
        printf(" %.6f %d", win_progress_get_value(other), win_widget_has_focus(other));
    puts("");
    assert(!hpclang_win_error());
}
static void key(int keycode, int down)
{
    int result =
        win_post_event(down ? WIN_KEY_PRESSED : WIN_KEY_RELEASED, (void *)(uintptr_t)keycode);
    printf("key %d %d %d\n", keycode, down, result);
    status();
}
static void advance(uint64_t milliseconds)
{
    now = milliseconds * 1000;
    printf("time %llu\n", (unsigned long long)milliseconds);
    assert(hpclang_win_step(1) == 1);
    status();
}
int main(void)
{
    const int inc[] = {KB_RT, KB_UP, KB_LF, KB_DN};
    const int dec[] = {KB_LF, KB_DN, KB_RT, KB_UP};
    const double minimum[] = {0, -30, 1.5}, maximum[] = {100, 30, 4.5};
    for (int slider = 0; slider < 2; ++slider) {
        for (int direction = 0; direction < 4; ++direction) {
            for (int range = 0; range < 3; ++range) {
                printf("scenario %d %d %d\n", slider, direction, range);
                now = 0;
                assert(!hpclang_win_reset());
                int changed = win_register_event();
                win_add_event_handler(changed, 0, change, NULL);
                bar = win_progress_new(slider);
                other = NULL;
                assert(bar);
                win_progress_set_event(bar, changed);
                win_progress_set_orient(bar, direction);
                win_progress_set_range(bar, minimum[range], maximum[range]);
                double increment = (maximum[range] - minimum[range]) / 10;
                win_progress_set_inc(bar, increment);
                win_progress_set_delay(bar, 10, 5);
                win_widget_pack(bar);
                int width, height;
                win_widget_get_size(bar, &width, &height);
                printf("size %d %d\n", width, height);
                win_widget_set_location(bar, 3, 4);
                win_widget_set_size(bar, 13, 9);
                win_add_widget(bar);
                for (int n = -1; n <= 11; ++n) {
                    win_progress_set_value(bar, minimum[range] + n * increment);
                    status();
                    assert(!win_post_event(WIN_REPAINT_EVENT, NULL));
                }
                win_progress_set_value(bar, minimum[range]);
                key(inc[direction], 1);
                advance(9);
                advance(10);
                advance(15);
                advance(30);
                key(KB_A, 0); /* Historical release of any key cancels repeat. */
                advance(500);
                assert(!win_post_event(WIN_REPAINT_EVENT, NULL));
                key(dec[direction], 1);
                key(dec[direction], 0);
                other = win_progress_new(1);
                assert(other);
                win_progress_set_event(other, changed);
                win_add_widget(other);
                win_progress_set_value(bar, maximum[range]);
                key(inc[direction], 1);
                key(inc[direction], 0);
                win_focus_prev();
                status();
                win_progress_set_range(bar, 20, 10);
                status();
                win_remove_widget(other);
                win_widget_destroy(other);
                other = NULL;
                win_remove_widget(bar);
                win_widget_destroy(bar);
                bar = NULL;
                assert(!hpclang_win_error() && !hpclang_win_reset());
            }
        }
    }
    return 0;
}
