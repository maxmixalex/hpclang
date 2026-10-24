#include <winimg.h>
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
static int image_token;
int hpg_get_width(hpg_t *image)
{
    assert(image == (hpg_t *)&image_token);
    return 4;
}
int hpg_get_height(hpg_t *image)
{
    assert(image == (hpg_t *)&image_token);
    return 3;
}
void hpg_blit(hpg_t *source, int sx, int sy, int width, int height, hpg_t *destination, int dx,
              int dy)
{
    assert(source == (hpg_t *)&image_token && destination == hpg_stdscreen);
    printf("blit %d %d %d %d %d %d\n", sx, sy, width, height, dx, dy);
}
int main(void)
{
    char bits[] = {0x55, (char)0xaa};
    hpg_pattern_t *pattern = hpg_alloc_pattern(bits, 2, 0);
    for (unsigned resources = 0; resources < 4; ++resources)
        for (unsigned transparent = 0; transparent < 2; ++transparent)
            for (unsigned geometry = 0; geometry < 4; ++geometry) {
                assert(!hpclang_win_reset());
                win_widget_t *widget = win_img_new(resources & 1 ? pattern : NULL,
                                                   resources & 2 ? (hpg_t *)&image_token : NULL);
                assert(widget);
                int w, h;
                win_widget_get_size(widget, &w, &h);
                printf("case %u %u %u preferred %d %d\n", resources, transparent, geometry, w, h);
                win_widget_set_location(widget, (int)geometry - 2, (int)geometry + 1);
                win_widget_set_size(widget, (int)geometry + 1, (int)geometry + 2);
                win_widget_set_colors(widget, 119, 34);
                win_widget_set_transparent(widget, transparent);
                win_add_widget(widget);
                assert(!win_post_event(WIN_REPAINT_EVENT, NULL));
                win_focus_next();
                printf("focus %d\n", win_widget_has_focus(widget));
                win_remove_widget(widget);
                puts("removed");
                assert(!win_post_event(WIN_REPAINT_EVENT, NULL));
                win_widget_destroy(widget);
                assert(!hpclang_win_error());
            }
    hpg_free_pattern(pattern);
    assert(!hpclang_win_reset());
    return 0;
}
