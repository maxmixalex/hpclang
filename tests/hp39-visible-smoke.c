#include <stdint.h>

#include <hpclang/abi.h>

#define REG32(address) (*(volatile uint32_t *)(uintptr_t)(address))

#define INTMSK    REG32(0x07100008u)
#define LCDSADDR1 REG32(0x07300014u)

#define WIDTH  131
#define HEIGHT 64
#define STRIDE 20

volatile uint32_t hpclang_e2e_result;

uint32_t hp39_irq_save_disable(void);
void hp39_irq_restore(uint32_t cpsr);

static void delay(uint32_t n)
{
    while (n--)
        __asm__ volatile("nop");
}

static volatile uint8_t *framebuffer(void)
{
    return (volatile uint8_t *)(uintptr_t)(
        ((LCDSADDR1 & 0xffffu) * 2u) + 0x07f00000u);
}

static void pixel(volatile uint8_t *fb, int x, int y)
{
    if ((unsigned)x >= WIDTH || (unsigned)y >= HEIGHT)
        return;

    volatile uint8_t *p = fb + y * STRIDE + (x >> 3);
    *p |= (uint8_t)(1u << (x & 7));
}

static uint32_t glyph_bits(char c)
{
    switch (c) {
    case 'A': return 0x2bedu;
    case 'C': return 0x3923u;
    case 'D': return 0x6b6eu;
    case 'E': return 0x79a7u;
    case 'H': return 0x5bedu;
    case 'I': return 0x7497u;
    case 'L': return 0x4927u;
    case 'M': return 0x5fedu;
    case 'O': return 0x2b6au;
    case 'P': return 0x6ba4u;
    case 'R': return 0x6badu;
    case 'S': return 0x388eu;
    case 'T': return 0x7492u;
    case 'W': return 0x5bfdu;
    case 'X': return 0x5aadu;
    case '0': return 0x7b6fu;
    case '1': return 0x2c97u;
    case '2': return 0x62a7u;
    case '3': return 0x628eu;
    case '4': return 0x5bc9u;
    case '5': return 0x798eu;
    case '6': return 0x39aau;
    case '7': return 0x7292u;
    case '8': return 0x2aaau;
    case '9': return 0x2aceu;
    default:  return 0;
    }
}

static void glyph(volatile uint8_t *fb, int x, int y, char c)
{
    uint32_t rows = glyph_bits(c);

    for (int yy = 0; yy < 5; ++yy) {
        uint32_t row = (rows >> ((4 - yy) * 3)) & 7u;
        for (int xx = 0; xx < 3; ++xx)
            if (row & (1u << (2 - xx)))
                pixel(fb, x + xx, y + yy);
    }
}

static void text(volatile uint8_t *fb, int x, int y, const char *s)
{
    while (*s) {
        if (*s != ' ')
            glyph(fb, x, y, *s);
        x += 4;
        ++s;
    }
}

static void hex8(volatile uint8_t *fb, int x, int y, uint32_t value)
{
    static const char digits[] = "0123456789ABCDEF";

    for (int shift = 28; shift >= 0; shift -= 4) {
        glyph(fb, x, y, digits[(value >> shift) & 15u]);
        x += 4;
    }
}

static void clear(volatile uint8_t *fb)
{
    for (int y = 0; y < HEIGHT; ++y) {
        for (int x = 0; x < 16; ++x)
            fb[y * STRIDE + x] = 0;
        fb[y * STRIDE + 16] &= 0xf8u;
    }
}

int main(void)
{
    const struct hpclang_boot_info *boot = hpclang_boot_info();

    uint32_t old_cpsr = hp39_irq_save_disable();
    uint32_t saved_intmsk = INTMSK;
    INTMSK = 0xffffffffu;

    volatile uint8_t *fb = framebuffer();
    clear(fb);

    text(fb, 20, 12, "HPC1 PASS");
    text(fb, 8, 28, "MODEL");
    hex8(fb, 36, 28, boot->model);
    text(fb, 8, 38, "RW");
    hex8(fb, 20, 38, boot->rw_bytes);
    text(fb, 8, 52, "AUTO EXIT");

    hpclang_e2e_result = 0x50415353u;

    /*
     * This gate validates return/state restoration, not the keyboard backend.
     * Keep the result visible for a short human-observable interval, then
     * restore interrupt state and return through the common bootstrap.
     */
    delay(60000000u);

    INTMSK = saved_intmsk;
    hp39_irq_restore(old_cpsr);

    return 0;
}
