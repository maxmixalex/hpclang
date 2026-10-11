#include <assert.h>
#include <errno.h>
#include <string.h>
#include <hpclang/abi.h>
#include <hpclang/console.h>
#include <hpclang/display.h>
#include <hpclang/font-package.h>

const struct hpclang_boot_info *hpclang_boot_info(void)
{
    return NULL;
}
/* Deliberately wider than its nominal cell: rendering must clip, not bleed. */
static const uint8_t pixels[] = {0xf8, 0xf8, 0xf8};
static const struct hpclang_bitmap_glyph glyph = {
    .codepoint = 'A', .bitmap = pixels, .width = 5, .height = 3, .advance = 3, .stride = 1};
static const struct hpclang_bitmap_glyph *const glyphs[] = {&glyph};
static const struct hpclang_bitmap_font_data data = {.glyphs = glyphs,
                                                     .glyph_count = 1,
                                                     .fallback_codepoint = 'A',
                                                     .ascent = 3,
                                                     .line_height = 4,
                                                     .nominal_advance = 3};
static const struct hpclang_font font = HPCLANG_BITMAP_FONT(data);
static uint32_t cell(struct hpclang_console *c, unsigned col, unsigned row)
{
    uint32_t cp;
    assert(hpclang_console_cell(c, col, row, &cp) == 0);
    return cp;
}
static void text(struct hpclang_console *c, const char *s)
{
    assert(hpclang_console_write(c, s, strlen(s)) == 0);
}
int main(void)
{
    assert(!hpclang_console_create(0, 1, &font) && errno == EINVAL);
    assert(!hpclang_console_create(1, 1, NULL) && errno == EINVAL);
    assert(!hpclang_console_create(UINT32_MAX, 1, &font) && errno == EINVAL);
    struct hpclang_console *c = hpclang_console_create(4, 2, &font);
    assert(c);
    text(c, "ABCDEFGH");
    assert(cell(c, 0, 0) == 'A' && cell(c, 3, 1) == 'H');
    text(c, "I");
    assert(cell(c, 0, 0) == 'E' && cell(c, 0, 1) == 'I' && cell(c, 1, 1) == ' ');
    text(c, "\rJ\bK");
    assert(cell(c, 0, 1) == 'K');
    text(c, "\fA\tB");
    assert(cell(c, 0, 0) == 'A' && cell(c, 1, 0) == ' ' && cell(c, 0, 1) == 'B');
    hpclang_console_clear(c);
    text(c, "ABCD\nE");
    assert(cell(c, 0, 0) == 'A' && cell(c, 0, 1) == 'E'); /* no double newline */
    hpclang_console_clear(c);
    text(c, "\xce");
    assert(cell(c, 0, 0) == ' ');
    text(c, "\xa9\xf0\x9f");
    text(c, "\x98\x80");
    assert(cell(c, 0, 0) == 0x3a9 && cell(c, 1, 0) == 0x1f600);
    text(c, "\xe2");
    assert(hpclang_console_flush(c) == 0 && cell(c, 2, 0) == 0xfffd);
    text(c, "\xe2Z");
    assert(cell(c, 3, 0) == 0xfffd && cell(c, 0, 1) == 'Z');
    hpclang_console_clear(c);
    text(c, "\xed\xa0\x80\xf4\x90\x80\x80\xc0\x80");
    for (unsigned i = 0; i < 4; ++i)
        assert(cell(c, i, 0) == 0xfffd);
    assert(hpclang_console_write(c, NULL, 0) == 0);
    assert(hpclang_console_write(c, NULL, 1) == -1 && errno == EINVAL);
    uint32_t untouched = 42;
    assert(hpclang_console_cell(c, 4, 0, &untouched) == -1 && untouched == 42);
    hpclang_console_clear(c);
    text(c, "A");
    struct hpclang_surface *s = hpclang_surface_create_mono(18, 12);
    assert(s);
    hpclang_surface_clear(s, 255);
    assert(hpclang_console_render(c, s, 2, 2) == 0);
    assert(hpclang_surface_get_pixel(s, 1, 2) == 255); /* outside unchanged */
    assert(hpclang_surface_get_pixel(s, 2, 2) == 255);
    assert(hpclang_surface_get_pixel(s, 4, 2) == 255);
    assert(hpclang_surface_get_pixel(s, 5, 2) == 0); /* overwide glyph clipped */
    assert(hpclang_surface_get_pixel(s, 2, 5) == 0);
    assert(hpclang_surface_get_pixel(s, 14, 2) == 255);
    assert(hpclang_console_render(c, s, INT32_MIN, INT32_MAX) == 0);
    assert(hpclang_console_render(c, NULL, 0, 0) == -1 && errno == EINVAL);
    hpclang_surface_destroy(s);
    hpclang_console_destroy(c);
    c = hpclang_console_create(1, 1, &font);
    assert(c);
    text(c, "A");
    assert(cell(c, 0, 0) == 'A');
    text(c, "B");
    assert(cell(c, 0, 0) == 'B');
    text(c, "\n");
    assert(cell(c, 0, 0) == ' ');
    hpclang_console_destroy(c);
    hpclang_console_destroy(NULL);
    return 0;
}
