#include <assert.h>
#include <errno.h>
#include <string.h>
#include <hpclang/abi.h>
#include <hpclang/console.h>
#include <hpclang/display.h>
#include <hpclang/font-package.h>
#include "console-internal.h"

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
static void controls(void)
{
    struct hpclang_console *c = hpclang_console_create(4, 2, &font);
    struct hpclang_console_state state;
    assert(c && hpclang_console_get_state(c, &state) == 0);
    assert(state.columns == 4 && state.rows == 2 && state.tab_width == 4);
    text(c, "ABCDEFGH");
    assert(hpclang_console_get_state(c, &state) == 0 && state.column == 4 && state.row == 1);
    assert(hpclang_console_clear_eol(c) == 0 && cell(c, 3, 1) == 'H');
    assert(hpclang_console_move(c, 2, 0) == 0 && hpclang_console_clear_eol(c) == 0);
    assert(cell(c, 1, 0) == 'B' && cell(c, 2, 0) == ' ' && cell(c, 0, 1) == 'E');
    text(c, "\xce");
    assert(hpclang_console_move(c, 5, 0) == -1 && errno == EINVAL);
    assert(hpclang_console_move(c, 0, 2) == -1 && errno == EINVAL);
    text(c, "\xa9");
    assert(cell(c, 2, 0) == 0x3a9);
    text(c, "\xe2");
    assert(hpclang_console_move(c, 0, 1) == 0 && cell(c, 3, 0) == 0xfffd);
    assert(hpclang_console_set_tab_width(c, 0) == -1 && errno == EINVAL);
    assert(hpclang_console_set_tab_width(c, UINT32_MAX) == 0);
    hpclang_console_clear(c);
    text(c, "A\t");
    assert(hpclang_console_get_state(c, &state) == 0);
    assert(state.tab_width == UINT32_MAX && state.column == 3 && state.row == 1);
    for (unsigned y = 0; y < 2; ++y)
        for (unsigned x = 0; x < 4; ++x)
            assert(cell(c, x, y) == ' ');
    assert(hpclang_console_get_state(NULL, &state) == -1 && state.column == 3);
    assert(hpclang_console_get_state(c, NULL) == -1);
    assert(hpclang_console_move(NULL, 0, 0) == -1);
    assert(hpclang_console_clear_eol(NULL) == -1);
    assert(hpclang_console_set_tab_width(NULL, 4) == -1);
    hpclang_console_destroy(c);
}

static void tab_reference(void)
{
    for (unsigned columns = 1; columns <= 4; ++columns)
        for (unsigned rows = 1; rows <= 3; ++rows) {
            struct hpclang_console *actual = hpclang_console_create(columns, rows, &font);
            struct hpclang_console *reference = hpclang_console_create(columns, rows, &font);
            assert(actual && reference);
            for (unsigned width = 1; width <= 17; ++width)
                for (unsigned y = 0; y < rows; ++y)
                    for (unsigned x = 0; x <= columns; ++x) {
                        hpclang_console_clear(actual);
                        hpclang_console_clear(reference);
                        for (unsigned i = 0; i < columns * rows; ++i) {
                            text(actual, "X");
                            text(reference, "X");
                        }
                        hpclang_console_move(actual, x, y);
                        hpclang_console_move(reference, x, y);
                        hpclang_console_set_tab_width(actual, width);
                        text(actual, "\t");
                        unsigned start = x;
                        if (x == columns) {
                            text(reference, "\n");
                            start = 0;
                        }
                        for (unsigned n = 0; n < width - start % width; ++n)
                            text(reference, " ");
                        struct hpclang_console_state a, b;
                        hpclang_console_get_state(actual, &a);
                        hpclang_console_get_state(reference, &b);
                        assert(a.column == b.column && a.row == b.row);
                        for (unsigned row = 0; row < rows; ++row)
                            for (unsigned col = 0; col < columns; ++col)
                                assert(cell(actual, col, row) == cell(reference, col, row));
                    }
            hpclang_console_destroy(actual);
            hpclang_console_destroy(reference);
        }
}
static unsigned adapter_calls;
static void adapter_glyph(struct hpclang_surface *s, const void *context, uint32_t cp)
{
    assert(context == &font && (cp == 'A' || cp == 0x3a9));
    assert(hpclang_surface_width(s) == 4 && hpclang_surface_height(s) == 6);
    assert(hpclang_surface_get_pixel(s, 0, 0) == 0); /* cleared for every cell */
    ++adapter_calls;
    hpclang_surface_set_pixel(s, 0, 0, 255);
    hpclang_surface_set_pixel(s, cp == 'A' ? 1 : 2, 1, 255);
    hpclang_surface_set_pixel(s, 4, 0, 255); /* must not bleed into next cell */
}
static void adapter_cells(void)
{
    assert(!hpclang_console_create_cells(2, 1, 4, 6, NULL, &font) && errno == EINVAL);
    assert(!hpclang_console_create_cells(2, 1, 0, 6, adapter_glyph, &font) && errno == EINVAL);
    assert(!hpclang_console_create_cells(2, 1, UINT32_MAX, 6, adapter_glyph, &font) &&
           errno == EINVAL);
    struct hpclang_console *c = hpclang_console_create_cells(3, 1, 4, 6, adapter_glyph, &font);
    struct hpclang_surface *s = hpclang_surface_create_mono(12, 6);
    assert(c && s && adapter_calls == 0);
    text(c, "A \xce\xa9");
    assert(hpclang_console_render(c, s, 0, 0) == 0 && adapter_calls == 2);
    for (int y = 0; y < 6; ++y)
        for (int x = 0; x < 12; ++x) {
            int ink = (y == 0 && (x == 0 || x == 8)) || (y == 1 && (x == 1 || x == 10));
            assert(hpclang_surface_get_pixel(s, x, y) == (ink ? 255 : 0));
        }
    hpclang_console_move(c, 2, 0);
    hpclang_console_clear_eol(c);
    assert(hpclang_console_render(c, s, 0, 0) == 0 && adapter_calls == 3);
    assert(hpclang_surface_get_pixel(s, 8, 0) == 0);
    assert(hpclang_console_write_byte(NULL, 0x87) == -1 && errno == EINVAL);
    for (unsigned code = 0x20; code <= 0xff; ++code) {
        hpclang_console_clear(c);
        assert(hpclang_console_write_byte(c, (unsigned char)code) == 0);
        assert(cell(c, 0, 0) == code);
    }
    hpclang_console_clear(c);
    text(c, "\xe2");
    assert(hpclang_console_write_byte(c, 0x87) == 0);
    assert(cell(c, 0, 0) == 0xfffd && cell(c, 1, 0) == 0x87);
    assert(hpclang_console_write_byte(c, '\r') == 0);
    assert(hpclang_console_write_byte(c, 'B') == 0 && cell(c, 0, 0) == 'B');
    hpclang_surface_destroy(s);
    hpclang_console_destroy(c);
}

int main(void)
{
    adapter_cells();
    controls();
    tab_reference();
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
