#ifndef HPCLANG_LCD_ENGINE_H
#define HPCLANG_LCD_ENGINE_H

#include <stdint.h>

/* S3C2410 LCD register word offsets. Pending interrupt registers are deliberately
 * absent: they are write-one-to-clear status, not configuration to restore. */
enum hpclang_lcd_reg
{
    LCD_CONTROL1 = 0,
    LCD_CONTROL2 = 1,
    LCD_CONTROL3 = 2,
    LCD_CONTROL5 = 4,
    LCD_ADDRESS1 = 5,
    LCD_ADDRESS2 = 6,
    LCD_ADDRESS3 = 7,
    LCD_BLUE_LUT = 10,
    LCD_DITHER = 19
};

struct hpclang_lcd_bus
{
    uint32_t (*read)(void *context, enum hpclang_lcd_reg reg);
    void (*write)(void *context, enum hpclang_lcd_reg reg, uint32_t value);
    void *context;
};

struct hpclang_lcd_state
{
    uint32_t control1, control5, address1, address2, address3, blue_lut, dither;
    int owned;
};

struct hpclang_lcd_layout
{
    uint32_t stride, rows, bytes;
};

/* Internal stopped-controller configuration transaction.
 * Zero-initialize state before first acquisition.
 * The caller owns and has validated/published the entire physical buffer.
 * Calls require exclusive LCD ownership and serialization against interrupts
 * that could change LCD configuration. Pixel data must remain alive until
 * release has disabled DMA and restored the previous framebuffer.
 * Invalid geometry, mode, or buffer leaves state, layout and registers intact.
 * Timing, panel polarity, GPIO power and IRQ state are not changed. */
int hpclang_lcd_acquire(const struct hpclang_lcd_bus *bus, struct hpclang_lcd_state *state,
                        uint32_t width, uint32_t height, uint32_t levels, uint32_t physical,
                        uint32_t capacity, struct hpclang_lcd_layout *layout);
void hpclang_lcd_release(const struct hpclang_lcd_bus *bus, struct hpclang_lcd_state *state);

int hpclang_lcd_wait_frame(const struct hpclang_lcd_bus *bus, int (*clock)(uint64_t *),
                           uint32_t timeout_us);
int hpclang_lcd_buffer_validate(uint32_t physical, uint32_t bytes);

#endif
