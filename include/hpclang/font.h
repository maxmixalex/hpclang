#ifndef HPCLANG_FONT_H
#define HPCLANG_FONT_H

#include <stdint.h>
#include <stddef.h>

/*
 * Mark a compile-time UI string for STATIC_AUTO font repertoire collection.
 *
 * This is deliberately an identity macro at runtime: marked text remains an
 * ordinary narrow UTF-8 C/C++ string. The hpclang CMake extractor recognizes
 * only explicit HPCLANG_UI_TEXT("...") calls; unrelated literals, diagnostics,
 * paths, format strings and metadata do not silently grow a static font.
 *
 * The argument must consist of one or more adjacent narrow string literals.
 * Dynamic/generated text belongs with STATIC_EXPLICIT or FULL selection.
 */
#define HPCLANG_UI_TEXT(text) text

#ifdef __cplusplus
extern "C"
{
#endif

    struct hpclang_surface;
    struct hpclang_font;

    /*
     * Calculator-native ROM fonts. No calculator glyph bitmap is embedded in the
     * application and hpclang does not redistribute stock-ROM font data.
     *
     * These accessors name concrete historical calculator fonts; they are not a
     * semantic UI-role layer. Applications remain free to use any native or
     * generated font for any purpose.
     *
     * default is a convenience selection for the calculator's normal text:
     * HP39-family main 5x8, HP48gII/49g+/50g bigfont.
     *
     * mini exposes the stock minifont on every currently supported family.
     *
     * big exposes only the historical HP48gII/49g+/50g bigfont (8 raster rows,
     * nominal advance 6). It returns NULL on the HP39 family.
     *
     * editline exposes the dedicated HP39g+/39gs/40gs fixed 6x10 EditLine ROM
     * font, using each model's own ROM. It returns NULL on HP48gII/49g+/50g.
     */
    const struct hpclang_font *hpclang_font_default(void);
    const struct hpclang_font *hpclang_font_mini(void);
    const struct hpclang_font *hpclang_font_big(void);
    const struct hpclang_font *hpclang_font_editline(void);

    /* Return 1 when this font's source is accessible in the current launch
     * context, otherwise 0 (also for NULL/unknown descriptors). Native accessors
     * describe a model's font even when Direct execution has no Stock mapping;
     * this query lets applications select an explicit packaged fallback.
     * It reads mapping/source metadata, does not acquire display ownership,
     * and preserves errno. Availability does not imply coverage of every glyph. */
    int hpclang_font_available(const struct hpclang_font *font);

    /* Line metrics. advance is the nominal/fixed advance; proportional bitmap
     * fonts should use hpclang_font_codepoint_advance() when laying out text. */
    uint32_t hpclang_font_height(const struct hpclang_font *font);
    uint32_t hpclang_font_advance(const struct hpclang_font *font);
    uint32_t hpclang_font_codepoint_advance(const struct hpclang_font *font, uint32_t codepoint);

    /* Measure bounded UTF-8 with the same fallback/newline policy as drawing.
     * Width is the largest line's advance, not its ink bounds; height includes
     * one line for empty text. Returns 0, or -1/EINVAL or EOVERFLOW, preserving
     * outputs on failure. A NUL ends text before byte_count. */
    int hpclang_font_measure_text_n(const struct hpclang_font *font, const char *text,
                                    size_t byte_count, uint32_t *width, uint32_t *height);

    /* Draw one Unicode code point. Native calculator fonts fall back to '?' for
     * code points outside their byte-oriented repertoire. */
    void hpclang_surface_draw_codepoint(struct hpclang_surface *surface,
                                        const struct hpclang_font *font, int32_t x, int32_t y,
                                        uint32_t codepoint, uint8_t intensity);

    /* Compatibility byte entry point. The byte value is interpreted directly as
     * U+0000..U+00FF; it is not decoded as UTF-8. */
    void hpclang_surface_draw_char(struct hpclang_surface *surface, const struct hpclang_font *font,
                                   int32_t x, int32_t y, uint8_t character, uint8_t intensity);

    /* UTF-8 text. x/y are the top-left of the first line. Invalid UTF-8 is
     * replaced by U+FFFD and then by the selected font's fallback if necessary. */
    void hpclang_surface_draw_utf8(struct hpclang_surface *surface, const struct hpclang_font *font,
                                   int32_t x, int32_t y, const char *text, uint8_t intensity);

    /* draw_text is the canonical text API and has the same UTF-8 semantics as
     * hpclang_surface_draw_utf8(). */
    void hpclang_surface_draw_text(struct hpclang_surface *surface, const struct hpclang_font *font,
                                   int32_t x, int32_t y, const char *text, uint8_t intensity);

    /* Bounded message/error text: at most byte_count bytes are read. No NUL
     * terminator is required; an embedded NUL ends the message. Truncated or
     * invalid UTF-8 follows draw_text's replacement policy. Does not exit,
     * wait for keys, change errno, or clear the application's surface. */
    void hpclang_surface_draw_text_n(struct hpclang_surface *surface,
                                     const struct hpclang_font *font, int32_t x, int32_t y,
                                     const char *text, size_t byte_count, uint8_t intensity);

#ifdef __cplusplus
}
#endif

#endif
