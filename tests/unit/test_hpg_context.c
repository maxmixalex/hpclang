#include <hpgraphics.h>
#include <hpclang/display.h>
#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stddef.h>

static struct hpclang_surface *display;
static unsigned borrowed;
struct hpclang_surface *hpclang_display_surface(void)
{
    ++borrowed;
    return display;
}

static void copies(void)
{
    hpg_t *g = hpg_alloc_gray16_image(8, 6);
    assert(g);
    unsigned char before[48];
    for (int mode = 0; mode < 2; ++mode)
        for (int direction = 0; direction < 4; ++direction) {
            hpg_set_mode(g, HPG_MODE_PAINT);
            for (int y = 0; y < 6; ++y)
                for (int x = 0; x < 8; ++x) {
                    unsigned char value = (unsigned char)(((y * 8 + x) % 16) * 17);
                    before[y * 8 + x] = value;
                    hpg_set_color(g, value);
                    hpg_draw_pixel_on(g, x, y);
                }
            int sx = direction & 1 ? 2 : 0, dx = 2 - sx;
            int sy = direction & 2 ? 2 : 0, dy = 2 - sy;
            hpg_set_color(g, 51);
            hpg_set_mode(g, (unsigned char)mode);
            hpg_blit(g, sx, sy, 6, 4, g, dx, dy);
            assert(hpg_get_color(g) == 51);
            for (int y = 0; y < 6; ++y)
                for (int x = 0; x < 8; ++x) {
                    unsigned char expected = before[y * 8 + x];
                    if (x >= dx && x < dx + 6 && y >= dy && y < dy + 4) {
                        unsigned char source = before[(y - dy + sy) * 8 + x - dx + sx];
                        expected = mode ? source ^ expected : source;
                    }
                    assert(hpg_get_pixel(g, x, y) == expected);
                }
        }
    hpg_t *mono = hpg_alloc_mono_image(8, 6);
    assert(mono);
    hpg_set_mode(g, HPG_MODE_PAINT);
    hpg_set_color(g, 170);
    hpg_fill_rect_on(g, 0, 0, 7, 5);
    hpg_clip(g, 0, 0, 1, 1); /* Source clip does not restrict reading. */
    hpg_clip(mono, 2, 1, 6, 5);
    hpg_blit(g, 0, 0, INT_MAX, INT_MAX, mono, 0, 0);
    for (int y = 0; y < 6; ++y)
        for (int x = 0; x < 8; ++x)
            assert(hpg_get_pixel(mono, x, y) == (x >= 2 && x < 6 && y >= 1 && y < 5 ? 255 : 0));
    hpg_blit(g, INT_MIN, 0, INT_MAX, 1, mono, INT_MAX, 0);
    hpg_free_image(mono);
    hpg_free_image(g);
}

int main(void)
{
    hpg_t *g = hpg_alloc_gray4_image(8, 6);
    assert(g && borrowed == 0 && hpg_get_width(g) == 8 && hpg_get_height(g) == 6);
    assert(hpg_get_color(g) == HPG_COLOR_BLACK && hpg_get_mode(g) == HPG_MODE_PAINT);
    assert(hpg_get_pixel(g, 0, 0) == 0);
    hpg_set_color(g, 170);
    hpg_fill_rect_on(g, INT_MIN, INT_MIN, INT_MAX, INT_MAX);
    assert(hpg_get_pixel(g, 7, 5) == 170);
    hpg_clip_set(g, 0, 0, 8, 6);
    hpg_set_color(g, 85);
    hpg_fill_rect_on(g, 0, 0, 7, 5);
    assert(hpg_get_pixel(g, 6, 4) == 85 && hpg_get_pixel(g, 7, 5) == 170);
    hpg_clip_reset(g);
    hpg_set_mode(g, HPG_MODE_XOR);
    hpg_draw_pixel_on(g, 7, 5);
    assert(hpg_get_pixel(g, 7, 5) == 255);
    hpg_set_mode(g, 3);
    assert(errno == EINVAL && hpg_get_mode(g) == HPG_MODE_XOR);
    hpg_clear_on(g); /* Whole image, independent of current ink/mode/clip. */
    assert(hpg_get_pixel(g, 7, 5) == 0);
    char pattern[] = {1, 2};
    hpg_pattern_t *p = hpg_alloc_pattern(pattern, 2, 0);
    assert(p);
    hpg_set_pattern(g, p);
    assert(hpg_get_pattern(g) == p);
    hpg_set_mode(g, HPG_MODE_PAINT);
    hpg_set_color(g, 255);
    hpg_fill_rect_on(g, 2, 1, 7, 5);
    assert(hpg_get_pixel(g, 2, 1) == 255 && hpg_get_pixel(g, 3, 2) == 255);
    assert(hpg_get_pixel(g, 3, 1) == 0);
    pattern[0] = 4;
    hpg_clear_on(g);
    hpg_fill_rect_on(g, 2, 1, 7, 5);
    assert(hpg_get_pixel(g, 4, 1) == 255 && hpg_get_pixel(g, 2, 1) == 0);
    hpg_set_pattern(g, NULL);
    hpg_free_pattern(p);
    hpg_clear_on(g);
    hpg_set_mode(g, HPG_MODE_XOR);
    hpg_draw_rect_on(g, 1, 1, 6, 4);
    assert(hpg_get_pixel(g, 1, 1) == 255 && hpg_get_pixel(g, 6, 4) == 255);
    assert(hpg_get_pixel(g, 3, 2) == 0);
    hpg_draw_rect_on(g, 6, 4, 1, 1);
    for (int y = 0; y < 6; ++y)
        for (int x = 0; x < 8; ++x)
            assert(hpg_get_pixel(g, x, y) == 0);
    hpg_clip(g, 2, 1, 6, 5);
    hpg_draw_line_on(g, INT_MIN, 3, INT_MAX, 3);
    for (int x = 0; x < 8; ++x)
        assert(hpg_get_pixel(g, x, 3) == (x >= 2 && x < 6 ? 255 : 0));
    hpg_clip_reset(g);
    hpg_clear_on(g);
    hpg_fill_circle_on(g, 3, 3, 2);
    assert(hpg_get_pixel(g, 3, 3) == 255 && hpg_get_pixel(g, 1, 3) == 255);
    assert(hpg_get_pixel(g, 0, 0) == 0);
    hpg_fill_circle_on(g, 3, 3, 2);
    assert(hpg_get_pixel(g, 3, 3) == 0 && hpg_get_pixel(g, 1, 3) == 0);
    hpg_draw_circle_on(g, 3, 3, 2);
    assert(hpg_get_pixel(g, 3, 3) == 0 && hpg_get_pixel(g, 1, 3) == 255);
    hpg_draw_circle_on(g, INT_MAX, INT_MAX, INT_MAX);
    hpg_clear_on(g);
    int vx[] = {1, 6, 1}, vy[] = {1, 1, 5};
    hpg_fill_polygon_on(g, vx, vy, 3);
    assert(hpg_get_pixel(g, 2, 2) == 255 && hpg_get_pixel(g, 7, 5) == 0);
    hpg_fill_polygon_on(g, vx, vy, 3);
    assert(hpg_get_pixel(g, 2, 2) == 0 && vx[1] == 6 && vy[2] == 5);
    hpg_draw_polygon_on(g, vx, vy, 3);
    assert(hpg_get_pixel(g, 1, 1) == 255 && hpg_get_pixel(g, 2, 2) == 0);
    hpg_fill_polygon_on(g, NULL, vy, 3);
    assert(errno == EINVAL);
    char font_bits[] = {1, 2, 4, 1, 2, 4};
    hpg_font_t *f = hpg_alloc_font(font_bits, 2, 3, 4);
    assert(f && hpg_font_get_height(f) == 3 && hpg_font_get_advance(f) == 4);
    hpg_set_font(g, f);
    assert(hpg_get_font(g) == f);
    hpg_set_font(g, NULL);
    hpg_free_font(f);
    assert(!hpg_alloc_font(font_bits, 257, 3, 4) && errno == EINVAL);
    assert(!hpg_alloc_pattern(pattern, 0, 0) && errno == EINVAL);
    assert(!hpg_alloc_mono_image(0, 1) && errno == EINVAL);
    hpg_free_image(g);
    display = hpclang_surface_create_mono(8, 6);
    assert(display);
    hpg_set_color(NULL, 255);
    hpg_draw_pixel(1, 1);
    assert(hpclang_surface_get_pixel(display, 1, 1) == 255);
    hpg_free_image(HPG_STDSCREEN);
    assert(errno == EINVAL && hpg_get_width(NULL) == 8);
    hpg_cleanup();
    assert(hpclang_surface_get_pixel(display, 1, 1) == 255);
    hpclang_surface_destroy(display);
    display = NULL;
    assert(hpg_get_width(NULL) == 0 && errno == ENODEV);
    copies();
    return 0;
}
