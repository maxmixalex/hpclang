#include <stdint.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <hpclang/abi.h>
#include <hpclang/bootstrap.h>
#include <hpclang/display.h>
#include <hpclang/font.h>
#include <hpclang/font-package.h>
#include <hpclang/target.h>

static struct hpclang_boot_info fake_boot = {
    .model = HPCLANG_MODEL_HP50G,
};

const struct hpclang_boot_info *hpclang_boot_info(void)
{
    return &fake_boot;
}

static void fail(const char *message)
{
    fprintf(stderr, "font test failed: %s\n", message);
    exit(EXIT_FAILURE);
}

static const uint8_t glyph_question_bitmap[] = {
    0xe0u, 0x20u, 0x40u, 0x00u, 0x40u,
};
static const uint8_t glyph_a_bitmap[] = {
    0x40u, 0xa0u, 0xe0u, 0xa0u, 0xa0u,
};
static const uint8_t glyph_omega_bitmap[] = {
    0x70u, 0x88u, 0x88u, 0x50u, 0xd8u,
};

static const struct hpclang_bitmap_glyph glyph_question = {
    .codepoint = '?',
    .bitmap = glyph_question_bitmap,
    .width = 3u,
    .height = 5u,
    .advance = 4u,
    .stride = 1u,
};
static const struct hpclang_bitmap_glyph glyph_a = {
    .codepoint = 'A',
    .bitmap = glyph_a_bitmap,
    .width = 3u,
    .height = 5u,
    .advance = 5u,
    .stride = 1u,
};
static const struct hpclang_bitmap_glyph glyph_omega = {
    .codepoint = 0x03a9u,
    .bitmap = glyph_omega_bitmap,
    .width = 5u,
    .height = 5u,
    .advance = 6u,
    .stride = 1u,
};

static const struct hpclang_bitmap_glyph *const test_glyphs[] = {
    &glyph_question,
    &glyph_a,
    &glyph_omega,
};
static const struct hpclang_bitmap_font_data test_data = {
    .glyphs = test_glyphs,
    .glyph_count = 3u,
    .fallback_codepoint = '?',
    .ascent = 5,
    .descent = 1,
    .line_height = 6u,
    .nominal_advance = 4u,
};
static const struct hpclang_font test_font = HPCLANG_BITMAP_FONT(test_data);

static void expect_pixel(const struct hpclang_surface *surface, int32_t x, int32_t y,
                         uint8_t expected)
{
    if (hpclang_surface_get_pixel(surface, x, y) != expected)
        fail("pixel mismatch");
}

int main(void)
{
    const struct hpclang_font *mini = hpclang_font_mini();
    const struct hpclang_font *big = hpclang_font_big();
    const struct hpclang_font *editline = hpclang_font_editline();
    errno = EDOM;
    if (hpclang_font_available(mini) || hpclang_font_available(big) ||
        hpclang_font_available(NULL) || !hpclang_font_available(&test_font) || errno != EDOM)
        fail("font source availability without a live mapping");
    if (hpclang_font_default() != big || editline != NULL || hpclang_font_height(mini) != 6u ||
        hpclang_font_advance(mini) != 4u || hpclang_font_height(big) != 8u ||
        hpclang_font_advance(big) != 6u)
        fail("HP50 native font metrics");

    fake_boot.model = HPCLANG_MODEL_HP39GS;
    mini = hpclang_font_mini();
    big = hpclang_font_big();
    editline = hpclang_font_editline();
    const struct hpclang_font *main_font = hpclang_font_default();
    if (big != NULL || editline == NULL || main_font == NULL || hpclang_font_height(mini) != 5u ||
        hpclang_font_advance(mini) != 4u || hpclang_font_height(main_font) != 8u ||
        hpclang_font_advance(main_font) != 6u || hpclang_font_height(editline) != 10u ||
        hpclang_font_advance(editline) != 6u)
        fail("HP39 native font metrics");

    fake_boot.model = HPCLANG_MODEL_HP39GPLUS;
    if (hpclang_font_big() != NULL || hpclang_font_editline() == NULL ||
        hpclang_font_default() == NULL || hpclang_font_height(hpclang_font_editline()) != 10u)
        fail("HP39g+ native font availability");
    /* A missing or stale private-window cookie must never be dereferenced.
     * The native mini-font reader fails closed to its nominal advance. The
     * exact valid VA is deliberately not dereferenced in this host unit test. */
    const uint32_t hp39_models[] = {HPCLANG_MODEL_HP39GPLUS, HPCLANG_MODEL_HP39GS,
                                    HPCLANG_MODEL_HP40GS};
    for (unsigned i = 0; i < sizeof(hp39_models) / sizeof(hp39_models[0]); ++i) {
        fake_boot.model = hp39_models[i];
        fake_boot.platform_cookie = 0u;
        if (hpclang_font_available(mini) || hpclang_font_codepoint_advance(mini, 'A') != 4u)
            fail("HP39 missing font-window cookie");
        fake_boot.platform_cookie = HPCLANG_CODE_WINDOW_BASE + HPCLANG_PAGE_SIZE;
        if (hpclang_font_codepoint_advance(mini, 'A') != 4u)
            fail("HP39 shifted font-window cookie");
        fake_boot.platform_cookie = 0x0a000000u;
        if (hpclang_font_codepoint_advance(mini, 'A') != 4u)
            fail("HP39 retired section-alias cookie");
        fake_boot.platform_cookie = HPCLANG_CODE_WINDOW_BASE;
        if (!hpclang_font_available(mini) || !hpclang_font_available(main_font) ||
            !hpclang_font_available(editline))
            fail("HP39 valid private font-window descriptor");
    }
    fake_boot.platform_cookie = 0u;

    if (hpclang_font_height(&test_font) != 6u || hpclang_font_advance(&test_font) != 4u ||
        hpclang_font_codepoint_advance(&test_font, 'A') != 5u ||
        hpclang_font_codepoint_advance(&test_font, 0x03a9u) != 6u ||
        hpclang_font_codepoint_advance(&test_font, 'Z') != 4u)
        fail("bitmap metrics or fallback");

    struct hpclang_surface *surface = hpclang_surface_create_mono(32u, 20u);
    if (surface == NULL)
        fail("surface allocation");

    hpclang_surface_clear(surface, 0u);
    hpclang_surface_draw_codepoint(surface, &test_font, 1, 1, 'A', 255u);
    expect_pixel(surface, 2, 1, 255u);
    expect_pixel(surface, 1, 2, 255u);
    expect_pixel(surface, 2, 3, 255u);

    hpclang_surface_clear(surface, 0u);
    hpclang_surface_draw_codepoint(surface, &test_font, 1, 1, 'Z', 255u);
    /* Missing Z falls back to '?': first row is 111. */
    expect_pixel(surface, 1, 1, 255u);
    expect_pixel(surface, 2, 1, 255u);
    expect_pixel(surface, 3, 1, 255u);

    hpclang_surface_clear(surface, 0u);
    hpclang_surface_draw_text(surface, &test_font, 1, 1, "A\xce\xa9\nA", 255u);
    /* A advances 5, then UTF-8 Omega starts at x=6. */
    expect_pixel(surface, 2, 1, 255u);
    expect_pixel(surface, 7, 1, 255u);
    expect_pixel(surface, 2, 7, 255u);

    hpclang_surface_clear(surface, 0u);
    hpclang_surface_draw_utf8(surface, &test_font, 1, 1, "\xff", 255u);
    /* Invalid UTF-8 -> U+FFFD -> configured '?' fallback. */
    expect_pixel(surface, 1, 1, 255u);

    hpclang_surface_clear(surface, 0u);
    const char bounded[] = {'A', (char)0xce, (char)0xa9};
    hpclang_surface_draw_text_n(surface, &test_font, 1, 1, bounded, 1, 255u);
    expect_pixel(surface, 2, 1, 255u);
    expect_pixel(surface, 7, 1, 0u);
    hpclang_surface_clear(surface, 0u);
    hpclang_surface_draw_text_n(surface, &test_font, 1, 1, bounded + 1, 1, 255u);
    expect_pixel(surface, 1, 1, 255u); /* Truncated Omega uses replacement. */
    hpclang_surface_clear(surface, 0u);
    hpclang_surface_draw_text_n(surface, &test_font, 1, 1, bounded, 0, 255u);
    expect_pixel(surface, 2, 1, 0u);

    hpclang_surface_destroy(surface);
    puts("PASS: native ROM fonts, packaged bitmap metrics, UTF-8 and fallback.");
    return EXIT_SUCCESS;
}
