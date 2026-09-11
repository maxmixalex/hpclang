#ifndef HPCLANG_FONT_PACKAGE_H
#define HPCLANG_FONT_PACKAGE_H

#include <stdint.h>

#include <hpclang/font.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Generated bitmap-font ABI.
 *
 * The canonical custom-font source is the exact sdk/fonts/fonts.zip archive.
 * tools/hpclang-font.py reads that archive directly and emits only the selected
 * font/face and glyph set for an application build. Generated C descriptors are
 * build artifacts; they do not replace or normalize away the original package,
 * its licenses, provenance, mappings, or source font files.
 */
#define HPCLANG_FONT_PACKAGE_ABI 1u
#define HPCLANG_FONT_PACKAGE_KIND_BITMAP 1u

struct hpclang_bitmap_glyph {
    uint32_t codepoint;
    const uint8_t *bitmap;
    int16_t x_offset;
    int16_t y_offset;
    uint16_t width;
    uint16_t height;
    uint16_t advance;
    uint16_t stride;
};

struct hpclang_bitmap_font_data {
    const struct hpclang_bitmap_glyph *const *glyphs;
    uint32_t glyph_count;
    uint32_t fallback_codepoint;
    int16_t ascent;
    int16_t descent;
    uint16_t line_height;
    uint16_t nominal_advance;
};

/* The wrapper is deliberately tiny. STATIC generation emits only requested
 * glyph records; FULL generation emits the selected face's complete mapped
 * repertoire. Normal section GC can still discard an entirely unused font. */
struct hpclang_font {
    uint32_t abi;
    uint32_t kind;
    const void *data;
};

#define HPCLANG_BITMAP_FONT(font_data) \
    { HPCLANG_FONT_PACKAGE_ABI, HPCLANG_FONT_PACKAGE_KIND_BITMAP, &(font_data) }

#ifdef __cplusplus
}
#endif

#endif
