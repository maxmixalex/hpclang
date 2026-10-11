#include <stddef.h>
#include <stdint.h>

#include <hpclang/abi.h>
#include <hpclang/bootstrap.h>
#include <hpclang/display.h>
#include <hpclang/font.h>
#include <hpclang/font-package.h>
#include <hpclang/target.h>

#include "saturn-view.h"

/* Internal kinds are intentionally outside the checked-in package ABI. */
#define FONT_KIND_HP39_MINI 0x1001u
#define FONT_KIND_HP39_MAIN 0x1002u
#define FONT_KIND_HP4950_MINI 0x1003u
#define FONT_KIND_HP4950_BIG 0x1004u
#define FONT_KIND_HP39_EDITLINE 0x1005u

/*
 * The HP39/40G atlas establishes the record format and character ordering, but
 * each ROM relocates them: HP39g+ D1.55 uses pages 0x6e/0x6f, while
 * HP39gs/40gs 2.22 uses pages 0x7e/0x7f. Host-side scans establish the
 * model-specific nibble bases below. Bootstrap maps the two physical pages
 * consecutively at HPCLANG_CODE_WINDOW_BASE; runtime translates legacy atlas
 * coordinates to the evidenced model-specific window coordinates.
 */
#define HP39_FONT_WINDOW_PAGE_BYTES 0x00001000u
#define HP39_FONT_WINDOW_PAGE_NIBBLES (HP39_FONT_WINDOW_PAGE_BYTES * 2u)
#define HP39_FONT_WINDOW_BYTES 0x00002000u
#define HP39_FONT_WINDOW_NIBBLES (HP39_FONT_WINDOW_BYTES * 2u)

#define HP39_MAIN_FIRST_NIBBLE 0x0007c651u
#define HP39_MAIN_LAST_NIBBLE (HP39_MAIN_FIRST_NIBBLE + 255u * 16u + 15u)
#define HP39_EDITLINE_FIRST_NIBBLE 0x0007d3eau
#define HP39_EDITLINE_RECORD_NIBBLES 20u
#define HP39_EDITLINE_LAST_NIBBLE                                                                  \
    (HP39_EDITLINE_FIRST_NIBBLE + 255u * HP39_EDITLINE_RECORD_NIBBLES +                            \
     HP39_EDITLINE_RECORD_NIBBLES - 1u)
#define HP39_MINI_FIRST_NIBBLE 0x0007e695u
#define HP39_MINI_GLYPH_COUNT 155u
#define HP39_MINI_RECORD_NIBBLES 11u
#define HP39_MINI_MAP_FIRST_CODE 0xa0u
#define HP39_MINI_MAP_COUNT 96u
#define HP39_MINI_MAP_FIRST_NIBBLE                                                                 \
    (HP39_MINI_FIRST_NIBBLE + HP39_MINI_GLYPH_COUNT * HP39_MINI_RECORD_NIBBLES)
#define HP39_MINI_LAST_NIBBLE (HP39_MINI_MAP_FIRST_NIBBLE + HP39_MINI_MAP_COUNT * 2u - 1u)

/* HP39g+ D1.55: independently located in the physical full-NOR fixture,
 * pages 0x6e/0x6f. Complete tables match the reviewed 2.22 rasters. */
#define HP39GPLUS_MAIN_WINDOW_FIRST_NIBBLE 0x00000096u
#define HP39GPLUS_EDITLINE_WINDOW_FIRST_NIBBLE 0x00000e2fu
#define HP39GPLUS_MINI_WINDOW_FIRST_NIBBLE 0x000020dau

/* HP39gs 2.22: main @ NOR 0x7e574 high nibble, EditLine candidate @
 * NOR 0x7ec41 low nibble, mini @ 0x7f596 high. The EditLine relocation is
 * independently gated against stock NOR by verify-hp39-native-font.py. */
#define HP39GS_MAIN_WINDOW_FIRST_NIBBLE 0x00000ae9u
#define HP39GS_EDITLINE_WINDOW_FIRST_NIBBLE 0x00001882u
#define HP39GS_MINI_WINDOW_FIRST_NIBBLE (HP39_FONT_WINDOW_PAGE_NIBBLES + 0x00000b2du)

/* HP40gs 2.22: main @ NOR 0x7e64e low nibble, EditLine candidate @
 * NOR 0x7ed1a high nibble, mini @ 0x7f670 low. */
#define HP40GS_MAIN_WINDOW_FIRST_NIBBLE 0x00000c9cu
#define HP40GS_EDITLINE_WINDOW_FIRST_NIBBLE 0x00001a35u
#define HP40GS_MINI_WINDOW_FIRST_NIBBLE (HP39_FONT_WINDOW_PAGE_NIBBLES + 0x00000ce0u)

#define HP39_MAIN_SPAN_NIBBLES (HP39_MAIN_LAST_NIBBLE - HP39_MAIN_FIRST_NIBBLE)
#define HP39_EDITLINE_SPAN_NIBBLES (HP39_EDITLINE_LAST_NIBBLE - HP39_EDITLINE_FIRST_NIBBLE)
#define HP39_MINI_SPAN_NIBBLES (HP39_MINI_LAST_NIBBLE - HP39_MINI_FIRST_NIBBLE)

_Static_assert(HP39GPLUS_MAIN_WINDOW_FIRST_NIBBLE + HP39_MAIN_SPAN_NIBBLES <
                   HP39_FONT_WINDOW_PAGE_NIBBLES,
               "HP39g+ main font must fit inside first private window page");
_Static_assert(HP39GPLUS_EDITLINE_WINDOW_FIRST_NIBBLE + HP39_EDITLINE_SPAN_NIBBLES <
                   HP39_FONT_WINDOW_NIBBLES,
               "HP39g+ EditLine font must fit inside private window");
_Static_assert(HP39GPLUS_MINI_WINDOW_FIRST_NIBBLE + HP39_MINI_SPAN_NIBBLES <
                   HP39_FONT_WINDOW_NIBBLES,
               "HP39g+ mini font must fit inside private window");

_Static_assert(HP39GS_MAIN_WINDOW_FIRST_NIBBLE + HP39_MAIN_SPAN_NIBBLES <
                   HP39_FONT_WINDOW_PAGE_NIBBLES,
               "HP39gs main font must fit inside first private window page");
_Static_assert(HP40GS_MAIN_WINDOW_FIRST_NIBBLE + HP39_MAIN_SPAN_NIBBLES <
                   HP39_FONT_WINDOW_PAGE_NIBBLES,
               "HP40gs main font must fit inside first private window page");
_Static_assert(HP39GS_EDITLINE_WINDOW_FIRST_NIBBLE + HP39_EDITLINE_SPAN_NIBBLES <
                   HP39_FONT_WINDOW_NIBBLES,
               "HP39gs EditLine font must fit inside private window");
_Static_assert(HP40GS_EDITLINE_WINDOW_FIRST_NIBBLE + HP39_EDITLINE_SPAN_NIBBLES <
                   HP39_FONT_WINDOW_NIBBLES,
               "HP40gs EditLine font must fit inside private window");
_Static_assert(HP39GS_MINI_WINDOW_FIRST_NIBBLE + HP39_MINI_SPAN_NIBBLES < HP39_FONT_WINDOW_NIBBLES,
               "HP39gs mini font must fit inside second private window page");
_Static_assert(HP40GS_MINI_WINDOW_FIRST_NIBBLE + HP39_MINI_SPAN_NIBBLES < HP39_FONT_WINDOW_NIBBLES,
               "HP40gs mini font must fit inside second private window page");

static const struct hpclang_font hp39_mini = {HPCLANG_FONT_PACKAGE_ABI, FONT_KIND_HP39_MINI, NULL};
static const struct hpclang_font hp39_main = {HPCLANG_FONT_PACKAGE_ABI, FONT_KIND_HP39_MAIN, NULL};
static const struct hpclang_font hp39_editline = {HPCLANG_FONT_PACKAGE_ABI, FONT_KIND_HP39_EDITLINE,
                                                  NULL};
static const struct hpclang_font hp4950_mini = {HPCLANG_FONT_PACKAGE_ABI, FONT_KIND_HP4950_MINI,
                                                NULL};
static const struct hpclang_font hp4950_big = {HPCLANG_FONT_PACKAGE_ABI, FONT_KIND_HP4950_BIG,
                                               NULL};

static uint32_t active_model(void)
{
    const struct hpclang_boot_info *boot = hpclang_boot_info();
    return boot != NULL ? boot->model : HPCLANG_MODEL_UNKNOWN;
}

static int hp39_family(uint32_t model)
{
    return model == HPCLANG_MODEL_HP39GPLUS || model == HPCLANG_MODEL_HP39GS ||
           model == HPCLANG_MODEL_HP40GS;
}

const struct hpclang_font *hpclang_font_mini(void)
{
    return hp39_family(active_model()) ? &hp39_mini : &hp4950_mini;
}

const struct hpclang_font *hpclang_font_big(void)
{
    return hp39_family(active_model()) ? NULL : &hp4950_big;
}

const struct hpclang_font *hpclang_font_editline(void)
{
    return hp39_family(active_model()) ? &hp39_editline : NULL;
}

const struct hpclang_font *hpclang_font_default(void)
{
    return hp39_family(active_model()) ? &hp39_main : &hp4950_big;
}

static const struct hpclang_bitmap_font_data *bitmap_data(const struct hpclang_font *font)
{
    if (font == NULL || font->abi != HPCLANG_FONT_PACKAGE_ABI ||
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
    if (font->kind == FONT_KIND_HP39_MAIN || font->kind == FONT_KIND_HP4950_BIG)
        return 8u;
    if (font->kind == FONT_KIND_HP39_EDITLINE)
        return 10u;
    return 0u;
}

uint32_t hpclang_font_advance(const struct hpclang_font *font)
{
    const struct hpclang_bitmap_font_data *bitmap = bitmap_data(font);
    if (bitmap != NULL)
        return bitmap->nominal_advance;
    if (font == NULL || font->abi != HPCLANG_FONT_PACKAGE_ABI)
        return 0u;
    if (font->kind == FONT_KIND_HP39_MINI || font->kind == FONT_KIND_HP4950_MINI)
        return 4u;
    if (font->kind == FONT_KIND_HP39_MAIN || font->kind == FONT_KIND_HP4950_BIG ||
        font->kind == FONT_KIND_HP39_EDITLINE)
        return 6u;
    return 0u;
}

static uint8_t saturn_nibble(uint32_t address)
{
    uint8_t value = 0u;
    (void)hpclang_saturn_read_nibble(hpclang_saturn_address_make(address), &value);
    return value;
}

static uint8_t saturn_byte(uint32_t address)
{
    uint8_t value = 0u;
    (void)hpclang_saturn_read_byte(hpclang_saturn_address_make(address), &value);
    return value;
}

static uintptr_t hp39_font_window_base(void)
{
    const struct hpclang_boot_info *boot = hpclang_boot_info();
    if (boot == NULL || !hp39_family(boot->model))
        return 0u;

    if (boot->platform_cookie == (uintptr_t)HPCLANG_CODE_WINDOW_BASE)
        return boot->platform_cookie;

    if ((boot->platform_cookie & (uintptr_t)3u) != HPCLANG_PLATFORM_COOKIE_HP39_TAG)
        return 0u;

    const struct hpclang_hp39_platform_context *context =
        (const struct hpclang_hp39_platform_context *)(boot->platform_cookie & ~(uintptr_t)3u);
    return context->magic == HPCLANG_HP39_PLATFORM_MAGIC &&
                   context->font_window_base == (uintptr_t)HPCLANG_CODE_WINDOW_BASE
               ? context->font_window_base
               : 0u;
}

static uint32_t hp39_window_font_first_nibble(uint32_t model, uint32_t logical_first)
{
    if (model == HPCLANG_MODEL_HP39GPLUS) {
        if (logical_first == HP39_MAIN_FIRST_NIBBLE)
            return HP39GPLUS_MAIN_WINDOW_FIRST_NIBBLE;
        if (logical_first == HP39_EDITLINE_FIRST_NIBBLE)
            return HP39GPLUS_EDITLINE_WINDOW_FIRST_NIBBLE;
        if (logical_first == HP39_MINI_FIRST_NIBBLE)
            return HP39GPLUS_MINI_WINDOW_FIRST_NIBBLE;
    }
    if (model == HPCLANG_MODEL_HP39GS) {
        if (logical_first == HP39_MAIN_FIRST_NIBBLE)
            return HP39GS_MAIN_WINDOW_FIRST_NIBBLE;
        if (logical_first == HP39_EDITLINE_FIRST_NIBBLE)
            return HP39GS_EDITLINE_WINDOW_FIRST_NIBBLE;
        if (logical_first == HP39_MINI_FIRST_NIBBLE)
            return HP39GS_MINI_WINDOW_FIRST_NIBBLE;
    }
    if (model == HPCLANG_MODEL_HP40GS) {
        if (logical_first == HP39_MAIN_FIRST_NIBBLE)
            return HP40GS_MAIN_WINDOW_FIRST_NIBBLE;
        if (logical_first == HP39_EDITLINE_FIRST_NIBBLE)
            return HP40GS_EDITLINE_WINDOW_FIRST_NIBBLE;
        if (logical_first == HP39_MINI_FIRST_NIBBLE)
            return HP40GS_MINI_WINDOW_FIRST_NIBBLE;
    }
    return UINT32_MAX;
}

static uint8_t hp39_font_nibble(uint32_t logical_first, uint32_t address)
{
    uint32_t model = active_model();

    /* The legacy HP39 font tables overlap slightly in logical Saturn address
       space. The caller therefore names the concrete ROM font; never infer
       table identity from the address alone. Each supported ROM has its own
       independently verified physical layout. */
    if (address < logical_first)
        return 0u;
    if (!hp39_family(model))
        return 0u;

    uintptr_t window_base = hp39_font_window_base();
    uint32_t window_first = hp39_window_font_first_nibble(model, logical_first);
    if (window_base == 0u || window_first == UINT32_MAX)
        return 0u;

    uint32_t window_nibble = window_first + (address - logical_first);
    if (window_nibble >= HP39_FONT_WINDOW_NIBBLES)
        return 0u;

    const volatile uint8_t *p =
        (const volatile uint8_t *)(window_base + (uintptr_t)(window_nibble >> 1));
    uint8_t value = *p;
    return (window_nibble & 1u) != 0u ? (uint8_t)(value >> 4) : (uint8_t)(value & 0x0fu);
}

static uint8_t hp39_font_byte(uint32_t logical_first, uint32_t address)
{
    return (uint8_t)(hp39_font_nibble(logical_first, address) |
                     (uint8_t)(hp39_font_nibble(logical_first, address + 1u) << 4));
}

static uint32_t native_byte_codepoint(uint32_t codepoint)
{
    /* Stock keyboard symbols use HP character bytes. The UTF-8 editor and
     * generated fonts use Unicode; translate their scalar values at the native
     * font boundary. Retain the existing byte entry points. */
    switch (codepoint) {
    case 0x0394u:
        return 0x9bu; /* Delta */
    case 0x03a0u:
        return 0x9cu; /* Pi */
    case 0x03a3u:
        return 0x85u; /* Sigma */
    case 0x03b1u:
        return 0x8cu;
    case 0x03b2u:
        return 0xdfu;
    case 0x03b4u:
        return 0x92u;
    case 0x03b5u:
        return 0x93u;
    case 0x03b8u:
        return 0x95u;
    case 0x03bbu:
        return 0x96u;
    case 0x03bcu:
        return 0xb5u;
    case 0x03c0u:
        return 0x87u;
    case 0x03c1u:
        return 0x97u;
    case 0x03c3u:
        return 0x98u;
    case 0x03c4u:
        return 0x99u;
    case 0x03c9u:
        return 0x9au;
    case 0x2192u:
        return 0x8du;
    case 0x221au:
        return 0x83u;
    case 0x2220u:
        return 0x80u;
    default:
        break;
    }
    return codepoint <= 0xffu ? codepoint : (uint32_t)'?';
}

static uint32_t hp39_mini_index(uint32_t codepoint)
{
    codepoint = native_byte_codepoint(codepoint);

    /* The stock small-font atlas has 155 11-nibble glyph records. ASCII
     * lower-case is folded to upper-case, freeing atlas slots 0x61..0x7a.
     * HP character codes 0x7b..0x9f are packed continuously into 0x61..0x85.
     * Codes 0xa0..0xff use the 96-byte ROM map immediately after the atlas. */
    if (codepoint >= (uint32_t)'a' && codepoint <= (uint32_t)'z')
        return codepoint - (uint32_t)('a' - 'A');
    if (codepoint >= HP39_MINI_MAP_FIRST_CODE) {
        uint32_t offset = codepoint - HP39_MINI_MAP_FIRST_CODE;
        return hp39_font_byte(HP39_MINI_FIRST_NIBBLE, HP39_MINI_MAP_FIRST_NIBBLE + offset * 2u);
    }
    if (codepoint >= 0x7bu)
        return codepoint - 0x1au;
    return codepoint;
}

static uint32_t hp39_mini_advance(uint32_t codepoint)
{
    uint32_t index = hp39_mini_index(codepoint);
    uint32_t advance = hp39_font_nibble(HP39_MINI_FIRST_NIBBLE,
                                        HP39_MINI_FIRST_NIBBLE + index * HP39_MINI_RECORD_NIBBLES);
    return advance != 0u ? advance : 4u;
}

static const struct hpclang_bitmap_glyph *bitmap_glyph(const struct hpclang_bitmap_font_data *font,
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
    if (lo < font->glyph_count && font->glyphs[lo]->codepoint == codepoint)
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
    return lo < font->glyph_count && font->glyphs[lo]->codepoint == font->fallback_codepoint
               ? font->glyphs[lo]
               : NULL;
}

uint32_t hpclang_font_codepoint_advance(const struct hpclang_font *font, uint32_t codepoint)
{
    const struct hpclang_bitmap_font_data *bitmap = bitmap_data(font);
    if (bitmap != NULL) {
        const struct hpclang_bitmap_glyph *glyph = bitmap_glyph(bitmap, codepoint);
        return glyph != NULL ? glyph->advance : bitmap->nominal_advance;
    }
    if (font != NULL && font->kind == FONT_KIND_HP39_MINI)
        return hp39_mini_advance(codepoint);
    return hpclang_font_advance(font);
}

static void draw_hp39_mini(struct hpclang_surface *surface, int32_t x, int32_t y,
                           uint32_t codepoint, uint8_t intensity)
{
    uint32_t index = hp39_mini_index(codepoint);
    uint32_t record = HP39_MINI_FIRST_NIBBLE + index * HP39_MINI_RECORD_NIBBLES;
    for (uint32_t yy = 0u; yy < 5u; ++yy) {
        uint8_t row = hp39_font_byte(HP39_MINI_FIRST_NIBBLE, record + 1u + yy * 2u);
        for (uint32_t xx = 0u; xx < 8u; ++xx) {
            if ((row & (uint8_t)(1u << xx)) != 0u)
                hpclang_surface_set_pixel(surface, x + (int32_t)xx, y + (int32_t)yy, intensity);
        }
    }
}

static void draw_hp39_main(struct hpclang_surface *surface, int32_t x, int32_t y,
                           uint32_t codepoint, uint8_t intensity)
{
    uint32_t index = native_byte_codepoint(codepoint);
    uint32_t record = HP39_MAIN_FIRST_NIBBLE + index * 16u;
    for (uint32_t yy = 0u; yy < 8u; ++yy) {
        uint8_t row = hp39_font_byte(HP39_MAIN_FIRST_NIBBLE, record + yy * 2u);
        for (uint32_t xx = 0u; xx < 5u; ++xx) {
            if ((row & (uint8_t)(1u << xx)) != 0u)
                hpclang_surface_set_pixel(surface, x + (int32_t)xx, y + (int32_t)yy, intensity);
        }
    }
}

static void draw_hp39_editline(struct hpclang_surface *surface, int32_t x, int32_t y,
                               uint32_t codepoint, uint8_t intensity)
{
    uint32_t index = native_byte_codepoint(codepoint);
    uint32_t record = HP39_EDITLINE_FIRST_NIBBLE + index * HP39_EDITLINE_RECORD_NIBBLES;
    for (uint32_t yy = 0u; yy < 10u; ++yy) {
        uint8_t row = hp39_font_byte(HP39_EDITLINE_FIRST_NIBBLE, record + yy * 2u);
        for (uint32_t xx = 0u; xx < 6u; ++xx) {
            if ((row & (uint8_t)(1u << xx)) != 0u)
                hpclang_surface_set_pixel(surface, x + (int32_t)xx, y + (int32_t)yy, intensity);
        }
    }
}

static void draw_hp4950_mini(struct hpclang_surface *surface, int32_t x, int32_t y,
                             uint32_t codepoint, uint8_t intensity)
{
    uint32_t index = native_byte_codepoint(codepoint);
    uint32_t record = 0x0812c8u + 7u + index * 6u;
    for (uint32_t yy = 0u; yy < 6u; ++yy) {
        uint8_t row = saturn_nibble(record + yy);
        for (uint32_t xx = 0u; xx < 4u; ++xx) {
            if ((row & (uint8_t)(1u << xx)) != 0u)
                hpclang_surface_set_pixel(surface, x + (int32_t)xx, y + (int32_t)yy, intensity);
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

int hpclang_font_available(const struct hpclang_font *font)
{
    if (font == NULL || font->abi != HPCLANG_FONT_PACKAGE_ABI)
        return 0;
    const struct hpclang_bitmap_font_data *bitmap = bitmap_data(font);
    if (bitmap)
        return bitmap->line_height != 0 && bitmap->nominal_advance != 0;
    if (font->kind == FONT_KIND_HP39_MINI || font->kind == FONT_KIND_HP39_MAIN ||
        font->kind == FONT_KIND_HP39_EDITLINE)
        return hp39_font_window_base() != 0;
    uint32_t model = active_model();
    if (model < HPCLANG_MODEL_HP48GII || model > HPCLANG_MODEL_HP50G)
        return 0;
    uint32_t first, last;
    uint8_t ignored;
    if (font->kind == FONT_KIND_HP4950_MINI) {
        first = 0x0812c8u + 7u;
        last = first + 256u * 6u - 1u;
    } else if (font->kind == FONT_KIND_HP4950_BIG) {
        if (!hpclang_saturn_read_byte(hpclang_saturn_address_make(0x084d82u + 14u), &ignored))
            return 0;
        first = hp4950_big_bitmap_base();
        last = first + 256u * 16u - 1u;
    } else {
        return 0;
    }
    return hpclang_saturn_read_nibble(hpclang_saturn_address_make(first), &ignored) &&
           hpclang_saturn_read_nibble(hpclang_saturn_address_make(last), &ignored);
}

static void draw_hp4950_big(struct hpclang_surface *surface, int32_t x, int32_t y,
                            uint32_t codepoint, uint8_t intensity)
{
    uint32_t index = native_byte_codepoint(codepoint);
    uint32_t record = hp4950_big_bitmap_base() + index * 16u;
    for (uint32_t yy = 0u; yy < 8u; ++yy) {
        uint8_t row = saturn_byte(record + yy * 2u);
        for (uint32_t xx = 0u; xx < 8u; ++xx) {
            if ((row & (uint8_t)(1u << xx)) != 0u)
                hpclang_surface_set_pixel(surface, x + (int32_t)xx, y + (int32_t)yy, intensity);
        }
    }
}

static void draw_bitmap_glyph(struct hpclang_surface *surface,
                              const struct hpclang_bitmap_font_data *font, int32_t x, int32_t y,
                              uint32_t codepoint, uint8_t intensity)
{
    const struct hpclang_bitmap_glyph *glyph = bitmap_glyph(font, codepoint);
    if (glyph == NULL || glyph->bitmap == NULL || glyph->stride == 0u)
        return;

    int32_t left = x + glyph->x_offset;
    int32_t top = y + (int32_t)font->ascent - (int32_t)glyph->y_offset - (int32_t)glyph->height;

    for (uint32_t yy = 0u; yy < glyph->height; ++yy) {
        const uint8_t *row = glyph->bitmap + yy * glyph->stride;
        for (uint32_t xx = 0u; xx < glyph->width; ++xx) {
            if ((row[xx >> 3] & (uint8_t)(0x80u >> (xx & 7u))) != 0u)
                hpclang_surface_set_pixel(surface, left + (int32_t)xx, top + (int32_t)yy,
                                          intensity);
        }
    }
}

void hpclang_surface_draw_codepoint(struct hpclang_surface *surface,
                                    const struct hpclang_font *font, int32_t x, int32_t y,
                                    uint32_t codepoint, uint8_t intensity)
{
    if (surface == NULL || font == NULL || font->abi != HPCLANG_FONT_PACKAGE_ABI)
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
    case FONT_KIND_HP39_EDITLINE:
        draw_hp39_editline(surface, x, y, codepoint, intensity);
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

void hpclang_surface_draw_char(struct hpclang_surface *surface, const struct hpclang_font *font,
                               int32_t x, int32_t y, uint8_t character, uint8_t intensity)
{
    hpclang_surface_draw_codepoint(surface, font, x, y, character, intensity);
}

static uint32_t utf8_next(const char **cursor, const char *end)
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
        if ((end && (size_t)(end - (const char *)p) <= i) || (p[i] & 0xc0u) != 0x80u) {
            *cursor = (const char *)s;
            return 0xfffdu;
        }
        value = (value << 6) | (uint32_t)(p[i] & 0x3fu);
    }

    if (value < minimum || value > 0x10ffffu || (value >= 0xd800u && value <= 0xdfffu)) {
        *cursor = (const char *)s;
        return 0xfffdu;
    }

    *cursor = (const char *)(p + continuation);
    return value;
}

static void draw_text(struct hpclang_surface *surface, const struct hpclang_font *font, int32_t x,
                      int32_t y, const char *text, const char *end, uint8_t intensity)
{
    if (surface == NULL || font == NULL || text == NULL)
        return;

    int32_t origin_x = x;
    while ((!end || text < end) && *text != '\0') {
        uint32_t codepoint = utf8_next(&text, end);
        if (codepoint == (uint32_t)'\n') {
            x = origin_x;
            uint32_t height = hpclang_font_height(font);
            if ((int64_t)y + height > INT32_MAX)
                return;
            y = (int32_t)((int64_t)y + height);
            continue;
        }
        hpclang_surface_draw_codepoint(surface, font, x, y, codepoint, intensity);
        uint32_t advance = hpclang_font_codepoint_advance(font, codepoint);
        if ((int64_t)x + advance > INT32_MAX)
            return;
        x = (int32_t)((int64_t)x + advance);
    }
}

void hpclang_surface_draw_utf8(struct hpclang_surface *surface, const struct hpclang_font *font,
                               int32_t x, int32_t y, const char *text, uint8_t intensity)
{
    draw_text(surface, font, x, y, text, NULL, intensity);
}

void hpclang_surface_draw_text_n(struct hpclang_surface *surface, const struct hpclang_font *font,
                                 int32_t x, int32_t y, const char *text, size_t byte_count,
                                 uint8_t intensity)
{
    if (text)
        draw_text(surface, font, x, y, text, text + byte_count, intensity);
}

void hpclang_surface_draw_text(struct hpclang_surface *surface, const struct hpclang_font *font,
                               int32_t x, int32_t y, const char *text, uint8_t intensity)
{
    hpclang_surface_draw_utf8(surface, font, x, y, text, intensity);
}
