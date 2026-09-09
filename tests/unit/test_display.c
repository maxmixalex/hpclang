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

    struct hpclang_surface *shapes = hpclang_surface_create_mono(11u, 11u);
    if (shapes == NULL)
        fail("shape surface allocation");

    hpclang_surface_draw_circle(shapes, 5, 5, 3, 255u);
    if (hpclang_surface_get_pixel(shapes, 5, 2) != 255u ||
        hpclang_surface_get_pixel(shapes, 8, 5) != 255u ||
        hpclang_surface_get_pixel(shapes, 5, 8) != 255u ||
        hpclang_surface_get_pixel(shapes, 2, 5) != 255u ||
        hpclang_surface_get_pixel(shapes, 5, 5) != 0u)
        fail("circle outline");

    hpclang_surface_clear(shapes, 0u);
    hpclang_surface_fill_circle(shapes, 5, 5, 3, 255u);
    if (hpclang_surface_get_pixel(shapes, 5, 5) != 255u ||
        hpclang_surface_get_pixel(shapes, 2, 5) != 255u ||
        hpclang_surface_get_pixel(shapes, 2, 2) != 0u)
        fail("circle fill");

    hpclang_surface_clear(shapes, 0u);
    hpclang_surface_draw_circle(
        shapes, 0, 0, INT32_MAX, 255u);
    if (hpclang_surface_get_pixel(shapes, 0, 0) != 0u)
        fail("extreme circle clipping");

    static const struct hpclang_point triangle[] = {
        {1, 1}, {9, 1}, {5, 9},
    };
    hpclang_surface_draw_polygon(
        shapes, triangle, 3u, 255u);
    if (hpclang_surface_get_pixel(shapes, 1, 1) != 255u ||
        hpclang_surface_get_pixel(shapes, 9, 1) != 255u ||
        hpclang_surface_get_pixel(shapes, 5, 9) != 255u ||
        hpclang_surface_get_pixel(shapes, 5, 4) != 0u)
        fail("polygon outline");

    hpclang_surface_clear(shapes, 0u);
    hpclang_surface_fill_polygon(
        shapes, triangle, 3u, 255u);
    if (hpclang_surface_get_pixel(shapes, 5, 4) != 255u ||
        hpclang_surface_get_pixel(shapes, 0, 10) != 0u)
        fail("polygon fill");

    hpclang_surface_destroy(shapes);

    if (hpclang_surface_create(4u, 1u, 3u) != NULL)
        fail("unsupported owned surface levels");

    struct hpclang_surface *gray4 = hpclang_surface_create(5u, 2u, 4u);
    struct hpclang_surface *gray16 = hpclang_surface_create(5u, 2u, 16u);
    if (gray4 == NULL || gray16 == NULL ||
        hpclang_surface_levels(gray4) != 4u ||
        hpclang_surface_levels(gray16) != 16u)
        fail("software grayscale surface allocation");

    hpclang_surface_set_pixel(gray4, 0, 0, 0u);
    hpclang_surface_set_pixel(gray4, 1, 0, 64u);
    hpclang_surface_set_pixel(gray4, 2, 0, 128u);
    hpclang_surface_set_pixel(gray4, 3, 0, 255u);
    if (hpclang_surface_get_pixel(gray4, 0, 0) != 0u ||
        hpclang_surface_get_pixel(gray4, 1, 0) != 85u ||
        hpclang_surface_get_pixel(gray4, 2, 0) != 170u ||
        hpclang_surface_get_pixel(gray4, 3, 0) != 255u)
        fail("four-level quantization");

    hpclang_surface_blit(gray16, 0, 0, gray4, 0, 0, 4, 1);
    if (hpclang_surface_get_pixel(gray16, 0, 0) != 0u ||
        hpclang_surface_get_pixel(gray16, 1, 0) != 85u ||
        hpclang_surface_get_pixel(gray16, 2, 0) != 170u ||
        hpclang_surface_get_pixel(gray16, 3, 0) != 255u)
        fail("cross-depth grayscale blit");

    hpclang_surface_clear(gray16, 127u);
    if (hpclang_surface_get_pixel(gray16, 4, 1) != 119u)
        fail("sixteen-level clear quantization");

    hpclang_surface_destroy(gray4);
    hpclang_surface_destroy(gray16);

    struct hpclang_surface *owned = hpclang_surface_create_mono(10u, 4u);
    if (owned == NULL ||
        hpclang_surface_width(owned) != 10u ||
        hpclang_surface_height(owned) != 4u ||
        hpclang_surface_levels(owned) != 2u ||
        hpclang_surface_get_pixel(owned, 0, 0) != 0u)
        fail("owned mono surface");

    hpclang_surface_set_pixel(owned, 0, 0, 255u);
    hpclang_surface_set_pixel(owned, 2, 0, 255u);
    hpclang_surface_blit(owned, 1, 0, owned, 0, 0, 4, 1);
    if (hpclang_surface_get_pixel(owned, 0, 0) != 255u ||
        hpclang_surface_get_pixel(owned, 1, 0) != 255u ||
        hpclang_surface_get_pixel(owned, 2, 0) != 0u ||
        hpclang_surface_get_pixel(owned, 3, 0) != 255u ||
        hpclang_surface_get_pixel(owned, 4, 0) != 0u)
        fail("overlapping blit");

    hpclang_surface_clear(&surface, 0u);
    hpclang_surface_blit(&surface, 0, 3, owned, 0, 0, 5, 1);
    expect_row(3u, (1u << 0) | (1u << 1) | (1u << 3));

    hpclang_surface_clear(&surface, 0u);
    hpclang_surface_blit(&surface, 0, 0, owned, -1, 0, 2, 1);
    expect_row(0u, (1u << 1));

    hpclang_surface_destroy(owned);
    hpclang_surface_destroy(NULL);

    puts("PASS: opaque surfaces, clipping, shapes, software levels, readback, ownership and blit.");
    return EXIT_SUCCESS;
}
