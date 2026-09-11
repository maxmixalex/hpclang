#include <stdint.h>
#include <stdio.h>

#include <hpclang/device.h>
#include <hpclang/display.h>
#include <hpclang/font.h>

#define HPCLANG_E2E_PASS UINT32_C(0x50415353)
#define HPCLANG_E2E_FAIL UINT32_C(0x46410000)

extern const struct hpclang_font hpclang_gallery_mini;
extern const struct hpclang_font hpclang_gallery_normal;
extern const struct hpclang_font hpclang_gallery_large;
extern const struct hpclang_font hpclang_gallery_spleen;

volatile uint32_t hpclang_e2e_result;

/*
 * STATIC_AUTO is deliberately driven by one explicit repertoire declaration.
 * Dynamic metric labels below use only these characters.  Ordinary literals
 * in this file therefore remain irrelevant to generated-font retention.
 */
static const char gallery_ui_repertoire[] =
    HPCLANG_UI_TEXT("FONT GALLERY NGMTEDILARWihax 0123456789/@+-");

static void hold_result(void)
{
    for (volatile uint32_t n = 12000000u; n != 0u; --n)
        __asm__ volatile("nop");
}

static int font_metrics_valid(const struct hpclang_font *font)
{
    return font != NULL &&
        hpclang_font_height(font) != 0u &&
        hpclang_font_advance(font) != 0u &&
        hpclang_font_codepoint_advance(font, (uint32_t)'W') != 0u &&
        hpclang_font_codepoint_advance(font, (uint32_t)'i') != 0u;
}

static void draw_metric_row(
    struct hpclang_surface *surface,
    int32_t y,
    const char *tag,
    const struct hpclang_font *sample_font)
{
    char metrics[32];
    (void)snprintf(
        metrics,
        sizeof(metrics),
        "%s %lu/%lu %lu/%lu",
        tag,
        (unsigned long)hpclang_font_height(sample_font),
        (unsigned long)hpclang_font_advance(sample_font),
        (unsigned long)hpclang_font_codepoint_advance(
            sample_font, (uint32_t)'i'),
        (unsigned long)hpclang_font_codepoint_advance(
            sample_font, (uint32_t)'W'));

    hpclang_surface_draw_text(
        surface, &hpclang_gallery_mini, 1, y, metrics, 255u);
    hpclang_surface_draw_text(
        surface, sample_font, 72, y, "Wi09", 255u);
}

static int finish(int failure)
{
    if (failure != 0) {
        struct hpclang_surface *surface = hpclang_display_surface();
        hpclang_surface_clear(surface, 0u);
        hpclang_surface_draw_text(
            surface, hpclang_font_mini(), 20, 26,
            "FONT GALLERY FAIL", 255u);
        hpclang_e2e_result = HPCLANG_E2E_FAIL | (uint32_t)failure;
        hold_result();
        return failure;
    }

    hpclang_e2e_result = HPCLANG_E2E_PASS;
    hold_result();
    return 0;
}

int main(void)
{
    (void)gallery_ui_repertoire;

    const struct hpclang_device_info *device = hpclang_device_info();
    if (device == NULL ||
        device->version != HPCLANG_DEVICE_INFO_VERSION ||
        device->display_width != 131u ||
        (device->display_height != 64u && device->display_height != 80u))
        return finish(1);

    struct hpclang_surface *surface = hpclang_display_surface();
    if (surface == NULL ||
        hpclang_surface_width(surface) != device->display_width ||
        hpclang_surface_height(surface) != device->display_height ||
        hpclang_surface_levels(surface) != 2u)
        return finish(2);

    const struct hpclang_font *native_mini = hpclang_font_mini();
    const struct hpclang_font *native_text = hpclang_font_default();
    const struct hpclang_font *native_edit = hpclang_font_big();

    if (!font_metrics_valid(native_mini) ||
        !font_metrics_valid(native_text) ||
        !font_metrics_valid(native_edit) ||
        !font_metrics_valid(&hpclang_gallery_mini) ||
        !font_metrics_valid(&hpclang_gallery_normal) ||
        !font_metrics_valid(&hpclang_gallery_large) ||
        !font_metrics_valid(&hpclang_gallery_spleen))
        return finish(3);

    hpclang_surface_clear(surface, 0u);

    /*
     * The arbitrary FULL face is used for the title, including two Latin-1
     * glyphs.  This proves arbitrary face selection and non-ASCII repertoire
     * without making those code points part of the STATIC_AUTO aliases.
     */
    hpclang_surface_draw_utf8(
        surface,
        &hpclang_gallery_spleen,
        1,
        0,
        "FONT GALLERY N/G \xC2\xA3\xC3\xA9",
        255u);

    /*
     * Rows are intentionally semantic rather than storage-oriented:
     *   NM = native mini
     *   NT = native normal/text
     *   NE = native edit-large candidate (current native big)
     *   GM/GN/GL = generated @mini/@normal/@large aliases
     *
     * Each annotation is height/nominal-advance/i-advance/W-advance.  HP39's
     * proportional native mini therefore exposes its spacing behavior directly
     * while fixed faces visibly retain equal i/W advances.
     */
    draw_metric_row(surface, 9,  "NM", native_mini);
    draw_metric_row(surface, 16, "NT", native_text);
    draw_metric_row(surface, 25, "NE", native_edit);
    draw_metric_row(surface, 34, "GM", &hpclang_gallery_mini);
    draw_metric_row(surface, 41, "GN", &hpclang_gallery_normal);
    draw_metric_row(surface, 50, "GL", &hpclang_gallery_large);

    /* Make the target geometry visible and exercise the extra 80-line area. */
    hpclang_surface_draw_line(
        surface,
        0,
        (int32_t)device->display_height - 1,
        130,
        (int32_t)device->display_height - 1,
        255u);

    if (device->display_height == 80u) {
        hpclang_surface_draw_utf8(
            surface,
            &hpclang_gallery_spleen,
            1,
            63,
            "80 LINE \xC2\xA3 \xC3\xA9\xC3\xB1 \xC3\x97\xC3\xB7",
            255u);
        hpclang_surface_draw_text(
            surface,
            &hpclang_gallery_mini,
            1,
            72,
            "BOTTOM 79",
            255u);
    }

    return finish(0);
}
