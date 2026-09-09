#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <hpclang/display.h>
#include <hpclang/font.h>

static void fail(const char *message)
{
    fprintf(stderr, "font test failed: %s\n", message);
    exit(EXIT_FAILURE);
}

static void expect_glyph(
    const struct hpclang_surface *surface,
    int32_t x,
    int32_t y,
    uint32_t width,
    uint32_t height,
    const uint8_t *rows)
{
    for (uint32_t yy = 0u; yy < height; ++yy) {
        for (uint32_t xx = 0u; xx < width; ++xx) {
            uint32_t shift = width - 1u - xx;
            uint8_t expected =
                (rows[yy] & (uint8_t)(1u << shift)) != 0u ? 255u : 0u;
            if (hpclang_surface_get_pixel(
                    surface,
                    x + (int32_t)xx,
                    y + (int32_t)yy) != expected)
                fail("glyph bitmap");
        }
    }
}

int main(void)
{
    const struct hpclang_font *mini = hpclang_font_mini();
    const struct hpclang_font *big = hpclang_font_big();

    if (hpclang_font_height(mini) != 6u ||
        hpclang_font_advance(mini) != 4u ||
        hpclang_font_height(big) != 8u ||
        hpclang_font_advance(big) != 6u)
        fail("built-in metrics");

    struct hpclang_surface *surface = hpclang_surface_create_mono(24u, 24u);
    if (surface == NULL)
        fail("surface allocation");

    static const uint8_t mini_a[6] = {
        0x04u, 0x0au, 0x0eu, 0x0au, 0x0au, 0x00u,
    };
    static const uint8_t big_a[7] = {
        0x0cu, 0x12u, 0x12u, 0x1eu, 0x12u, 0x12u, 0x00u,
    };

    hpclang_surface_draw_char(surface, mini, 1, 1, (uint8_t)'A', 255u);
    expect_glyph(surface, 1, 1, 4u, 6u, mini_a);

    hpclang_surface_clear(surface, 0u);
    hpclang_surface_draw_char(surface, big, 1, 1, (uint8_t)'A', 255u);
    expect_glyph(surface, 1, 1, 5u, 7u, big_a);
    for (uint32_t x = 1u; x < 6u; ++x) {
        if (hpclang_surface_get_pixel(surface, (int32_t)x, 8) != 0u)
            fail("big-font eighth row");
    }

    hpclang_surface_clear(surface, 0u);
    hpclang_surface_draw_text(surface, mini, 1, 1, "AA\nA", 255u);
    if (hpclang_surface_get_pixel(surface, 2, 1) != 255u ||
        hpclang_surface_get_pixel(surface, 6, 1) != 255u ||
        hpclang_surface_get_pixel(surface, 2, 7) != 255u)
        fail("advance or newline");

    hpclang_surface_clear(surface, 0u);
    hpclang_surface_draw_char(surface, mini, -2, -2, (uint8_t)'A', 255u);

    /* Bytes outside canonical Basic Latin use the documented '?' fallback. */
    hpclang_surface_clear(surface, 0u);
    hpclang_surface_draw_char(surface, mini, 1, 1, 0xa0u, 255u);
    static const uint8_t mini_question[6] = {
        0x0cu, 0x02u, 0x04u, 0x00u, 0x04u, 0x00u,
    };
    expect_glyph(surface, 1, 1, 4u, 6u, mini_question);

    hpclang_surface_destroy(surface);
    puts("PASS: built-in fonts, metrics, clipping and text layout.");
    return EXIT_SUCCESS;
}
