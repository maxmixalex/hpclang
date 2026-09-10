#ifndef HPCLANG_FONT_PACKAGE_H
#define HPCLANG_FONT_PACKAGE_H

#include <stdint.h>

#include <hpclang/font.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Precompiled font-package ABI.
 *
 * Applications normally do not construct these records by hand. hpclang's
 * checked-in normalized font database and CMake font-selection helpers emit
 * them without running a source-font converter during an application build.
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

/* The wrapper is deliberately tiny so a selected STATIC descriptor can point
 * only at the glyph records it needs. With -fdata-sections/--gc-sections,
 * unreferenced glyph and bitmap sections from the normalized database vanish
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
