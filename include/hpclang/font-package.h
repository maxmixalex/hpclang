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
 * The canonical custom-font source is sdk/fonts/fonts.zip. Repository
 * maintenance tooling validates that archive and regenerates checked-in
 * linkable font assets when the source package changes. Normal application
 * builds consume those assets directly; they do not run Python or source-font
 * converters and do not depend on anything under tools/.
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

/* The wrapper is deliberately tiny. STATIC descriptors reference only the
 * selected mappings; FULL descriptors reference a selected face's complete
 * mapped repertoire. Section GC removes unreferenced glyph/bitmap sections
 * from the final HPC1 image. */
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
