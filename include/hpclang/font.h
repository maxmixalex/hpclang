#ifndef HPCLANG_FONT_H
#define HPCLANG_FONT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct hpclang_surface;
struct hpclang_font;

/*
 * Immutable built-in bitmap fonts.
 *
 * The canonical v1 repertoire is printable Basic Latin (0x20-0x7e). Bytes
 * outside that range render as '?'. This is an hpclang text contract, not a
 * claim to reproduce HPG's historical 256-entry calculator font namespace.
 */
const struct hpclang_font *hpclang_font_mini(void);
const struct hpclang_font *hpclang_font_big(void);

uint32_t hpclang_font_height(const struct hpclang_font *font);
uint32_t hpclang_font_advance(const struct hpclang_font *font);

void hpclang_surface_draw_char(
    struct hpclang_surface *surface,
    const struct hpclang_font *font,
    int32_t x,
    int32_t y,
    uint8_t character,
    uint8_t intensity);

void hpclang_surface_draw_text(
    struct hpclang_surface *surface,
    const struct hpclang_font *font,
    int32_t x,
    int32_t y,
    const char *text,
    uint8_t intensity);

#ifdef __cplusplus
}
#endif

#endif
