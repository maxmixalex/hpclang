#include <errno.h>
#include "lcd-engine.h"

/* Samsung S3C2410A manual, LCD controller special registers (15-26..15-33).
 * LINECNT is read-only; only bits 17:0 of CONTROL1 are configuration. */
#define CONTROL1_CONFIG UINT32_C(0x3ffff)
#define CONTROL5_CONFIG UINT32_C(0x1fff)
#define LCD_BANK_BYTES UINT32_C(0x400000)

static int bus_valid(const struct hpclang_lcd_bus *bus)
{
    return bus && bus->read && bus->write;
}

int hpclang_lcd_buffer_validate(uint32_t physical, uint32_t bytes)
{
    if (!bytes || (physical & 3u) || physical >= UINT32_C(0x80000000) ||
        bytes >= LCD_BANK_BYTES - (physical & (LCD_BANK_BYTES - 1u))) {
        errno = ERANGE;
        return -1;
    }
    return 0;
}

static void install(const struct hpclang_lcd_bus *bus, const struct hpclang_lcd_state *value)
{
    /* Disable before changing packing, stride or bank; enable only after all
     * configuration is installed. Do not echo the read-only line counter. */
    uint32_t control = bus->read(bus->context, LCD_CONTROL1) & CONTROL1_CONFIG;
    bus->write(bus->context, LCD_CONTROL1, control & ~1u);
    bus->write(bus->context, LCD_CONTROL5, value->control5);
    bus->write(bus->context, LCD_ADDRESS1, value->address1);
    bus->write(bus->context, LCD_ADDRESS2, value->address2);
    bus->write(bus->context, LCD_ADDRESS3, value->address3);
    bus->write(bus->context, LCD_BLUE_LUT, value->blue_lut);
    bus->write(bus->context, LCD_DITHER, value->dither);
    bus->write(bus->context, LCD_CONTROL1, value->control1);
}

int hpclang_lcd_acquire(const struct hpclang_lcd_bus *bus, struct hpclang_lcd_state *state,
                        uint32_t width, uint32_t height, uint32_t levels, uint32_t physical,
                        uint32_t capacity, struct hpclang_lcd_layout *layout)
{
    if (!bus_valid(bus) || !state || !layout || !width || !height ||
        (levels != 2 && levels != 4 && levels != 16)) {
        errno = EINVAL;
        return -1;
    }
    if (state->owned) {
        errno = EBUSY;
        return -1;
    }
    struct hpclang_lcd_state original = {
        .control1 = bus->read(bus->context, LCD_CONTROL1) & CONTROL1_CONFIG,
        .control5 = bus->read(bus->context, LCD_CONTROL5) & CONTROL5_CONFIG,
        .address1 = bus->read(bus->context, LCD_ADDRESS1),
        .address2 = bus->read(bus->context, LCD_ADDRESS2),
        .address3 = bus->read(bus->context, LCD_ADDRESS3),
        .blue_lut = bus->read(bus->context, LCD_BLUE_LUT),
        .dither = bus->read(bus->context, LCD_DITHER),
        .owned = 1,
    };
    /* Only the calculators' 4-bit single-scan STN panel is supported here.
     * Preserve its actual scan geometry, including non-visible rows/columns. */
    if ((original.control1 & 0x60u) != 0x20u || (original.control1 & 0x1eu) > 4u) {
        errno = ENOTSUP;
        return -1;
    }
    uint32_t scan_width = (((bus->read(bus->context, LCD_CONTROL3) >> 8) & 0x7ffu) + 1u) * 4u;
    uint32_t rows = ((bus->read(bus->context, LCD_CONTROL2) >> 14) & 0x3ffu) + 1u;
    uint32_t mode = levels == 2 ? 0u : levels == 4 ? 1u : 2u;
    uint32_t bits = 1u << mode;
    uint32_t stride = scan_width * bits / 8u;
    if (scan_width < width || rows < height || (scan_width * bits) % 32u || stride / 2u > 0x7ffu) {
        errno = ENOTSUP;
        return -1;
    }
    uint32_t bytes = stride * rows;
    /* ADDRESS2 stores only A[21:1]. The exclusive end must also remain in the
     * same bank; accepting an endpoint exactly on the boundary would wrap it. */
    if (hpclang_lcd_buffer_validate(physical, bytes)) {
        return -1;
    }
    if (capacity < bytes) {
        errno = ENOSPC;
        return -1;
    }

    struct hpclang_lcd_state next = original;
    next.control1 = (original.control1 & ~0x1eu) | (mode << 1);
    next.control5 = (original.control5 & ~3u) | (2u - mode);
    next.address1 = physical >> 1;
    next.address2 = ((physical + bytes) >> 1) & 0x1fffffu;
    next.address3 = stride / 2u;
    next.blue_lut = 0xfa70u;
    next.dither = mode ? 0x12210u : 0u;
    install(bus, &next);
    *state = original;
    *layout = (struct hpclang_lcd_layout){stride, rows, bytes};
    return 0;
}

void hpclang_lcd_release(const struct hpclang_lcd_bus *bus, struct hpclang_lcd_state *state)
{
    if (!bus_valid(bus) || !state || !state->owned)
        return;
    install(bus, state);
    state->owned = 0;
}

int hpclang_lcd_wait_frame(const struct hpclang_lcd_bus *bus, int (*clock)(uint64_t *),
                           uint32_t timeout_us)
{
    if (!bus_valid(bus) || !clock || !timeout_us || timeout_us > 1000000u) {
        errno = EINVAL;
        return -1;
    }
    uint64_t start, now, previous_time;
    if (clock(&start))
        return -1;
    previous_time = start;
    uint32_t previous = (bus->read(bus->context, LCD_CONTROL1) >> 18) & 1023u;
    unsigned stalled = 0;
    for (;;) {
        uint32_t control = bus->read(bus->context, LCD_CONTROL1);
        if (!(control & 1u)) {
            errno = ENODEV;
            return -1;
        }
        if (clock(&now))
            return -1;
        if (now < previous_time || (now == previous_time && ++stalled == 65536u)) {
            errno = EIO;
            return -1;
        }
        if (now != previous_time)
            stalled = 0;
        previous_time = now;
        if (now - start >= timeout_us) {
            errno = ETIMEDOUT;
            return -1;
        }
        uint32_t line = (control >> 18) & 1023u;
        if (line > previous)
            return 0;
        previous = line;
    }
}
