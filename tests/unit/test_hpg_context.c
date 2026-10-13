#include <hpgraphics.h>
#include <hpclang/display.h>
#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stddef.h>
#include <hpclang/font-package.h>

/* Distinct font providers let this test observe adapter selection and HP-byte
 * translation independently of the clean font renderer's own raster tests. */
static const uint8_t wide_bits[] = {0xff, 0x80};
static const struct hpclang_bitmap_glyph wide_glyph = {
    .codepoint = 0x03c9, .bitmap = wide_bits, .width = 9, .height = 1, .stride = 2};
static const struct hpclang_bitmap_glyph *const fixture_glyphs[] = {&wide_glyph};
static const struct hpclang_bitmap_font_data fixture_data = {.glyphs = fixture_glyphs,
                                                             .glyph_count = 1};
const struct hpclang_font hpclang_font_newrpl_font6_menus_full = HPCLANG_BITMAP_FONT(fixture_data);
const struct hpclang_font hpclang_font_newrpl_font8_styleb_full = HPCLANG_BITMAP_FONT(fixture_data);
static const struct hpclang_font native_mini = {1, 2, NULL}, native_big = {1, 3, NULL};
static int classic, available;
static const struct hpclang_font *drawn_font;
static uint32_t drawn_code;
const struct hpclang_font *hpclang_font_big(void)
{
    return classic ? &native_big : NULL;
}
const struct hpclang_font *hpclang_font_mini(void)
{
    return &native_mini;
}
const struct hpclang_font *hpclang_font_default(void)
{
    return &native_big;
}
int hpclang_font_available(const struct hpclang_font *font)
{
    return font && available;
}
void hpclang_surface_draw_codepoint(struct hpclang_surface *s, const struct hpclang_font *f,
                                    int32_t x, int32_t y, uint32_t cp, uint8_t color)
{
    drawn_font = f;
    drawn_code = cp;
    hpclang_surface_set_pixel(s, x, y, color);
}
void hpclang_surface_draw_char(struct hpclang_surface *s, const struct hpclang_font *f, int32_t x,
                               int32_t y, uint8_t cp, uint8_t color)
{
    hpclang_surface_draw_codepoint(s, f, x, y, cp, color);
}

static void defaults(void)
{
    hpg_t *g = hpg_alloc_mono_image(16, 16);
    assert(g);
    hpg_draw_text_on(g, "AB\nC", 1, 1);
    assert(hpg_get_font(g) == hpg_get_minifont());
    assert(hpg_get_pixel(g, 1, 1) == 255 && hpg_get_pixel(g, 5, 1) == 255);
    assert(hpg_get_pixel(g, 1, 7) == 255);
    assert(drawn_font == &hpclang_font_newrpl_font6_menus_full);
    hpg_draw_letter_on(g, (char)0x87, 1, 1);
    assert(drawn_code == 0x03c0);
    classic = available = 1;
    hpg_draw_letter_on(g, (char)0x87, 1, 1);
    assert(drawn_font == &native_mini && drawn_code == 0x87);
    classic = 0;
    hpg_draw_letter_on(g, 'A', 1, 1);
    assert(drawn_font == &hpclang_font_newrpl_font6_menus_full);
    hpg_set_font(g, hpg_get_bigfont());
    assert(hpg_font_get_advance(hpg_get_bigfont()) == 6);
    assert(hpg_font_get_height(hpg_get_bigfont()) == 8);
    hpg_draw_letter_on(g, 'A', 1, 1);
    assert(drawn_font == &native_big);
    available = 0;
    hpg_draw_letter_on(g, 'A', 1, 1);
    assert(drawn_font == &hpclang_font_newrpl_font8_styleb_full);
    hpg_clear_on(g);
    hpg_draw_letter_on(g, (char)0x9a, 2, 3);
    for (int x = 0; x < 16; ++x)
        assert(hpg_get_pixel(g, x, 3) == (x >= 2 && x < 8 ? 255 : 0));
    hpg_set_mode(g, HPG_MODE_XOR);
    hpg_draw_letter_on(g, (char)0x9a, 2, 3);
    for (int x = 0; x < 16; ++x)
        assert(hpg_get_pixel(g, x, 3) == 0);
    hpg_draw_letter_on(g, (char)0x80, 2, 3);
    assert(hpg_get_pixel(g, 2, 9) == 255 && hpg_get_pixel(g, 6, 5) == 255);
    hpg_free_font(hpg_get_minifont());
    hpg_free_font(hpg_get_bigfont());
    hpg_free_image(g);
}

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

static void text(void)
{
    char bits[256 * 3] = {0};
    bits[65 * 3] = 1;
    bits[65 * 3 + 1] = 2;
    bits[65 * 3 + 2] = 4;
    bits[255 * 3] = 4;
    bits[255 * 3 + 1] = 2;
    bits[255 * 3 + 2] = 1;
    hpg_font_t *font = hpg_alloc_font(bits, 256, 3, 4);
    hpg_t *g = hpg_alloc_mono_image(16, 10);
    char zero = 0;
    hpg_pattern_t *pattern = hpg_alloc_pattern(&zero, 1, 1);
    assert(font && g && pattern);
    hpg_set_font(g, font);
    hpg_set_pattern(g, pattern);
    char message[] = "A\n\xff";
    hpg_draw_text_on(g, message, 1, 1);
    assert(hpg_get_pixel(g, 1, 1) == 255 && hpg_get_pixel(g, 2, 2) == 255);
    assert(hpg_get_pixel(g, 3, 4) == 255 && hpg_get_pixel(g, 1, 6) == 255);
    assert(hpg_get_pixel(g, 5, 4) == 0);
    assert(hpg_get_pattern(g) == pattern && hpg_get_font(g) == font);
    hpg_set_mode(g, HPG_MODE_XOR);
    hpg_draw_text_on(g, message, 1, 1);
    for (int y = 0; y < 10; ++y)
        for (int x = 0; x < 16; ++x)
            assert(hpg_get_pixel(g, x, y) == 0);
    hpg_draw_letter_on(g, 'A', -1, -1);
    assert(hpg_get_pixel(g, 0, 0) == 255 && hpg_get_pixel(g, 1, 1) == 255);
    hpg_draw_text_on(g, message, INT_MAX, INT_MAX);
    bits[0] = 1;
    hpg_draw_letter_on(g, 0, 5, 0);
    assert(hpg_get_pixel(g, 5, 0) == 255); /* Borrowed data, including direct NUL. */
    hpg_set_font(g, NULL);
    hpg_free_font(font);
    char small_bits[] = {1, 1};
    font = hpg_alloc_font(small_bits, 2, 1, 12);
    assert(font);
    hpg_set_font(g, font);
    hpg_clear_on(g);
    char outside[] = {3, 1, 0};
    hpg_draw_text_on(g, outside, 1, 2);
    assert(hpg_get_pixel(g, 1, 2) == 255 && hpg_get_pixel(g, 9, 2) == 255);
    assert(hpg_get_pixel(g, 13, 2) == 0); /* Out-of-font byte did not advance. */
    hpg_set_font(g, NULL);
    hpg_set_pattern(g, NULL);
    hpg_free_font(font);
    hpg_free_pattern(pattern);
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
    text();
    defaults();
    return 0;
}
