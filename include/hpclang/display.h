#ifndef HPCLANG_DISPLAY_H
#define HPCLANG_DISPLAY_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Pixel target with an implementation-private representation.
 *
 * Applications never receive framebuffer addresses, packing, or stride from
 * this type. The physical display surface is borrowed from the runtime.
 */
struct hpclang_surface;

/* Borrow the current physical display surface. */
struct hpclang_surface *hpclang_display_surface(void);

uint32_t hpclang_surface_width(const struct hpclang_surface *surface);
uint32_t hpclang_surface_height(const struct hpclang_surface *surface);
uint32_t hpclang_surface_levels(const struct hpclang_surface *surface);

/*
 * Drawing coordinates use a top-left origin. Operations clip to the surface.
 *
 * intensity uses 0 for white and 255 for black. The current physical display
 * has two levels, so intermediate values are thresholded internally.
 */
void hpclang_surface_clear(
    struct hpclang_surface *surface,
    uint8_t intensity);

void hpclang_surface_set_pixel(
    struct hpclang_surface *surface,
    int32_t x,
    int32_t y,
    uint8_t intensity);

void hpclang_surface_draw_line(
    struct hpclang_surface *surface,
    int32_t x0,
    int32_t y0,
    int32_t x1,
    int32_t y1,
    uint8_t intensity);

void hpclang_surface_fill_rect(
    struct hpclang_surface *surface,
    int32_t x,
    int32_t y,
    int32_t width,
    int32_t height,
    uint8_t intensity);

#ifdef __cplusplus
}
#endif

#endif
