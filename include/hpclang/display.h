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

/*
 * Allocate an owned two-level surface initialized to white. The returned
 * surface has the same opaque drawing contract as the physical display but is
 * backed by process heap memory. Return NULL if allocation fails.
 */
struct hpclang_surface *hpclang_surface_create_mono(
    uint32_t width,
    uint32_t height);

/* Release a surface returned by hpclang_surface_create_mono(). */
void hpclang_surface_destroy(struct hpclang_surface *surface);

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

/*
 * Read one pixel as an hpclang intensity. Out-of-bounds coordinates read as
 * white (0). The current two-level display returns either 0 or 255.
 */
uint8_t hpclang_surface_get_pixel(
    const struct hpclang_surface *surface,
    int32_t x,
    int32_t y);

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

/*
 * Draw an outline rectangle. width/height are extents, not inclusive end
 * coordinates. Like the other primitives, the outline is geometrically
 * clipped rather than replaced by an outline of the clipped bounding box.
 */
void hpclang_surface_draw_rect(
    struct hpclang_surface *surface,
    int32_t x,
    int32_t y,
    int32_t width,
    int32_t height,
    uint8_t intensity);

/*
 * Copy a rectangular source region to a destination surface. Source and
 * destination coordinates are clipped together so their pixel correspondence
 * is preserved. Overlap on the same surface is supported.
 */
void hpclang_surface_blit(
    struct hpclang_surface *destination,
    int32_t destination_x,
    int32_t destination_y,
    const struct hpclang_surface *source,
    int32_t source_x,
    int32_t source_y,
    int32_t width,
    int32_t height);

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
