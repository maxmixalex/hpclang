#include <assert.h>
#include <errno.h>
#include <string.h>
#include "lcd-engine.h"

struct hardware
{
    uint32_t reg[25];
    unsigned writes;
};

static uint32_t read_reg(void *context, enum hpclang_lcd_reg reg)
{
    return ((struct hardware *)context)->reg[reg];
}

static void write_reg(void *context, enum hpclang_lcd_reg reg, uint32_t value)
{
    struct hardware *hardware = context;
    ++hardware->writes;
    if (reg == LCD_CONTROL1) {
        assert(!(value & ~0x3ffffu));
        hardware->reg[reg] = (hardware->reg[reg] & ~0x3ffffu) | value;
    } else {
        assert(!(hardware->reg[LCD_CONTROL1] & 1u));
        /* No timing, power, status or interrupt register may be written. */
        assert(reg == LCD_CONTROL5 || reg == LCD_ADDRESS1 || reg == LCD_ADDRESS2 ||
               reg == LCD_ADDRESS3 || reg == LCD_BLUE_LUT || reg == LCD_DITHER);
        if (reg == LCD_CONTROL5) {
            assert(!(value & ~0x1fffu));
            hardware->reg[reg] = (hardware->reg[reg] & ~0x1fffu) | value;
        } else
            hardware->reg[reg] = value;
    }
}

static struct hardware stock(uint32_t height, int enabled)
{
    struct hardware hardware = {0};
    hardware.reg[LCD_CONTROL1] = (17u << 18) | (50u << 8) | 0x20u | (uint32_t)enabled;
    hardware.reg[LCD_CONTROL2] = (height + 4u) << 14;
    hardware.reg[LCD_CONTROL3] = (39u << 8) | 12u;
    hardware.reg[LCD_CONTROL5] = 0x18302u; /* Polarity plus read-only TFT status. */
    hardware.reg[LCD_ADDRESS1] = 0x20000136u;
    hardware.reg[LCD_ADDRESS2] = 0x3e8u;
    hardware.reg[LCD_ADDRESS3] = 10;
    hardware.reg[LCD_BLUE_LUT] = 0x1234;
    hardware.reg[LCD_DITHER] = 0x5678;
    hardware.reg[3] = 0x5011; /* MVAL and line timing. */
    hardware.reg[21] = 3;     /* Interrupt pending, write-one-to-clear. */
    hardware.reg[22] = 2;
    hardware.reg[23] = 3; /* Interrupt mask. */
    return hardware;
}

static void rejected(struct hardware *hardware, uint32_t width, uint32_t height, uint32_t levels,
                     uint32_t address, uint32_t capacity, int error)
{
    struct hpclang_lcd_bus bus = {read_reg, write_reg, hardware};
    struct hpclang_lcd_state state = {0}, previous = state;
    struct hpclang_lcd_layout layout = {77, 88, 99};
    unsigned writes = hardware->writes;
    assert(hpclang_lcd_acquire(&bus, &state, width, height, levels, address, capacity, &layout) ==
           -1);
    assert(errno == error && hardware->writes == writes);
    assert(memcmp(&state, &previous, sizeof(state)) == 0);
    assert(layout.stride == 77 && layout.rows == 88 && layout.bytes == 99);
}

int main(void)
{
    const uint32_t levels[] = {2, 4, 16};
    for (uint32_t height = 64; height <= 80; height += 16) {
        for (unsigned mode = 0; mode < 3; ++mode) {
            for (int enabled = 0; enabled < 2; ++enabled) {
                struct hardware hardware = stock(height, enabled), original = hardware;
                struct hpclang_lcd_bus bus = {read_reg, write_reg, &hardware};
                struct hpclang_lcd_state state = {0};
                struct hpclang_lcd_layout layout;
                assert(hpclang_lcd_acquire(&bus, &state, 131, height, levels[mode], 0x08012000,
                                           8192, &layout) == 0);
                assert(state.owned && hardware.writes == 8);
                assert(layout.stride == (20u << mode));
                assert(layout.rows == height + 5 && layout.bytes == layout.stride * layout.rows);
                assert((hardware.reg[0] & 0x1eu) == mode * 2);
                assert((hardware.reg[0] & 1u) == (uint32_t)enabled);
                assert((hardware.reg[4] & 3u) == 2u - mode);
                assert((hardware.reg[4] & ~3u) == (original.reg[4] & ~3u));
                assert(hardware.reg[5] == 0x04009000);
                assert(hardware.reg[6] == 0x9000 + layout.bytes / 2);
                assert(hardware.reg[7] == layout.stride / 2);
                assert(hpclang_lcd_acquire(&bus, &state, 131, height, 4, 0x08014000, 8192,
                                           &layout) == -1 &&
                       errno == EBUSY);
                assert(hardware.writes == 8);
                hpclang_lcd_release(&bus, &state);
                assert(!state.owned && hardware.writes == 16);
                assert(memcmp(hardware.reg, original.reg, sizeof(hardware.reg)) == 0);
                hpclang_lcd_release(&bus, &state);
                assert(hardware.writes == 16);
            }
        }
    }
    struct hardware hardware = stock(80, 1);
    rejected(&hardware, 131, 80, 16, 0x08000000, 6799, ENOSPC);
    rejected(&hardware, 131, 80, 16, 0x083ffffc, 8192, ERANGE);
    rejected(&hardware, 131, 80, 16, 0x08400000 - 6800, 8192, ERANGE);
    rejected(&hardware, 131, 80, 16, 0x08000002, 8192, ERANGE);
    rejected(&hardware, 131, 80, 16, 0x80000000, 8192, ERANGE);
    rejected(&hardware, 131, 80, 8, 0x08000000, 8192, EINVAL);
    rejected(&hardware, 0, 80, 4, 0x08000000, 8192, EINVAL);
    rejected(&hardware, 161, 80, 4, 0x08000000, 8192, ENOTSUP);
    rejected(&hardware, 131, 86, 4, 0x08000000, 8192, ENOTSUP);
    hardware.reg[LCD_CONTROL1] |= 0x40; /* TFT must not be treated as STN. */
    rejected(&hardware, 131, 80, 4, 0x08000000, 8192, ENOTSUP);
    return 0;
}
