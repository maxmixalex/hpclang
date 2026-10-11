#include <assert.h>
#include <stdint.h>
#include <hpclang/abi.h>
#include <hpclang/display.h>
#include <hpclang/font.h>
#include <hpclang/target.h>
#include "saturn-view.h"

static struct hpclang_boot_info boot;
static int mapping_available = 1;
const struct hpclang_boot_info *hpclang_boot_info(void)
{
    return &boot;
}

/* Distinct synthetic ROM records let the rendering path detect wrong character
 * indices without requiring proprietary fixtures in the portable host suite. */
int hpclang_saturn_read_nibble(struct hpclang_saturn_address address, uint8_t *value)
{
    if (!mapping_available)
        return 0;
    *value = (uint8_t)(((address.nibble >> 4) ^ (address.nibble >> 8) ^ address.nibble) & 15);
    return 1;
}
int hpclang_saturn_read_byte(struct hpclang_saturn_address address, uint8_t *value)
{
    uint8_t low, high;
    if (!hpclang_saturn_read_nibble(address, &low))
        return 0;
    ++address.nibble;
    hpclang_saturn_read_nibble(address, &high);
    *value = low | (uint8_t)(high << 4);
    return 1;
}

int main(void)
{
    const uint32_t unicode[] = {0x0394, 0x03a3, 0x03b1, 0x03b2, 0x03b4, 0x03b5,
                                0x03b8, 0x03bb, 0x03bc, 0x03c0, 0x03c1, 0x03c3,
                                0x03c4, 0x03c9, 0x2192, 0x221a, 0x2220, 0x03a0};
    const uint8_t native[] = {0x9b, 0x85, 0x8c, 0xdf, 0x92, 0x93, 0x95, 0x96, 0xb5,
                              0x87, 0x97, 0x98, 0x99, 0x9a, 0x8d, 0x83, 0x80, 0x9c};
    struct hpclang_surface *surface = hpclang_surface_create_mono(24, 12);
    assert(surface);
    /* HP39 reads a fixed private ROM window; its pixel gate runs on calculator
     * fixtures. This portable stub exercises the classic live Saturn reader. */
    for (boot.model = HPCLANG_MODEL_HP48GII; boot.model <= HPCLANG_MODEL_HP50G; ++boot.model) {
        const struct hpclang_font *fonts[] = {hpclang_font_mini(), hpclang_font_big(),
                                              hpclang_font_editline()};
        for (unsigned f = 0; f < 3; ++f) {
            if (!fonts[f])
                continue;
            assert(hpclang_font_available(fonts[f]));
            mapping_available = 0;
            assert(!hpclang_font_available(fonts[f]));
            mapping_available = 1;
            unsigned differences = 0;
            for (unsigned i = 0; i < sizeof(native); ++i) {
                hpclang_surface_clear(surface, 0);
                hpclang_surface_draw_codepoint(surface, fonts[f], 0, 0, unicode[i], 255);
                hpclang_surface_draw_char(surface, fonts[f], 8, 0, native[i], 255);
                hpclang_surface_draw_char(surface, fonts[f], 16, 0, '?', 255);
                assert(hpclang_font_codepoint_advance(fonts[f], unicode[i]) ==
                       hpclang_font_codepoint_advance(fonts[f], native[i]));
                for (int y = 0; y < 12; ++y)
                    for (int x = 0; x < 8; ++x) {
                        uint8_t pixel = hpclang_surface_get_pixel(surface, x, y);
                        assert(pixel == hpclang_surface_get_pixel(surface, x + 8, y));
                        differences += pixel != hpclang_surface_get_pixel(surface, x + 16, y);
                    }
            }
            assert(differences);
        }
    }
    hpclang_surface_destroy(surface);
    return 0;
}
