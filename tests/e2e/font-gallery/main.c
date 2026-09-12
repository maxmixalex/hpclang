#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <hpclang/device.h>
#include <hpclang/display.h>
#include <hpclang/font.h>
#include <hpclang/keyboard.h>

#include "font-gallery-catalog.h"

#define HPCLANG_E2E_PASS UINT32_C(0x50415353)
#define HPCLANG_E2E_FAIL UINT32_C(0x46410000)
#define HPCLANG_GALLERY_STATE UINT32_C(0x47000000)
#define HPCLANG_GALLERY_COUNT_MAX UINT32_C(0x7ff)
#define HPCLANG_GALLERY_INDEX_MAX UINT32_C(0xfff)

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

static uint32_t gallery_family_first(uint32_t index)
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

static uint32_t gallery_next_family(uint32_t index, uint32_t count)
{
    uint32_t first = gallery_family_first(index);
    struct gallery_view current = gallery_at(first);
    uint32_t next = first + 1u;
    while (next < count) {
        struct gallery_view candidate = gallery_at(next);
        if (strcmp(candidate.family, current.family) != 0)
            return next;
        ++next;
    }
    return 0u;
}

static uint32_t gallery_previous_family(uint32_t index, uint32_t count)
{
    uint32_t first = gallery_family_first(index);
    if (first == 0u)
        return gallery_family_first(count - 1u);
    return gallery_family_first(first - 1u);
}

static int32_t ascii_width(const struct hpclang_font *font, const char *text)
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
    int32_t width = ascii_width(font, text);
    int32_t x = (131 - width) / 2;
    if (x < 7)
        x = 7;
    draw_ascii_clipped(surface, font, x, y, 124, text);
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
    const int32_t max_width = 116;

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
            if (y + (int32_t)line_height > bottom)
                break;
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

static int32_t draw_codepoints(
    struct hpclang_surface *surface,
    const struct hpclang_font *font,
    const uint32_t *codepoints,
    uint32_t count,
    int32_t y,
    int32_t bottom)
{
    uint32_t line_height = hpclang_font_height(font);
    if (line_height == 0u || codepoints == NULL || count == 0u)
        return y;

    int32_t x = 8;
    for (uint32_t i = 0u; i < count; ++i) {
        uint32_t cp = codepoints[i];
        uint32_t advance = hpclang_font_codepoint_advance(font, cp);
        if (advance == 0u)
            continue;
        if (x + (int32_t)advance > 123) {
            x = 8;
            y += (int32_t)line_height;
        }
        if (y + (int32_t)line_height > bottom)
            break;
        hpclang_surface_draw_codepoint(surface, font, x, y, cp, 255u);
        x += (int32_t)advance + 1;
    }
    return y + (int32_t)line_height;
}

static int32_t draw_codepoint_line(
    struct hpclang_surface *surface,
    const struct hpclang_font *font,
    const uint32_t *codepoints,
    uint32_t count,
    int32_t y,
    int32_t bottom)
{
    uint32_t line_height = hpclang_font_height(font);
    if (line_height == 0u || codepoints == NULL || count == 0u ||
        y + (int32_t)line_height > bottom)
        return y;

    int32_t x = 8;
    for (uint32_t i = 0u; i < count; ++i) {
        uint32_t advance = hpclang_font_codepoint_advance(font, codepoints[i]);
        if (advance == 0u)
            continue;
        if (x + (int32_t)advance > 123)
            break;
        hpclang_surface_draw_codepoint(
            surface, font, x, y, codepoints[i], 255u);
        x += (int32_t)advance + 1;
    }
    return y + (int32_t)line_height;
}

static void draw_arrow(
    struct hpclang_surface *surface,
    int right,
    int32_t center_y)
{
    int32_t tip = right ? 129 : 1;
    int32_t tail = right ? 125 : 5;
    hpclang_surface_draw_line(surface, tail, center_y - 4, tip, center_y, 255u);
    hpclang_surface_draw_line(surface, tip, center_y, tail, center_y + 4, 255u);
}

static void draw_chrome(
    struct hpclang_surface *surface,
    const struct gallery_view *view,
    uint32_t index,
    uint32_t count,
    uint32_t page,
    uint32_t height)
{
    const struct hpclang_font *ui = hpclang_font_mini();
    char counter[20];
    (void)snprintf(counter, sizeof(counter), "%lu/%lu",
        (unsigned long)(index + 1u), (unsigned long)count);

    int32_t counter_width = ascii_width(ui, counter);
    draw_ascii_clipped(surface, ui, 7, 0, 126 - counter_width, view->family);
    hpclang_surface_draw_text(
        surface, ui, 129 - counter_width, 0, counter, 255u);
    draw_ascii_clipped(surface, ui, 7, 6, 124, view->face);

    hpclang_surface_draw_line(surface, 7, 12, 123, 12, 255u);
    hpclang_surface_draw_line(
        surface, 7, (int32_t)height - 8, 123, (int32_t)height - 8, 255u);

    draw_arrow(surface, 0, (int32_t)height / 2);
    draw_arrow(surface, 1, (int32_t)height / 2);

    const char *mode = page == 0u ? "SAMPLE" : "TEXT";
    int32_t mode_width = ascii_width(ui, mode);
    hpclang_surface_draw_text(
        surface, ui, (131 - mode_width) / 2, (int32_t)height - 6, mode, 255u);
}

static void draw_sample(
    struct hpclang_surface *surface,
    const struct gallery_view *view,
    uint32_t height)
{
    const int32_t bottom = (int32_t)height - 10;
    const uint32_t line_height = hpclang_font_height(view->font);
    int32_t y = 15;

    if (line_height == 0u)
        return;

    /* Show real private-use mappings first when a face has them.  Keeping the
       special sample to one line guarantees that ordinary comparison rows are
       still visible, while the catalog ordering puts one glyph from each PUA
       page first (for example separate controller-button families). */
    if (view->private_count != 0u)
        y = draw_codepoint_line(
            surface, view->font, view->private_codepoints,
            view->private_count, y, bottom);

    if (y + (int32_t)line_height <= bottom) {
        draw_centered_ascii(surface, view->font, y, "AaBbMmXxZz 012345");
        y += (int32_t)line_height;
    }
    if (y + (int32_t)line_height <= bottom) {
        draw_centered_ascii(surface, view->font, y, "6789 !?@#$%&*+-=/<>");
        y += (int32_t)line_height;
    }
    if (y < bottom)
        (void)draw_codepoints(
            surface, view->font, view->unicode_codepoints,
            view->unicode_count, y, bottom);
}

static void draw_text_page(
    struct hpclang_surface *surface,
    const struct gallery_view *view,
    uint32_t height)
{
    const int32_t bottom = (int32_t)height - 10;
    int32_t y = draw_words(
        surface,
        view->font,
        15,
        bottom,
        "The quick brown fox jumps over the lazy dog. Sphinx of black quartz, judge my vow. Pack my box with five dozen liquor jugs. 0123456789.");
    if (y < bottom)
        (void)draw_codepoints(
            surface, view->font, view->unicode_codepoints,
            view->unicode_count, y, bottom);
}

static void publish_state(uint32_t index, uint32_t count, uint32_t page)
{
    hpclang_e2e_result = HPCLANG_GALLERY_STATE |
        ((page & 1u) << 23) |
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
    draw_chrome(surface, &view, index, count, page, height);
    if (page == 0u)
        draw_sample(surface, &view, height);
    else
        draw_text_page(surface, &view, height);
    publish_state(index, count, page);
    return 1;
}

int main(void)
{
    const struct hpclang_device_info *device = hpclang_device_info();
    if (device == NULL ||
        device->version != HPCLANG_DEVICE_INFO_VERSION ||
        device->display_width != 131u ||
        (device->display_height != 64u && device->display_height != 80u))
        return fail(1);

    struct hpclang_surface *surface = hpclang_display_surface();
    if (surface == NULL || hpclang_surface_levels(surface) != 2u)
        return fail(2);

    uint32_t count = gallery_count();
    if (count == 0u || count > HPCLANG_GALLERY_COUNT_MAX)
        return fail(3);

    uint32_t index = 0u;
    uint32_t page = 0u;
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
            index = index == 0u ? count - 1u : index - 1u;
            changed = 1;
        } else if (event.key == HPCLANG_KEY_RIGHT) {
            index = (index + 1u) % count;
            changed = 1;
        } else if (event.key == HPCLANG_KEY_UP) {
            index = gallery_previous_family(index, count);
            changed = 1;
        } else if (event.key == HPCLANG_KEY_DOWN) {
            index = gallery_next_family(index, count);
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
