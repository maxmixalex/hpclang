#include <stdint.h>
#include <string.h>

#include <hpclang/abi.h>
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
    uint32_t text_capable;
    uint32_t native;
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
            NULL, 0u, 1u, 1u,
        };
        return view;
    }
    if (index == 1u) {
        const struct gallery_view view = {
            "Native", "calculator main font", hpclang_font_default(),
            native_unicode,
            (uint32_t)(sizeof(native_unicode) / sizeof(native_unicode[0])),
            NULL, 0u, 1u, 1u,
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
        face->text_capable,
        0u,
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

static void chrome_text_centered(
    struct hpclang_surface *surface,
    int32_t y,
    const char *text)
{
    size_t n = strlen(text);
    int32_t width = n != 0u ? (int32_t)(n * 5u - 1u) : 0;
    int32_t x = (LCD_WIDTH - width) / 2;
    if (x < CONTENT_LEFT)
        x = CONTENT_LEFT;
    chrome_text(surface, x, y, CONTENT_RIGHT, text);
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

static int32_t draw_codepoint_rows(
    struct hpclang_surface *surface,
    const struct hpclang_font *font,
    const uint32_t *codepoints,
    uint32_t count,
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
        /* The font's own advance already includes its intended side bearing.
           Gallery must not add synthetic tracking on top of it. */
        x += (int32_t)advance;
        ++drawn;
    }
    return drawn != 0u ? y + (int32_t)line_height : y;
}

static int cp_in_range(uint32_t cp, uint32_t lo, uint32_t hi)
{
    return cp >= lo && cp <= hi;
}

static void append_unique(
    uint32_t *out,
    uint32_t *count,
    uint32_t capacity,
    uint32_t cp)
{
    if (*count >= capacity)
        return;
    for (uint32_t i = 0u; i < *count; ++i) {
        if (out[i] == cp)
            return;
    }
    out[(*count)++] = cp;
}

static void append_unicode_range(
    const struct gallery_view *view,
    uint32_t *out,
    uint32_t *count,
    uint32_t capacity,
    uint32_t lo,
    uint32_t hi,
    uint32_t limit)
{
    uint32_t added = 0u;
    for (uint32_t i = 0u;
         i < view->unicode_count && *count < capacity && added < limit; ++i) {
        uint32_t cp = view->unicode_codepoints[i];
        if (!cp_in_range(cp, lo, hi))
            continue;
        uint32_t before = *count;
        append_unique(out, count, capacity, cp);
        if (*count != before)
            ++added;
    }
}

static int view_has_unicode(
    const struct gallery_view *view,
    uint32_t cp)
{
    for (uint32_t i = 0u; i < view->unicode_count; ++i) {
        if (view->unicode_codepoints[i] == cp)
            return 1;
    }
    return 0;
}

static void append_preferred_unicode(
    const struct gallery_view *view,
    uint32_t *out,
    uint32_t *count,
    uint32_t capacity,
    const uint32_t *preferred,
    uint32_t preferred_count,
    uint32_t limit)
{
    uint32_t added = 0u;
    for (uint32_t i = 0u;
         i < preferred_count && *count < capacity && added < limit; ++i) {
        if (!view_has_unicode(view, preferred[i]))
            continue;
        uint32_t before = *count;
        append_unique(out, count, capacity, preferred[i]);
        if (*count != before)
            ++added;
    }
}

static uint32_t build_feature_probe(
    const struct gallery_view *view,
    uint32_t *out,
    uint32_t capacity)
{
    static const uint32_t kana[] = {
        12354u, 12356u, 12358u, 12360u, 12362u,
        12450u, 12452u, 12454u, 12456u, 12458u,
    };
    static const uint32_t hangul[] = {
        44032u, 45208u, 45796u, 54620u, 44544u,
    };
    static const uint32_t han[] = {
        22825u, 22320u, 20154u, 20013u, 25991u, 23383u,
        20307u, 39636u, 38376u, 38272u, 39118u, 39080u,
        40857u, 40845u, 35821u, 35486u, 35328u,
        22269u, 22283u, 21488u, 33274u, 28286u, 28771u,
    };
    static const uint32_t greek[] = { 937u, 945u, 946u, 955u, 960u };
    static const uint32_t math_arrow[] = {
        8592u, 8593u, 8594u, 8595u, 8596u,
        8730u, 8734u, 8776u, 8800u, 8804u, 8805u,
    };
    static const uint32_t box[] = {
        9484u, 9472u, 9488u, 9474u, 9492u, 9496u,
    };
    uint32_t count = 0u;

    for (uint32_t i = 0u; i < view->private_count && count < capacity && i < 6u; ++i)
        append_unique(out, &count, capacity, view->private_codepoints[i]);

    append_preferred_unicode(view, out, &count, capacity, kana,
        (uint32_t)(sizeof(kana) / sizeof(kana[0])), 5u);
    append_preferred_unicode(view, out, &count, capacity, hangul,
        (uint32_t)(sizeof(hangul) / sizeof(hangul[0])), 3u);
    append_preferred_unicode(view, out, &count, capacity, han,
        (uint32_t)(sizeof(han) / sizeof(han[0])), 8u);
    append_preferred_unicode(view, out, &count, capacity, greek,
        (uint32_t)(sizeof(greek) / sizeof(greek[0])), 4u);
    append_preferred_unicode(view, out, &count, capacity, math_arrow,
        (uint32_t)(sizeof(math_arrow) / sizeof(math_arrow[0])), 6u);
    append_preferred_unicode(view, out, &count, capacity, box,
        (uint32_t)(sizeof(box) / sizeof(box[0])), 6u);

    for (uint32_t i = 0u; i < view->unicode_count && count < capacity; ++i)
        append_unique(out, &count, capacity, view->unicode_codepoints[i]);
    return count;
}

static int32_t draw_features(
    struct hpclang_surface *surface,
    const struct gallery_view *view,
    int32_t y,
    int32_t bottom,
    uint32_t max_glyphs)
{
    uint32_t probe[24];
    uint32_t count = build_feature_probe(view, probe,
        (uint32_t)(sizeof(probe) / sizeof(probe[0])));
    return draw_codepoint_rows(
        surface, view->font, probe, count, y, bottom, max_glyphs);
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

static void draw_nav_triangle(
    struct hpclang_surface *surface,
    int right,
    int32_t center_y)
{
    /* A compact filled 3x5 pixel triangle is deliberately quieter than the
       previous 6x11 line-chevron and leaves the specimen as the focal point. */
    for (int32_t d = 0; d < 3; ++d) {
        int32_t x = right ? 127 - d : 3 + d;
        for (int32_t y = center_y - d; y <= center_y + d; ++y)
            hpclang_surface_set_pixel(surface, x, y, 255u);
    }
}

static void draw_chrome(
    struct hpclang_surface *surface,
    const struct gallery_view *view,
    uint32_t height)
{
    chrome_text(surface, CHROME_LEFT, 1, CHROME_RIGHT, view->family);
    chrome_text(surface, CHROME_LEFT, 8, CHROME_RIGHT, view->face);
    draw_nav_triangle(surface, 0, (int32_t)height / 2 + 2);
    draw_nav_triangle(surface, 1, (int32_t)height / 2 + 2);
}

static int native_raster_available(const struct hpclang_device_info *device)
{
    if (device->model == HPCLANG_MODEL_HP39GS ||
        device->model == HPCLANG_MODEL_HP40GS)
        return 1;

    /* HP48gII/49g+/50g native fonts require the live Saturn Read[] map carried
       by a stock launch. Synthetic Direct launchers intentionally do not fake
       one. HP39g+ also remains evidence-gated and uses its launch-provided map. */
    const struct hpclang_boot_info *boot = hpclang_boot_info();
    return boot != NULL && boot->saturn_state != 0u;
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

    if (!view->text_capable) {
        (void)draw_features(surface, view, y, bottom, 18u);
        return;
    }

    if (line_height >= 18u) {
        int32_t centered_y = CONTENT_TOP +
            (bottom - CONTENT_TOP - (int32_t)line_height) / 2;
        draw_centered_ascii(surface, font, centered_y, "Il1 0OQ");
        return;
    }

    const char *rows[] = {
        "Il1| 0OQ",
        "2Z5S6G8B",
        "aegxy rnMW",
        "0123456789",
        "{}[]() /\\",
    };
    for (uint32_t i = 0u;
         i < (uint32_t)(sizeof(rows) / sizeof(rows[0])) &&
         y + (int32_t)line_height <= bottom; ++i) {
        draw_centered_ascii(surface, font, y, rows[i]);
        y += (int32_t)line_height;
    }

    if ((view->unicode_count != 0u || view->private_count != 0u) && y < bottom)
        (void)draw_features(surface, view, y, bottom, 16u);
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

    if (!view->text_capable) {
        (void)draw_features(surface, view, y, bottom, 18u);
        return;
    }

    if (line_height >= 15u) {
        draw_centered_ascii(surface, font, CONTENT_TOP, "Il1 0OQ");
        y = CONTENT_TOP + (int32_t)line_height;
        if (y + (int32_t)line_height <= bottom) {
            draw_centered_ascii(surface, font, y, "aeg 258");
            y += (int32_t)line_height;
        }
    } else {
        y = draw_words(
            surface,
            font,
            CONTENT_TOP,
            bottom,
            "Il1| 0OQ 2Z5S 6G8B rn m w MW AV To aegxy "
            "0123456789 {}[]() <> /\\ _-+= *#@&%");
    }

    if (y < bottom && (view->unicode_count != 0u || view->private_count != 0u))
        (void)draw_features(surface, view, y, bottom, 12u);
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
    const struct hpclang_device_info *device,
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

    if (view.native && !native_raster_available(device)) {
        chrome_text_centered(surface, (int32_t)height / 2, "NATIVE MAP UNAVAILABLE");
        publish_state(index, count, page);
        return 1;
    }

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
    if (!redraw(surface, device, index, count, page, device->display_height))
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
            !redraw(surface, device, index, count, page, device->display_height))
            return fail(5);
    }
}
