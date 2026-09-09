#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <hpclang/display.h>

#include "display-surface.h"

enum clip_code {
    CLIP_LEFT = 1u << 0,
    CLIP_RIGHT = 1u << 1,
    CLIP_TOP = 1u << 2,
    CLIP_BOTTOM = 1u << 3
};

static int black(uint8_t intensity)
{
    return intensity >= 128u;
}

static uint64_t magnitude(int64_t value)
{
    return value < 0 ? (uint64_t)(-value) : (uint64_t)value;
}

/*
 * Return delta * numerator / denominator without signed multiplication.
 *
 * All three inputs originate from differences of int32_t drawing coordinates,
 * so their magnitudes fit uint32_t and the unsigned product fits uint64_t.
 */
static int64_t scaled_delta(
    int64_t delta,
    int64_t numerator,
    int64_t denominator)
{
    int negative = 0;

    if (delta < 0)
        negative ^= 1;
    if (numerator < 0)
        negative ^= 1;
    if (denominator < 0)
        negative ^= 1;

    uint64_t product = magnitude(delta) * magnitude(numerator);
    uint64_t quotient = product / magnitude(denominator);
    return negative ? -(int64_t)quotient : (int64_t)quotient;
}

static unsigned clip_code_for(
    const struct hpclang_surface *surface,
    int64_t x,
    int64_t y)
{
    unsigned code = 0u;

    if (x < 0)
        code |= CLIP_LEFT;
    else if ((uint64_t)x >= surface->width)
        code |= CLIP_RIGHT;

    if (y < 0)
        code |= CLIP_TOP;
    else if ((uint64_t)y >= surface->height)
        code |= CLIP_BOTTOM;

    return code;
}

static int clip_line(
    const struct hpclang_surface *surface,
    int64_t *x0,
    int64_t *y0,
    int64_t *x1,
    int64_t *y1)
{
    if (surface->width == 0u || surface->height == 0u)
        return 0;

    int64_t xmax = (int64_t)surface->width - 1;
    int64_t ymax = (int64_t)surface->height - 1;

    for (;;) {
        unsigned code0 = clip_code_for(surface, *x0, *y0);
        unsigned code1 = clip_code_for(surface, *x1, *y1);

        if ((code0 | code1) == 0u)
            return 1;
        if ((code0 & code1) != 0u)
            return 0;

        unsigned code = code0 != 0u ? code0 : code1;
        int64_t x;
        int64_t y;

        if ((code & CLIP_TOP) != 0u) {
            y = 0;
            x = *x0 + scaled_delta(
                *x1 - *x0, -*y0, *y1 - *y0);
        } else if ((code & CLIP_BOTTOM) != 0u) {
            y = ymax;
            x = *x0 + scaled_delta(
                *x1 - *x0, ymax - *y0, *y1 - *y0);
        } else if ((code & CLIP_RIGHT) != 0u) {
            x = xmax;
            y = *y0 + scaled_delta(
                *y1 - *y0, xmax - *x0, *x1 - *x0);
        } else {
            x = 0;
            y = *y0 + scaled_delta(
                *y1 - *y0, -*x0, *x1 - *x0);
        }

        if (code == code0) {
            *x0 = x;
            *y0 = y;
        } else {
            *x1 = x;
            *y1 = y;
        }
    }
}

struct hpclang_surface *hpclang_surface_create_mono(
    uint32_t width,
    uint32_t height)
{
    if (width == 0u || height == 0u)
        return NULL;

    uint32_t stride = (width >> 3) + ((width & 7u) != 0u);
    if (height > SIZE_MAX / stride)
        return NULL;

    size_t bytes = (size_t)stride * height;
    struct hpclang_surface *surface = malloc(sizeof(*surface));
    if (surface == NULL)
        return NULL;

    uint8_t *pixels = malloc(bytes);
    if (pixels == NULL) {
        free(surface);
        return NULL;
    }

    memset(pixels, 0, bytes);
    surface->pixels = pixels;
    surface->width = width;
    surface->height = height;
    surface->stride = stride;
    surface->levels = 2u;
    return surface;
}

void hpclang_surface_destroy(struct hpclang_surface *surface)
{
    if (surface == NULL)
        return;

    free((void *)surface->pixels);
    free(surface);
}

uint32_t hpclang_surface_width(const struct hpclang_surface *surface)
{
    return surface->width;
}

uint32_t hpclang_surface_height(const struct hpclang_surface *surface)
{
    return surface->height;
}

uint32_t hpclang_surface_levels(const struct hpclang_surface *surface)
{
    return surface->levels;
}

void hpclang_surface_clear(
    struct hpclang_surface *surface,
    uint8_t intensity)
{
    uint32_t full_bytes = surface->width >> 3;
    uint32_t tail_bits = surface->width & 7u;
    uint8_t value = black(intensity) ? UINT8_C(0xff) : 0u;

    for (uint32_t y = 0u; y < surface->height; ++y) {
        volatile uint8_t *row = surface->pixels + y * surface->stride;

        for (uint32_t x = 0u; x < full_bytes; ++x)
            row[x] = value;

        if (tail_bits != 0u) {
            uint8_t mask = (uint8_t)((1u << tail_bits) - 1u);
            row[full_bytes] =
                (uint8_t)((row[full_bytes] & (uint8_t)~mask) |
                          (value & mask));
        }
    }
}

uint8_t hpclang_surface_get_pixel(
    const struct hpclang_surface *surface,
    int32_t x,
    int32_t y)
{
    if (x < 0 || y < 0 ||
        (uint32_t)x >= surface->width ||
        (uint32_t)y >= surface->height)
        return 0u;

    const volatile uint8_t *pixel =
        surface->pixels + (uint32_t)y * surface->stride + ((uint32_t)x >> 3);
    uint8_t mask = (uint8_t)(1u << ((uint32_t)x & 7u));
    return (*pixel & mask) != 0u ? 255u : 0u;
}

void hpclang_surface_set_pixel(
    struct hpclang_surface *surface,
    int32_t x,
    int32_t y,
    uint8_t intensity)
{
    if (x < 0 || y < 0 ||
        (uint32_t)x >= surface->width ||
        (uint32_t)y >= surface->height)
        return;

    volatile uint8_t *pixel =
        surface->pixels + (uint32_t)y * surface->stride + ((uint32_t)x >> 3);
    uint8_t mask = (uint8_t)(1u << ((uint32_t)x & 7u));

    if (black(intensity))
        *pixel |= mask;
    else
        *pixel &= (uint8_t)~mask;
}

static void draw_line(
    struct hpclang_surface *surface,
    int64_t x0,
    int64_t y0,
    int64_t x1,
    int64_t y1,
    uint8_t intensity)
{
    if (!clip_line(surface, &x0, &y0, &x1, &y1))
        return;

    int64_t dx = x1 >= x0 ? x1 - x0 : x0 - x1;
    int64_t sx = x0 < x1 ? 1 : -1;
    int64_t dy_abs = y1 >= y0 ? y1 - y0 : y0 - y1;
    int64_t dy = -dy_abs;
    int64_t sy = y0 < y1 ? 1 : -1;
    int64_t error = dx + dy;

    for (;;) {
        hpclang_surface_set_pixel(
            surface, (int32_t)x0, (int32_t)y0, intensity);
        if (x0 == x1 && y0 == y1)
            return;

        int64_t twice = error * 2;
        if (twice >= dy) {
            error += dy;
            x0 += sx;
        }
        if (twice <= dx) {
            error += dx;
            y0 += sy;
        }
    }
}

void hpclang_surface_draw_line(
    struct hpclang_surface *surface,
    int32_t x0,
    int32_t y0,
    int32_t x1,
    int32_t y1,
    uint8_t intensity)
{
    draw_line(surface, x0, y0, x1, y1, intensity);
}

void hpclang_surface_draw_rect(
    struct hpclang_surface *surface,
    int32_t x,
    int32_t y,
    int32_t width,
    int32_t height,
    uint8_t intensity)
{
    if (width <= 0 || height <= 0)
        return;

    int64_t left = x;
    int64_t top = y;
    int64_t right = left + (int64_t)width - 1;
    int64_t bottom = top + (int64_t)height - 1;

    draw_line(surface, left, top, right, top, intensity);
    if (bottom != top)
        draw_line(surface, left, bottom, right, bottom, intensity);

    draw_line(surface, left, top, left, bottom, intensity);
    if (right != left)
        draw_line(surface, right, top, right, bottom, intensity);
}

void hpclang_surface_blit(
    struct hpclang_surface *destination,
    int32_t destination_x,
    int32_t destination_y,
    const struct hpclang_surface *source,
    int32_t source_x,
    int32_t source_y,
    int32_t width,
    int32_t height)
{
    if (width <= 0 || height <= 0)
        return;

    int64_t x0 = 0;
    int64_t y0 = 0;
    int64_t x1 = width;
    int64_t y1 = height;

    if (source_x < 0 && -(int64_t)source_x > x0)
        x0 = -(int64_t)source_x;
    if (destination_x < 0 && -(int64_t)destination_x > x0)
        x0 = -(int64_t)destination_x;
    if (source_y < 0 && -(int64_t)source_y > y0)
        y0 = -(int64_t)source_y;
    if (destination_y < 0 && -(int64_t)destination_y > y0)
        y0 = -(int64_t)destination_y;

    int64_t source_right = (int64_t)source->width - source_x;
    int64_t destination_right =
        (int64_t)destination->width - destination_x;
    int64_t source_bottom = (int64_t)source->height - source_y;
    int64_t destination_bottom =
        (int64_t)destination->height - destination_y;

    if (source_right < x1)
        x1 = source_right;
    if (destination_right < x1)
        x1 = destination_right;
    if (source_bottom < y1)
        y1 = source_bottom;
    if (destination_bottom < y1)
        y1 = destination_bottom;

    if (x0 >= x1 || y0 >= y1)
        return;

    int reverse_y =
        source == destination && destination_y > source_y;
    int reverse_x =
        source == destination &&
        destination_y == source_y &&
        destination_x > source_x;

    int64_t y = reverse_y ? y1 - 1 : y0;
    int64_t y_end = reverse_y ? y0 - 1 : y1;
    int64_t y_step = reverse_y ? -1 : 1;

    for (; y != y_end; y += y_step) {
        int64_t x = reverse_x ? x1 - 1 : x0;
        int64_t x_end = reverse_x ? x0 - 1 : x1;
        int64_t x_step = reverse_x ? -1 : 1;

        for (; x != x_end; x += x_step) {
            uint8_t intensity = hpclang_surface_get_pixel(
                source,
                (int32_t)((int64_t)source_x + x),
                (int32_t)((int64_t)source_y + y));
            hpclang_surface_set_pixel(
                destination,
                (int32_t)((int64_t)destination_x + x),
                (int32_t)((int64_t)destination_y + y),
                intensity);
        }
    }
}

void hpclang_surface_fill_rect(
    struct hpclang_surface *surface,
    int32_t x,
    int32_t y,
    int32_t width,
    int32_t height,
    uint8_t intensity)
{
    if (width <= 0 || height <= 0)
        return;

    if (x < 0) {
        width += x;
        x = 0;
    }
    if (y < 0) {
        height += y;
        y = 0;
    }
    if (width <= 0 || height <= 0 ||
        (uint32_t)x >= surface->width ||
        (uint32_t)y >= surface->height)
        return;

    if ((uint32_t)width > surface->width - (uint32_t)x)
        width = (int32_t)(surface->width - (uint32_t)x);
    if ((uint32_t)height > surface->height - (uint32_t)y)
        height = (int32_t)(surface->height - (uint32_t)y);

    for (int32_t yy = 0; yy < height; ++yy)
        for (int32_t xx = 0; xx < width; ++xx)
            hpclang_surface_set_pixel(
                surface, x + xx, y + yy, intensity);
}
