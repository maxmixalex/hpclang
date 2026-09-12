#include <stdint.h>
#include <string.h>

#include <hpclang/device.h>
#include <hpclang/display.h>
#include <hpclang/font.h>
#include <hpclang/keyboard.h>

#include "font-gallery-catalog.h"

#define HPCLANG_E2E_PASS UINT32_C(0x50415353)
#define HPCLANG_E2E_FAIL UINT32_C(0x46410000)
#define HPCLANG_GALLERY_STATE UINT32_C(0x47000000)
#define HPCLANG_GALLERY_COUNT_MAX UINT32_C(0x3ff)
#define HPCLANG_GALLERY_INDEX_MAX UINT32_C(0xfff)
#define HPCLANG_GALLERY_NEXT_FACE UINT32_C(0x00400000)

#define LCD_WIDTH 131
#define CHROME_LEFT 12
#define CHROME_RIGHT 119
#define CONTENT_LEFT 13
#define CONTENT_RIGHT 118
#define CONTENT_TOP 17
#define CONTENT_BOTTOM_MARGIN 3

volatile uint32_t hpclang_e2e_result;

static const uint32_t native_unicode[] = {
    163u, 165u, 169u, 176u, 177u, 181u, 215u, 247u,
};

struct gallery_view {
    const char *family;
    const char *face;
    const struct hpclang_font *font;
    const uint32_t *unicode_codepoints;
    uint32_t unicode_count;
    const uint32_t *private_codepoints;
    uint32_t private_count;
};

/*
 * Gallery chrome deliberately does not use either the font under test or a
 * calculator-native font. These 4x6 rows are the same mechanically extracted
 * X11 misc-fixed public-domain raster that hpclang used before native-ROM
 * fonts were introduced. Lowercase labels are folded to uppercase so the tiny
 * chrome only needs U+0020..U+005F.
 *
 * The specimen itself always goes through the public hpclang font API.
 */
static const uint8_t chrome_rows[64][6] = {
    {0x00u,0x00u,0x00u,0x00u,0x00u,0x00u}, /* 20 */
    {0x04u,0x04u,0x04u,0x00u,0x04u,0x00u}, /* 21 */
    {0x0au,0x0au,0x00u,0x00u,0x00u,0x00u}, /* 22 */
    {0x0au,0x0fu,0x0au,0x0fu,0x0au,0x00u}, /* 23 */
    {0x04u,0x0eu,0x0cu,0x02u,0x0eu,0x04u}, /* 24 */
    {0x08u,0x02u,0x04u,0x08u,0x02u,0x00u}, /* 25 */
    {0x04u,0x0au,0x04u,0x0au,0x05u,0x00u}, /* 26 */
    {0x04u,0x04u,0x00u,0x00u,0x00u,0x00u}, /* 27 */
    {0x02u,0x04u,0x04u,0x04u,0x04u,0x02u}, /* 28 */
    {0x08u,0x04u,0x04u,0x04u,0x04u,0x08u}, /* 29 */
    {0x0au,0x04u,0x0eu,0x04u,0x0au,0x00u}, /* 2A */
    {0x04u,0x04u,0x0eu,0x04u,0x04u,0x00u}, /* 2B */
    {0x00u,0x00u,0x00u,0x00u,0x04u,0x08u}, /* 2C */
    {0x00u,0x00u,0x0eu,0x00u,0x00u,0x00u}, /* 2D */
    {0x00u,0x00u,0x00u,0x00u,0x04u,0x00u}, /* 2E */
    {0x02u,0x02u,0x04u,0x08u,0x08u,0x00u}, /* 2F */
    {0x04u,0x0au,0x0eu,0x0au,0x04u,0x00u}, /* 30 */
    {0x04u,0x0cu,0x04u,0x04u,0x0eu,0x00u}, /* 31 */
    {0x04u,0x0au,0x02u,0x04u,0x0eu,0x00u}, /* 32 */
    {0x0eu,0x02u,0x04u,0x02u,0x0cu,0x00u}, /* 33 */
    {0x0au,0x0au,0x0eu,0x02u,0x02u,0x00u}, /* 34 */
    {0x0eu,0x08u,0x0cu,0x02u,0x0cu,0x00u}, /* 35 */
    {0x06u,0x08u,0x0cu,0x0au,0x04u,0x00u}, /* 36 */
    {0x0eu,0x02u,0x04u,0x08u,0x08u,0x00u}, /* 37 */
    {0x06u,0x0au,0x04u,0x0au,0x0cu,0x00u}, /* 38 */
    {0x04u,0x0au,0x06u,0x02u,0x0cu,0x00u}, /* 39 */
    {0x00u,0x04u,0x00u,0x00u,0x04u,0x00u}, /* 3A */
    {0x00u,0x04u,0x00u,0x00u,0x04u,0x08u}, /* 3B */
    {0x02u,0x04u,0x08u,0x04u,0x02u,0x00u}, /* 3C */
    {0x00u,0x0eu,0x00u,0x0eu,0x00u,0x00u}, /* 3D */
    {0x08u,0x04u,0x02u,0x04u,0x08u,0x00u}, /* 3E */
    {0x0cu,0x02u,0x04u,0x00u,0x04u,0x00u}, /* 3F */
    {0x06u,0x0au,0x0au,0x08u,0x06u,0x00u}, /* 40 */
    {0x04u,0x0au,0x0eu,0x0au,0x0au,0x00u}, /* 41 */
    {0x0cu,0x0au,0x0cu,0x0au,0x0cu,0x00u}, /* 42 */
    {0x04u,0x0au,0x08u,0x0au,0x04u,0x00u}, /* 43 */
    {0x0cu,0x0au,0x0au,0x0au,0x0cu,0x00u}, /* 44 */
    {0x0eu,0x08u,0x0cu,0x08u,0x0eu,0x00u}, /* 45 */
    {0x0eu,0x08u,0x0cu,0x08u,0x08u,0x00u}, /* 46 */
    {0x06u,0x08u,0x0au,0x0au,0x06u,0x00u}, /* 47 */
    {0x0au,0x0au,0x0eu,0x0au,0x0au,0x00u}, /* 48 */
    {0x0eu,0x04u,0x04u,0x04u,0x0eu,0x00u}, /* 49 */
    {0x02u,0x02u,0x02u,0x0au,0x04u,0x00u}, /* 4A */
    {0x0au,0x0au,0x0cu,0x0au,0x0au,0x00u}, /* 4B */
    {0x08u,0x08u,0x08u,0x08u,0x0eu,0x00u}, /* 4C */
    {0x0au,0x0eu,0x0eu,0x0au,0x0au,0x00u}, /* 4D */
    {0x02u,0x0au,0x0eu,0x0au,0x08u,0x00u}, /* 4E */
    {0x04u,0x0au,0x0au,0x0au,0x04u,0x00u}, /* 4F */
    {0x0cu,0x0au,0x0cu,0x08u,0x08u,0x00u}, /* 50 */
    {0x04u,0x0au,0x0au,0x0au,0x04u,0x02u}, /* 51 */
    {0x0cu,0x0au,0x0cu,0x0au,0x0au,0x00u}, /* 52 */
    {0x06u,0x08u,0x04u,0x02u,0x0cu,0x00u}, /* 53 */
    {0x0eu,0x04u,0x04u,0x04u,0x04u,0x00u}, /* 54 */
    {0x0au,0x0au,0x0au,0x0au,0x0eu,0x00u}, /* 55 */
    {0x0au,0x0au,0x0au,0x0eu,0x04u,0x00u}, /* 56 */
    {0x0au,0x0au,0x0eu,0x0eu,0x0au,0x00u}, /* 57 */
    {0x0au,0x0au,0x04u,0x0au,0x0au,0x00u}, /* 58 */
    {0x0au,0x0au,0x04u,0x04u,0x04u,0x00u}, /* 59 */
    {0x0eu,0x02u,0x04u,0x08u,0x0eu,0x00u}, /* 5A */
    {0x06u,0x04u,0x04u,0x04u,0x06u,0x00u}, /* 5B */
    {0x08u,0x08u,0x04u,0x02u,0x02u,0x00u}, /* 5C */
    {0x0cu,0x04u,0x04u,0x04u,0x0cu,0x00u}, /* 5D */
    {0x04u,0x0au,0x00u,0x00u,0x00u,0x00u}, /* 5E */
    {0x00u,0x00u,0x00u,0x00u,0x00u,0x0eu}, /* 5F */
};

static uint16_t family_face_position[HPCLANG_GALLERY_COUNT_MAX + 1u];

static void hold_result(void)
{
    for (volatile uint32_t n = 12000000u; n != 0u; --n)
        __asm__ volatile("nop");
}

static int fail(int code)
{
    hpclang_e2e_result = HPCLANG_E2E_FAIL | (uint32_t)code;
    hold_result();
    return code;
}

static uint32_t gallery_count(void)
{
    return hpclang_gallery_face_count + 2u;
}

static struct gallery_view gallery_at(uint32_t index)
{
    if (index == 0u) {
        const struct gallery_view view = {
            "Native", "calculator minifont", hpclang_font_mini(),
            native_unicode,
            (uint32_t)(sizeof(native_unicode) / sizeof(native_unicode[0])),
            NULL, 0u,
        };
        return view;
    }
    if (index == 1u) {
        const struct gallery_view view = {
            "Native", "calculator main font", hpclang_font_default(),
            native_unicode,
            (uint32_t)(sizeof(native_unicode) / sizeof(native_unicode[0])),
            NULL, 0u,
        };
        return view;
    }

    const struct hpclang_gallery_face *face =
        &hpclang_gallery_faces[index - 2u];
    const struct gallery_view view = {
        face->family,
        face->face,
        face->font,
        face->unicode_codepoints,
        face->unicode_count,
        face->private_codepoints,
        face->private_count,
    };
    return view;
}

static uint32_t family_first(uint32_t index)
{
    struct gallery_view current = gallery_at(index);
    while (index > 0u) {
        struct gallery_view previous = gallery_at(index - 1u);
        if (strcmp(previous.family, current.family) != 0)
            break;
        --index;
    }
    return index;
}

static uint32_t family_last(uint32_t index, uint32_t count)
{
    uint32_t first = family_first(index);
    struct gallery_view current = gallery_at(first);
    uint32_t last = first;
    while (last + 1u < count) {
        struct gallery_view next = gallery_at(last + 1u);
        if (strcmp(next.family, current.family) != 0)
            break;
        ++last;
    }
    return last;
}

static void remember_face(uint32_t index)
{
    uint32_t first = family_first(index);
    family_face_position[first] = (uint16_t)(index - first);
}

static uint32_t restore_face(uint32_t first, uint32_t count)
{
    uint32_t last = family_last(first, count);
    uint32_t offset = family_face_position[first];
    return offset <= last - first ? first + offset : first;
}

static uint32_t next_family_first(uint32_t index, uint32_t count)
{
    uint32_t last = family_last(index, count);
    return last + 1u < count ? last + 1u : 0u;
}

static uint32_t previous_family_first(uint32_t index, uint32_t count)
{
    uint32_t first = family_first(index);
    return first == 0u ? family_first(count - 1u) : family_first(first - 1u);
}

static uint32_t step_family(uint32_t index, uint32_t count, int forward)
{
    remember_face(index);
    uint32_t first = forward
        ? next_family_first(index, count)
        : previous_family_first(index, count);
    return restore_face(first, count);
}

static uint32_t step_face(uint32_t index, uint32_t count, int forward)
{
    uint32_t first = family_first(index);
    uint32_t last = family_last(index, count);
    uint32_t next;
    if (forward)
        next = index == last ? first : index + 1u;
    else
        next = index == first ? last : index - 1u;
    remember_face(next);
    return next;
}

static void chrome_char(
    struct hpclang_surface *surface,
    int32_t x,
    int32_t y,
    unsigned char c)
{
    if (c >= 'a' && c <= 'z')
        c = (unsigned char)(c - ('a' - 'A'));
    if (c < 0x20u || c > 0x5fu)
        c = '?';
    const uint8_t *rows = chrome_rows[c - 0x20u];
    for (int32_t yy = 0; yy < 6; ++yy) {
        uint8_t row = rows[yy];
        for (int32_t xx = 0; xx < 4; ++xx) {
            if ((row & (uint8_t)(1u << (3 - xx))) != 0u)
                hpclang_surface_set_pixel(surface, x + xx, y + yy, 255u);
        }
    }
}

static void chrome_text(
    struct hpclang_surface *surface,
    int32_t x,
    int32_t y,
    int32_t right,
    const char *text)
{
    for (const unsigned char *p = (const unsigned char *)text;
         *p != 0u && x + 4 <= right; ++p) {
        chrome_char(surface, x, y, *p);
        x += 5;
    }
}

static int32_t text_width(const struct hpclang_font *font, const char *text)
{
    int32_t width = 0;
    for (const unsigned char *p = (const unsigned char *)text; *p != 0u; ++p)
        width += (int32_t)hpclang_font_codepoint_advance(font, *p);
    return width;
}

static void draw_ascii_clipped(
    struct hpclang_surface *surface,
    const struct hpclang_font *font,
    int32_t x,
    int32_t y,
    int32_t right,
    const char *text)
{
    for (const unsigned char *p = (const unsigned char *)text; *p != 0u; ++p) {
        uint32_t advance = hpclang_font_codepoint_advance(font, *p);
        if (x + (int32_t)advance > right)
            break;
        hpclang_surface_draw_codepoint(surface, font, x, y, *p, 255u);
        x += (int32_t)advance;
    }
}

static void draw_centered_ascii(
    struct hpclang_surface *surface,
    const struct hpclang_font *font,
    int32_t y,
    const char *text)
{
    int32_t width = text_width(font, text);
    int32_t span = CONTENT_RIGHT - CONTENT_LEFT + 1;
    int32_t x = CONTENT_LEFT + (span - width) / 2;
    if (x < CONTENT_LEFT)
        x = CONTENT_LEFT;
    draw_ascii_clipped(surface, font, x, y, CONTENT_RIGHT + 1, text);
}

static int friendly_unicode(uint32_t cp)
{
    /* Box-drawing, block elements and geometric shapes are valuable stress
       probes, but they make a normal specimen look like rendering corruption.
       Keep them out of the display Gallery; a diagnostic probe can own them. */
    return !(cp >= 0x2500u && cp <= 0x25ffu);
}

static int32_t draw_codepoint_rows(
    struct hpclang_surface *surface,
    const struct hpclang_font *font,
    const uint32_t *codepoints,
    uint32_t count,
    int filter_unicode,
    int32_t y,
    int32_t bottom,
    uint32_t max_glyphs)
{
    uint32_t line_height = hpclang_font_height(font);
    if (line_height == 0u || codepoints == NULL || count == 0u)
        return y;

    int32_t x = CONTENT_LEFT;
    uint32_t drawn = 0u;
    for (uint32_t i = 0u; i < count && drawn < max_glyphs; ++i) {
        uint32_t cp = codepoints[i];
        if (filter_unicode && !friendly_unicode(cp))
            continue;
        uint32_t advance = hpclang_font_codepoint_advance(font, cp);
        if (advance == 0u)
            continue;
        if (x + (int32_t)advance > CONTENT_RIGHT + 1) {
            x = CONTENT_LEFT;
            y += (int32_t)line_height;
        }
        if (y + (int32_t)line_height > bottom)
            break;
        hpclang_surface_draw_codepoint(surface, font, x, y, cp, 255u);
        x += (int32_t)advance + 1;
        ++drawn;
    }
    return drawn != 0u ? y + (int32_t)line_height : y;
}

static int32_t draw_words(
    struct hpclang_surface *surface,
    const struct hpclang_font *font,
    int32_t y,
    int32_t bottom,
    const char *text)
{
    uint32_t line_height = hpclang_font_height(font);
    if (line_height == 0u)
        return y;

    const char *p = text;
    char line[64];
    size_t used = 0u;
    int32_t width = 0;
    const int32_t max_width = CONTENT_RIGHT - CONTENT_LEFT + 1;

    while (*p != '\0' && y + (int32_t)line_height <= bottom) {
        while (*p == ' ')
            ++p;
        if (*p == '\0')
            break;

        const char *word = p;
        size_t word_len = 0u;
        int32_t word_width = 0;
        while (p[word_len] != '\0' && p[word_len] != ' ') {
            word_width += (int32_t)hpclang_font_codepoint_advance(
                font, (uint8_t)p[word_len]);
            ++word_len;
        }

        int32_t space_width = used != 0u
            ? (int32_t)hpclang_font_codepoint_advance(font, (uint32_t)' ')
            : 0;
        if (used != 0u && width + space_width + word_width > max_width) {
            line[used] = '\0';
            draw_centered_ascii(surface, font, y, line);
            y += (int32_t)line_height;
            used = 0u;
            width = 0;
            continue;
        }

        if (used != 0u && used + 1u < sizeof(line)) {
            line[used++] = ' ';
            width += space_width;
        }
        for (size_t i = 0u; i < word_len && used + 1u < sizeof(line); ++i)
            line[used++] = word[i];
        width += word_width;
        p += word_len;
    }

    if (used != 0u && y + (int32_t)line_height <= bottom) {
        line[used] = '\0';
        draw_centered_ascii(surface, font, y, line);
        y += (int32_t)line_height;
    }
    return y;
}

static void draw_chevron(
    struct hpclang_surface *surface,
    int right,
    int32_t center_y)
{
    /* Fixed 6x11 raster, inset from the LCD edge. It is deliberately independent
       of both the specimen font and the Gallery chrome font. */
    int32_t tip = right ? 127 : 3;
    int32_t tail = right ? 122 : 8;
    hpclang_surface_draw_line(surface, tail, center_y - 5, tip, center_y, 255u);
    hpclang_surface_draw_line(surface, tip, center_y, tail, center_y + 5, 255u);
}

static void draw_chrome(
    struct hpclang_surface *surface,
    const struct gallery_view *view,
    uint32_t height)
{
    chrome_text(surface, CHROME_LEFT, 1, CHROME_RIGHT, view->family);
    chrome_text(surface, CHROME_LEFT, 8, CHROME_RIGHT, view->face);
    draw_chevron(surface, 0, (int32_t)height / 2 + 2);
    draw_chevron(surface, 1, (int32_t)height / 2 + 2);
}

static void draw_basic_page(
    struct hpclang_surface *surface,
    const struct gallery_view *view,
    uint32_t height)
{
    const struct hpclang_font *font = view->font;
    const int32_t bottom = (int32_t)height - CONTENT_BOTTOM_MARGIN;
    const uint32_t line_height = hpclang_font_height(font);
    int32_t y = CONTENT_TOP;

    if (line_height == 0u)
        return;

    if (view->private_count != 0u) {
        y = draw_codepoint_rows(
            surface, font, view->private_codepoints, view->private_count,
            0, y, bottom, 12u);
        if (y < bottom)
            (void)draw_codepoint_rows(
                surface, font, view->unicode_codepoints, view->unicode_count,
                1, y, bottom, 10u);
        return;
    }

    if (line_height >= 18u) {
        int32_t centered_y = CONTENT_TOP +
            (bottom - CONTENT_TOP - (int32_t)line_height) / 2;
        draw_centered_ascii(surface, font, centered_y, "Aa 09");
        return;
    }

    if (y + (int32_t)line_height <= bottom) {
        draw_centered_ascii(surface, font, y,
            line_height >= 12u ? "AaBbCc" : "AaBbCc XxYyZz");
        y += (int32_t)line_height;
    }
    if (y + (int32_t)line_height <= bottom) {
        draw_centered_ascii(surface, font, y, "0123456789");
        y += (int32_t)line_height;
    }
    if (line_height < 12u && y + (int32_t)line_height <= bottom) {
        draw_centered_ascii(surface, font, y, "!?%&@# +-=/*");
        y += (int32_t)line_height;
    }
    if (y < bottom)
        (void)draw_codepoint_rows(
            surface, font, view->unicode_codepoints, view->unicode_count,
            1, y, bottom, 16u);
}

static void draw_paragraph_page(
    struct hpclang_surface *surface,
    const struct gallery_view *view,
    uint32_t height)
{
    const struct hpclang_font *font = view->font;
    const int32_t bottom = (int32_t)height - CONTENT_BOTTOM_MARGIN;
    const uint32_t line_height = hpclang_font_height(font);
    int32_t y = CONTENT_TOP;

    if (line_height == 0u)
        return;

    if (view->private_count != 0u) {
        y = draw_codepoint_rows(
            surface, font, view->private_codepoints, view->private_count,
            0, y, bottom, 12u);
        if (y < bottom)
            (void)draw_codepoint_rows(
                surface, font, view->unicode_codepoints, view->unicode_count,
                1, y, bottom, 12u);
        return;
    }

    if (line_height >= 15u) {
        draw_centered_ascii(surface, font, CONTENT_TOP, "Quick fox");
        if (CONTENT_TOP + (int32_t)line_height * 2 <= bottom)
            draw_centered_ascii(
                surface, font, CONTENT_TOP + (int32_t)line_height, "012345");
        return;
    }

    y = draw_words(
        surface,
        font,
        CONTENT_TOP,
        bottom,
        "The quick brown fox jumps over the lazy dog. Pack my box with five dozen liquor jugs. 0123456789.");
    if (y < bottom)
        (void)draw_codepoint_rows(
            surface, font, view->unicode_codepoints, view->unicode_count,
            1, y, bottom, 12u);
}

static int next_is_same_family(uint32_t index, uint32_t count)
{
    if (index + 1u >= count)
        return 0;
    struct gallery_view current = gallery_at(index);
    struct gallery_view next = gallery_at(index + 1u);
    return strcmp(current.family, next.family) == 0;
}

static void publish_state(uint32_t index, uint32_t count, uint32_t page)
{
    hpclang_e2e_result = HPCLANG_GALLERY_STATE |
        ((page & 1u) << 23) |
        (next_is_same_family(index, count) ? HPCLANG_GALLERY_NEXT_FACE : 0u) |
        ((count & HPCLANG_GALLERY_COUNT_MAX) << 12) |
        (index & HPCLANG_GALLERY_INDEX_MAX);
}

static int redraw(
    struct hpclang_surface *surface,
    uint32_t index,
    uint32_t count,
    uint32_t page,
    uint32_t height)
{
    struct gallery_view view = gallery_at(index);
    if (view.font == NULL ||
        hpclang_font_height(view.font) == 0u ||
        hpclang_font_advance(view.font) == 0u)
        return 0;

    hpclang_surface_clear(surface, 0u);
    draw_chrome(surface, &view, height);
    if (page == 0u)
        draw_basic_page(surface, &view, height);
    else
        draw_paragraph_page(surface, &view, height);
    publish_state(index, count, page);
    return 1;
}

int main(void)
{
    const struct hpclang_device_info *device = hpclang_device_info();
    if (device == NULL ||
        device->version != HPCLANG_DEVICE_INFO_VERSION ||
        device->display_width != LCD_WIDTH ||
        (device->display_height != 64u && device->display_height != 80u))
        return fail(1);

    struct hpclang_surface *surface = hpclang_display_surface();
    if (surface == NULL || hpclang_surface_levels(surface) != 2u)
        return fail(2);

    uint32_t count = gallery_count();
    if (count < 3u || count > HPCLANG_GALLERY_COUNT_MAX)
        return fail(3);

    uint32_t index = 0u;
    uint32_t page = 0u;
    remember_face(index);
    if (!redraw(surface, index, count, page, device->display_height))
        return fail(4);

    hpclang_keyboard_events_reset();
    hpclang_keyboard_events_set_timing(10000u, 400000u, 120000u);

    for (;;) {
        struct hpclang_keyboard_event event;
        if (!hpclang_keyboard_event_poll(&event))
            continue;
        if (event.type != HPCLANG_KEYBOARD_EVENT_DOWN &&
            event.type != HPCLANG_KEYBOARD_EVENT_REPEAT)
            continue;

        int changed = 0;
        if (event.key == HPCLANG_KEY_LEFT) {
            index = step_family(index, count, 0);
            changed = 1;
        } else if (event.key == HPCLANG_KEY_RIGHT) {
            index = step_family(index, count, 1);
            changed = 1;
        } else if (event.key == HPCLANG_KEY_UP) {
            index = step_face(index, count, 0);
            changed = 1;
        } else if (event.key == HPCLANG_KEY_DOWN) {
            index = step_face(index, count, 1);
            changed = 1;
        } else if (event.key == HPCLANG_KEY_ENTER &&
                   event.type == HPCLANG_KEYBOARD_EVENT_DOWN) {
            page ^= 1u;
            changed = 1;
        } else if (event.key == HPCLANG_KEY_ON &&
                   event.type == HPCLANG_KEYBOARD_EVENT_DOWN) {
            hpclang_e2e_result = HPCLANG_E2E_PASS;
            hold_result();
            return 0;
        }

        if (changed &&
            !redraw(surface, index, count, page, device->display_height))
            return fail(5);
    }
}
