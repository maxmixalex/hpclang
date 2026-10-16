#include "hpg-internal.h"
#include "hp-character.h"
#include <hpclang/font-package.h>
#include <limits.h>

extern const struct hpclang_font hpclang_font_newrpl_font6_menus_full;
extern const struct hpclang_font hpclang_font_newrpl_font8_styleb_full;

static hpg_font_t mini = {NULL, 256, 6, 4, 2};
static hpg_font_t big = {NULL, 256, 8, 6, 3};

hpg_font_t *hpg_get_minifont(void)
{
    return &mini;
}
hpg_font_t *hpg_get_bigfont(void)
{
    return &big;
}

/* HP's byte namespace is Latin-1 with calculator symbols in 0x80..0xa0
 * and beta in place of sharp s. Native fonts already understand these bytes. */
static uint32_t unicode(unsigned char code)
{
    if (code < 32)
        return 0x25a0;
    if (code == 127)
        return 0x2592;
    return hpclang_hp_unicode_from_byte(code);
}

/* Map every source ink pixel into the fixed legacy cell. Only oversized faces
 * are compressed; ordinary glyphs retain their exact packaged raster. OR-ing
 * the destination pixels keeps thin strokes from disappearing when compressed. */
static int fit_glyph(struct hpclang_surface *surface, const struct hpclang_font *face,
                     hpg_font_t *font, uint32_t code, int x, int y)
{
    const struct hpclang_bitmap_font_data *data = face->data;
    for (uint32_t i = 0; i < data->glyph_count; ++i) {
        const struct hpclang_bitmap_glyph *g = data->glyphs[i];
        if (g->codepoint != code)
            continue;
        unsigned width = g->width, height = g->height;
        int top = data->ascent - g->y_offset - (int)height;
        if (g->x_offset >= 0 && top >= 0 && (unsigned)g->x_offset + width <= font->advance &&
            (unsigned)top + height <= font->height)
            return 0;
        for (unsigned yy = 0; yy < height; ++yy)
            for (unsigned xx = 0; xx < width; ++xx) {
                if (!(g->bitmap[(size_t)yy * g->stride + xx / 8] & (0x80u >> (xx % 8))))
                    continue;
                int64_t dx = (int64_t)x + (width > font->advance ? xx * font->advance / width : xx);
                int64_t dy = (int64_t)y + (height > font->height ? yy * font->height / height : yy);
                if (dx <= INT_MAX && dy <= INT_MAX)
                    hpclang_surface_set_pixel(surface, (int)dx, (int)dy, 255);
            }
        return 1;
    }
    return 0;
}

void hpclang_hpg_builtin_glyph(struct hpclang_surface *surface, hpg_font_t *font,
                               unsigned char code, int x, int y)
{
    const struct hpclang_font *native;
    if (font == &mini)
        native = hpclang_font_big() ? hpclang_font_mini() : NULL;
    else
        native = hpclang_font_default();
    if (hpclang_font_available(native)) {
        hpclang_surface_draw_char(surface, native, x, y, code, 255);
        return;
    }
    const struct hpclang_font *fallback = font == &mini ? &hpclang_font_newrpl_font6_menus_full
                                                        : &hpclang_font_newrpl_font8_styleb_full;
    if (code == 0x80) {
        /* A mathematical angle constructed from two rays; no ROM bitmap. */
        for (unsigned col = 0; col + 1 < font->advance; ++col) {
            int64_t dx = (int64_t)x + col;
            int64_t bottom = (int64_t)y + font->height - 2;
            if (dx <= INT_MAX && bottom <= INT_MAX) {
                hpclang_surface_set_pixel(surface, (int)dx, (int)bottom, 255);
                hpclang_surface_set_pixel(surface, (int)dx, (int)(bottom - col), 255);
            }
        }
    } else if (!fit_glyph(surface, fallback, font, unicode(code), x, y))
        hpclang_surface_draw_codepoint(surface, fallback, x, y, unicode(code), 255);
    if (code == 0x81)
        for (unsigned col = 0; col + 1 < font->advance; ++col)
            if ((int64_t)x + col <= INT_MAX)
                hpclang_surface_set_pixel(surface, (int)((int64_t)x + col), y, 255);
}
