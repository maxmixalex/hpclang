#include <stddef.h>
#include <stdint.h>

#include <hpclang/abi.h>
#include <hpclang/bootstrap.h>
#include <hpclang/display.h>
#include <hpclang/font.h>
#include <hpclang/font-package.h>
#include <hpclang/target.h>

/* Internal kinds are intentionally outside the checked-in package ABI. */
#define FONT_KIND_HP39_MINI   0x1001u
#define FONT_KIND_HP39_MAIN   0x1002u
#define FONT_KIND_HP4950_MINI 0x1003u
#define FONT_KIND_HP4950_BIG  0x1004u

/* x50ng's HP39gs/40gs KINPO loader places the 16-byte update header at NOR
 * +0x4000 and the 1 MiB Saturn ROM body immediately after it. The native main
 * and mini font tables lie wholly inside physical NOR pages 0x42 and 0x43,
 * which the bootstrap exposes at HPCLANG_CODE_WINDOW_BASE. */
#define HP39_UPDATE_ROM_OFFSET      0x00004010u
#define HP39_FONT_NOR_PHYS_BASE     0x00042000u
#define HP39_FONT_WINDOW_BYTES      0x00002000u

#define HP39_MAIN_FIRST_NIBBLE      0x0007c651u
#define HP39_MAIN_LAST_NIBBLE       (HP39_MAIN_FIRST_NIBBLE + 255u * 16u + 15u)
#define HP39_MINI_FIRST_NIBBLE      0x0007e695u
#define HP39_MINI_LAST_NIBBLE       (HP39_MINI_FIRST_NIBBLE + 255u * 11u + 10u)

_Static_assert(
    HP39_UPDATE_ROM_OFFSET + (HP39_MAIN_FIRST_NIBBLE >> 1) >=
        HP39_FONT_NOR_PHYS_BASE,
    "HP39 main font must start inside private NOR window");
_Static_assert(
    HP39_UPDATE_ROM_OFFSET + (HP39_MAIN_LAST_NIBBLE >> 1) <
        HP39_FONT_NOR_PHYS_BASE + HP39_FONT_WINDOW_BYTES,
    "HP39 main font must fit inside private NOR window");
_Static_assert(
    HP39_UPDATE_ROM_OFFSET + (HP39_MINI_FIRST_NIBBLE >> 1) >=
        HP39_FONT_NOR_PHYS_BASE,
    "HP39 mini font must start inside private NOR window");
_Static_assert(
    HP39_UPDATE_ROM_OFFSET + (HP39_MINI_LAST_NIBBLE >> 1) <
        HP39_FONT_NOR_PHYS_BASE + HP39_FONT_WINDOW_BYTES,
    "HP39 mini font must fit inside private NOR window");

static const struct hpclang_font hp39_mini = {
    HPCLANG_FONT_PACKAGE_ABI, FONT_KIND_HP39_MINI, NULL
};
static const struct hpclang_font hp39_main = {
    HPCLANG_FONT_PACKAGE_ABI, FONT_KIND_HP39_MAIN, NULL
};
static const struct hpclang_font hp4950_mini = {
    HPCLANG_FONT_PACKAGE_ABI, FONT_KIND_HP4950_MINI, NULL
};
static const struct hpclang_font hp4950_big = {
    HPCLANG_FONT_PACKAGE_ABI, FONT_KIND_HP4950_BIG, NULL
};

static uint32_t active_model(void)
{
    const struct hpclang_boot_info *boot = hpclang_boot_info();
    return boot != NULL ? boot->model : HPCLANG_MODEL_UNKNOWN;
}

static int hp39_family(uint32_t model)
{
    return model == HPCLANG_MODEL_HP39GPLUS ||
           model == HPCLANG_MODEL_HP39GS ||
           model == HPCLANG_MODEL_HP40GS;
}

const struct hpclang_font *hpclang_font_mini(void)
{
    return hp39_family(active_model()) ? &hp39_mini : &hp4950_mini;
}

const struct hpclang_font *hpclang_font_big(void)
{
    return hp39_family(active_model()) ? &hp39_main : &hp4950_big;
}

const struct hpclang_font *hpclang_font_default(void)
{
    return hpclang_font_big();
}

static const struct hpclang_bitmap_font_data *bitmap_data(
    const struct hpclang_font *font)
{
    if (font == NULL ||
        font->abi != HPCLANG_FONT_PACKAGE_ABI ||
        font->kind != HPCLANG_FONT_PACKAGE_KIND_BITMAP)
        return NULL;
    return (const struct hpclang_bitmap_font_data *)font->data;
}

uint32_t hpclang_font_height(const struct hpclang_font *font)
{
    const struct hpclang_bitmap_font_data *bitmap = bitmap_data(font);
    if (bitmap != NULL)
        return bitmap->line_height;
    if (font == NULL || font->abi != HPCLANG_FONT_PACKAGE_ABI)
        return 0u;
    if (font->kind == FONT_KIND_HP39_MINI)
        return 5u;
    if (font->kind == FONT_KIND_HP4950_MINI)
        return 6u;
    if (font->kind == FONT_KIND_HP39_MAIN ||
        font->kind == FONT_KIND_HP4950_BIG)
        return 8u;
    return 0u;
}

uint32_t hpclang_font_advance(const struct hpclang_font *font)
{
    const struct hpclang_bitmap_font_data *bitmap = bitmap_data(font);
    if (bitmap != NULL)
        return bitmap->nominal_advance;
    if (font == NULL || font->abi != HPCLANG_FONT_PACKAGE_ABI)
        return 0u;
    if (font->kind == FONT_KIND_HP39_MINI ||
        font->kind == FONT_KIND_HP4950_MINI)
        return 4u;
    if (font->kind == FONT_KIND_HP39_MAIN ||
        font->kind == FONT_KIND_HP4950_BIG)
        return 6u;
    return 0u;
}

static uintptr_t saturn_page_base(uint32_t saturn_nibble_address)
{
    const struct hpclang_boot_info *boot = hpclang_boot_info();
    if (boot == NULL || boot->saturn_state == 0u)
        return 0u;

    /* HPGCC's SAT_CPU starts with Read[257]. The launch ABI deliberately
     * preserves that Saturn-state pointer for logical Saturn RAM/object
     * access. Fixed HP39gs/40gs ROM raster assets intentionally do not use
     * this live bank view; see hp39_physical_nibble(). */
    const uint32_t *read_map =
        (const uint32_t *)(uintptr_t)boot->saturn_state;
    return (uintptr_t)read_map[saturn_nibble_address >> 12];
}

static uint8_t saturn_nibble(uint32_t address)
{
    uintptr_t base = saturn_page_base(address);
    if (base == 0u)
        return 0u;

    const volatile uint8_t *p =
        (const volatile uint8_t *)(base + ((address & 0xfffu) >> 1));
    uint8_t value = *p;
    return (address & 1u) != 0u ? (uint8_t)(value >> 4) :
                                  (uint8_t)(value & 0x0fu);
}

static uint8_t saturn_byte(uint32_t address)
{
    return (uint8_t)(saturn_nibble(address) |
                     (uint8_t)(saturn_nibble(address + 1u) << 4));
}

static uintptr_t hp39_font_window_base(void)
{
    const struct hpclang_boot_info *boot = hpclang_boot_info();
    if (boot == NULL ||
        (boot->model != HPCLANG_MODEL_HP39GS &&
         boot->model != HPCLANG_MODEL_HP40GS))
        return 0u;

    return boot->platform_cookie == (uintptr_t)HPCLANG_CODE_WINDOW_BASE
        ? boot->platform_cookie
        : 0u;
}

static uint8_t hp39_physical_nibble(uint32_t address)
{
    uint32_t model = active_model();

    /* HP39g+ still lacks a trustworthy stock full-flash fixture. Do not infer
       the gs/40gs update-image layout for it; retain the live Saturn mapping
       until its own NOR layout is evidenced on real hardware. */
    if (model == HPCLANG_MODEL_HP39GPLUS)
        return saturn_nibble(address);

    if (model != HPCLANG_MODEL_HP39GS && model != HPCLANG_MODEL_HP40GS)
        return 0u;

    /*
     * HP39gs/40gs stock mini/main raster tables are specified in Saturn ROM
     * nibble addresses. Their KINPO firmware body is packed at physical NOR
     * +0x4010. Bootstrap maps only the two physical NOR pages that contain the
     * font tables into the private prefix of hpclang's own code coarse table.
     * Validate the opaque cookie and the physical offset before dereferencing
     * that window so a stale launcher/runtime contract fails closed.
     */
    uintptr_t window_base = hp39_font_window_base();
    uint32_t byte_offset = HP39_UPDATE_ROM_OFFSET + (address >> 1);
    if (window_base == 0u ||
        byte_offset < HP39_FONT_NOR_PHYS_BASE ||
        byte_offset >= HP39_FONT_NOR_PHYS_BASE + HP39_FONT_WINDOW_BYTES)
        return 0u;

    const volatile uint8_t *p = (const volatile uint8_t *)(
        window_base + (byte_offset - HP39_FONT_NOR_PHYS_BASE));
    uint8_t value = *p;
    return (address & 1u) != 0u ? (uint8_t)(value >> 4) :
                                  (uint8_t)(value & 0x0fu);
}

static uint8_t hp39_physical_byte(uint32_t address)
{
    return (uint8_t)(hp39_physical_nibble(address) |
                     (uint8_t)(hp39_physical_nibble(address + 1u) << 4));
}

static uint32_t native_byte_codepoint(uint32_t codepoint)
{
    return codepoint <= 0xffu ? codepoint : (uint32_t)'?';
}

static uint32_t hp39_mini_codepoint(uint32_t codepoint)
{
    codepoint = native_byte_codepoint(codepoint);
    /* The stock HP39 small-font path case-folds ASCII lower-case before the
     * ROM lookup. Preserve that observable native behavior. */
    if (codepoint >= (uint32_t)'a' && codepoint <= (uint32_t)'z')
        codepoint -= (uint32_t)('a' - 'A');
    return codepoint;
}

static uint32_t hp39_mini_advance(uint32_t codepoint)
{
    /* 0x07E695 is the physical Saturn-ROM start of 256 records, each
     * 1 advance nibble + 10 bitmap nibbles. */
    uint32_t index = hp39_mini_codepoint(codepoint);
    uint32_t advance = hp39_physical_nibble(0x07e695u + index * 11u);
    return advance != 0u ? advance : 4u;
}

static const struct hpclang_bitmap_glyph *bitmap_glyph(
    const struct hpclang_bitmap_font_data *font,
    uint32_t codepoint)
{
    if (font == NULL || font->glyphs == NULL || font->glyph_count == 0u)
        return NULL;

    uint32_t lo = 0u;
    uint32_t hi = font->glyph_count;
    while (lo < hi) {
        uint32_t mid = lo + (hi - lo) / 2u;
        const struct hpclang_bitmap_glyph *glyph = font->glyphs[mid];
        if (glyph->codepoint < codepoint)
            lo = mid + 1u;
        else
            hi = mid;
    }
    if (lo < font->glyph_count &&
        font->glyphs[lo]->codepoint == codepoint)
        return font->glyphs[lo];

    if (codepoint == font->fallback_codepoint)
        return NULL;

    lo = 0u;
    hi = font->glyph_count;
    while (lo < hi) {
        uint32_t mid = lo + (hi - lo) / 2u;
        const struct hpclang_bitmap_glyph *glyph = font->glyphs[mid];
        if (glyph->codepoint < font->fallback_codepoint)
            lo = mid + 1u;
        else
            hi = mid;
    }
    return lo < font->glyph_count &&
           font->glyphs[lo]->codepoint == font->fallback_codepoint
        ? font->glyphs[lo]
        : NULL;
}

uint32_t hpclang_font_codepoint_advance(
    const struct hpclang_font *font,
    uint32_t codepoint)
{
    const struct hpclang_bitmap_font_data *bitmap = bitmap_data(font);
    if (bitmap != NULL) {
        const struct hpclang_bitmap_glyph *glyph =
            bitmap_glyph(bitmap, codepoint);
        return glyph != NULL ? glyph->advance : bitmap->nominal_advance;
    }
    if (font != NULL && font->kind == FONT_KIND_HP39_MINI)
        return hp39_mini_advance(codepoint);
    return hpclang_font_advance(font);
}

static void draw_hp39_mini(
    struct hpclang_surface *surface,
    int32_t x,
    int32_t y,
    uint32_t codepoint,
    uint8_t intensity)
{
    uint32_t index = hp39_mini_codepoint(codepoint);
    uint32_t record = 0x07e695u + index * 11u;
    for (uint32_t yy = 0u; yy < 5u; ++yy) {
        uint8_t row = hp39_physical_byte(record + 1u + yy * 2u);
        for (uint32_t xx = 0u; xx < 8u; ++xx) {
            if ((row & (uint8_t)(1u << xx)) != 0u)
                hpclang_surface_set_pixel(
                    surface, x + (int32_t)xx, y + (int32_t)yy, intensity);
        }
    }
}

static void draw_hp39_main(
    struct hpclang_surface *surface,
    int32_t x,
    int32_t y,
    uint32_t codepoint,
    uint8_t intensity)
{
    uint32_t index = native_byte_codepoint(codepoint);
    uint32_t record = 0x07c651u + index * 16u;
    for (uint32_t yy = 0u; yy < 8u; ++yy) {
        uint8_t row = hp39_physical_byte(record + yy * 2u);
        for (uint32_t xx = 0u; xx < 5u; ++xx) {
            if ((row & (uint8_t)(1u << xx)) != 0u)
                hpclang_surface_set_pixel(
                    surface, x + (int32_t)xx, y + (int32_t)yy, intensity);
        }
    }
}

static void draw_hp4950_mini(
    struct hpclang_surface *surface,
    int32_t x,
    int32_t y,
    uint32_t codepoint,
    uint8_t intensity)
{
    uint32_t index = native_byte_codepoint(codepoint);
    uint32_t record = 0x0812c8u + 7u + index * 6u;
    for (uint32_t yy = 0u; yy < 6u; ++yy) {
        uint8_t row = saturn_nibble(record + yy);
        for (uint32_t xx = 0u; xx < 4u; ++xx) {
            if ((row & (uint8_t)(1u << xx)) != 0u)
                hpclang_surface_set_pixel(
                    surface, x + (int32_t)xx, y + (int32_t)yy, intensity);
        }
    }
}

static uint32_t hp4950_big_bitmap_base(void)
{
    /* HPGCC get_bigfont() locates the stock font object at 0x84D82, then
     * skips its byte-oriented header/name. Convert that byte skip back to
     * Saturn nibbles so page crossings still go through the Read[] map. */
    const uint32_t object = 0x084d82u;
    uint8_t name_length = saturn_byte(object + 14u); /* ptr[7] */
    uint32_t skip_bytes = name_length != 0u ? 9u + name_length : 8u;
    return object + skip_bytes * 2u;
}

static void draw_hp4950_big(
    struct hpclang_surface *surface,
    int32_t x,
    int32_t y,
    uint32_t codepoint,
    uint8_t intensity)
{
    uint32_t index = native_byte_codepoint(codepoint);
    uint32_t record = hp4950_big_bitmap_base() + index * 16u;
    for (uint32_t yy = 0u; yy < 8u; ++yy) {
        uint8_t row = saturn_byte(record + yy * 2u);
        for (uint32_t xx = 0u; xx < 8u; ++xx) {
            if ((row & (uint8_t)(1u << xx)) != 0u)
                hpclang_surface_set_pixel(
                    surface, x + (int32_t)xx, y + (int32_t)yy, intensity);
        }
    }
}

static void draw_bitmap_glyph(
    struct hpclang_surface *surface,
    const struct hpclang_bitmap_font_data *font,
    int32_t x,
    int32_t y,
    uint32_t codepoint,
    uint8_t intensity)
{
    const struct hpclang_bitmap_glyph *glyph = bitmap_glyph(font, codepoint);
    if (glyph == NULL || glyph->bitmap == NULL || glyph->stride == 0u)
        return;

    int32_t left = x + glyph->x_offset;
    int32_t top = y + (int32_t)font->ascent -
                  (int32_t)glyph->y_offset - (int32_t)glyph->height;

    for (uint32_t yy = 0u; yy < glyph->height; ++yy) {
        const uint8_t *row = glyph->bitmap + yy * glyph->stride;
        for (uint32_t xx = 0u; xx < glyph->width; ++xx) {
            if ((row[xx >> 3] &
                 (uint8_t)(0x80u >> (xx & 7u))) != 0u)
                hpclang_surface_set_pixel(
                    surface,
                    left + (int32_t)xx,
                    top + (int32_t)yy,
                    intensity);
        }
    }
}

void hpclang_surface_draw_codepoint(
    struct hpclang_surface *surface,
    const struct hpclang_font *font,
    int32_t x,
    int32_t y,
    uint32_t codepoint,
    uint8_t intensity)
{
    if (surface == NULL || font == NULL ||
        font->abi != HPCLANG_FONT_PACKAGE_ABI)
        return;

    const struct hpclang_bitmap_font_data *bitmap = bitmap_data(font);
    if (bitmap != NULL) {
        draw_bitmap_glyph(surface, bitmap, x, y, codepoint, intensity);
        return;
    }

    switch (font->kind) {
    case FONT_KIND_HP39_MINI:
        draw_hp39_mini(surface, x, y, codepoint, intensity);
        break;
    case FONT_KIND_HP39_MAIN:
        draw_hp39_main(surface, x, y, codepoint, intensity);
        break;
    case FONT_KIND_HP4950_MINI:
        draw_hp4950_mini(surface, x, y, codepoint, intensity);
        break;
    case FONT_KIND_HP4950_BIG:
        draw_hp4950_big(surface, x, y, codepoint, intensity);
        break;
    default:
        break;
    }
}

void hpclang_surface_draw_char(
    struct hpclang_surface *surface,
    const struct hpclang_font *font,
    int32_t x,
    int32_t y,
    uint8_t character,
    uint8_t intensity)
{
    hpclang_surface_draw_codepoint(
        surface, font, x, y, character, intensity);
}

static uint32_t utf8_next(const char **cursor)
{
    const uint8_t *s = (const uint8_t *)*cursor;
    uint32_t c = *s++;
    if (c < 0x80u) {
        *cursor = (const char *)s;
        return c;
    }

    uint32_t value;
    uint32_t minimum;
    unsigned continuation;
    if (c >= 0xc2u && c <= 0xdfu) {
        value = c & 0x1fu;
        minimum = 0x80u;
        continuation = 1u;
    } else if (c >= 0xe0u && c <= 0xefu) {
        value = c & 0x0fu;
        minimum = 0x800u;
        continuation = 2u;
    } else if (c >= 0xf0u && c <= 0xf4u) {
        value = c & 0x07u;
        minimum = 0x10000u;
        continuation = 3u;
    } else {
        *cursor = (const char *)s;
        return 0xfffdu;
    }

    const uint8_t *p = s;
    for (unsigned i = 0u; i < continuation; ++i) {
        if ((p[i] & 0xc0u) != 0x80u) {
            *cursor = (const char *)s;
            return 0xfffdu;
        }
        value = (value << 6) | (uint32_t)(p[i] & 0x3fu);
    }

    if (value < minimum || value > 0x10ffffu ||
        (value >= 0xd800u && value <= 0xdfffu)) {
        *cursor = (const char *)s;
        return 0xfffdu;
    }

    *cursor = (const char *)(p + continuation);
    return value;
}

void hpclang_surface_draw_utf8(
    struct hpclang_surface *surface,
    const struct hpclang_font *font,
    int32_t x,
    int32_t y,
    const char *text,
    uint8_t intensity)
{
    if (surface == NULL || font == NULL || text == NULL)
        return;

    int32_t origin_x = x;
    while (*text != '\0') {
        uint32_t codepoint = utf8_next(&text);
        if (codepoint == (uint32_t)'\n') {
            x = origin_x;
            y += (int32_t)hpclang_font_height(font);
            continue;
        }
        hpclang_surface_draw_codepoint(
            surface, font, x, y, codepoint, intensity);
        x += (int32_t)hpclang_font_codepoint_advance(font, codepoint);
    }
}

void hpclang_surface_draw_text(
    struct hpclang_surface *surface,
    const struct hpclang_font *font,
    int32_t x,
    int32_t y,
    const char *text,
    uint8_t intensity)
{
    hpclang_surface_draw_utf8(surface, font, x, y, text, intensity);
}