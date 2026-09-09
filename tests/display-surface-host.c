#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <hpclang/display.h>

#include "display-surface.h"

#define WIDTH 10u
#define HEIGHT 4u
#define STRIDE 2u

static uint8_t pixels[HEIGHT * STRIDE];

static struct hpclang_surface surface = {
    .pixels = pixels,
    .width = WIDTH,
    .height = HEIGHT,
    .stride = STRIDE,
    .levels = 2u
};

static void fail(const char *message)
{
    fprintf(stderr, "display surface test failed: %s\n", message);
    exit(EXIT_FAILURE);
}

static int visible_pixel(unsigned x, unsigned y)
{
    return (pixels[y * STRIDE + (x >> 3)] >> (x & 7u)) & 1u;
}

static void expect_row(unsigned y, uint16_t bits)
{
    for (unsigned x = 0u; x < WIDTH; ++x) {
        int expected = (bits >> x) & 1u;
        if (visible_pixel(x, y) != expected)
            fail("visible row mismatch");
    }
}

int main(void)
{
    if (hpclang_surface_width(&surface) != WIDTH ||
        hpclang_surface_height(&surface) != HEIGHT ||
        hpclang_surface_levels(&surface) != 2u)
        fail("geometry");

    memset(pixels, 0xa5, sizeof(pixels));
    hpclang_surface_clear(&surface, 0u);
    for (unsigned y = 0u; y < HEIGHT; ++y) {
        if (pixels[y * STRIDE] != 0u ||
            pixels[y * STRIDE + 1u] != 0xa4u)
            fail("white clear or padding preservation");
    }

    hpclang_surface_clear(&surface, 255u);
    for (unsigned y = 0u; y < HEIGHT; ++y) {
        if (pixels[y * STRIDE] != 0xffu ||
            pixels[y * STRIDE + 1u] != 0xa7u)
            fail("black clear or padding preservation");
    }

    hpclang_surface_clear(&surface, 0u);
    hpclang_surface_set_pixel(&surface, 0, 0, 255u);
    hpclang_surface_set_pixel(&surface, 9, 0, 255u);
    hpclang_surface_set_pixel(&surface, -1, 0, 255u);
    hpclang_surface_set_pixel(&surface, 10, 0, 255u);
    expect_row(0u, (1u << 0) | (1u << 9));
    if (hpclang_surface_get_pixel(&surface, 0, 0) != 255u ||
        hpclang_surface_get_pixel(&surface, 9, 0) != 255u ||
        hpclang_surface_get_pixel(&surface, -1, 0) != 0u ||
        hpclang_surface_get_pixel(&surface, 10, 0) != 0u)
        fail("pixel readback");

    hpclang_surface_set_pixel(&surface, 9, 0, 0u);
    expect_row(0u, 1u << 0);
    if (hpclang_surface_get_pixel(&surface, 9, 0) != 0u)
        fail("cleared pixel readback");

    hpclang_surface_clear(&surface, 0u);
    hpclang_surface_fill_rect(&surface, -2, -1, 5, 3, 255u);
    expect_row(0u, 0x007u);
    expect_row(1u, 0x007u);
    expect_row(2u, 0u);
    expect_row(3u, 0u);

    /*
     * A billion-pixel off-screen span must clip before rasterization. The
     * pre-refactor implementation would iterate the whole span.
     */
    hpclang_surface_clear(&surface, 0u);
    hpclang_surface_draw_line(
        &surface, -1000000000, 1, 1000000000, 1, 255u);
    expect_row(0u, 0u);
    expect_row(1u, 0x3ffu);
    expect_row(2u, 0u);
    expect_row(3u, 0u);

    hpclang_surface_clear(&surface, 0u);
    hpclang_surface_draw_line(
        &surface, -1000000000, -1000000000,
        1000000000, 1000000000, 255u);
    expect_row(0u, 1u << 0);
    expect_row(1u, 1u << 1);
    expect_row(2u, 1u << 2);
    expect_row(3u, 1u << 3);

    /* Clipping an outline must not invent borders at clipped edges. */
    hpclang_surface_clear(&surface, 0u);
    hpclang_surface_draw_rect(&surface, -2, -1, 5, 4, 255u);
    expect_row(0u, 1u << 2);
    expect_row(1u, 1u << 2);
    expect_row(2u, 0x007u);
    expect_row(3u, 0u);

    hpclang_surface_draw_rect(
        &surface, INT32_MAX, INT32_MAX, INT32_MAX, INT32_MAX, 255u);
    expect_row(0u, 1u << 2);
    expect_row(1u, 1u << 2);
    expect_row(2u, 0x007u);
    expect_row(3u, 0u);

    puts("PASS: opaque surface clipping, readback and packed-pixel primitives.");
    return EXIT_SUCCESS;
}
