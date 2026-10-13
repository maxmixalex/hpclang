#ifndef HPCLANG_DISPLAY_SURFACE_H
#define HPCLANG_DISPLAY_SURFACE_H

#include <stdint.h>

struct hpclang_surface
{
    volatile uint8_t *pixels;
    uint32_t width;
    uint32_t height;
    uint32_t stride;
    uint32_t levels;
    /* Internal borrowed drawing lease; coordinates remain surface-relative. */
    int clipped;
    int64_t clip_left, clip_top, clip_right, clip_bottom;
};

/* Copies the descriptor, never pixels. Intersects an existing lease; neither
 * changes its owner nor allocates. The result must never be destroyed. */
void hpclang_surface_clip(struct hpclang_surface *view, const struct hpclang_surface *source,
                          int32_t x, int32_t y, int32_t width, int32_t height);

#endif
