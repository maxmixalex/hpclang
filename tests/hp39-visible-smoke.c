#include <stdint.h>

#include <hpclang/abi.h>

#define REG32(address) (*(volatile uint32_t *)(uintptr_t)(address))

#define INTMSK    REG32(0x07100008u)
#define LCDSADDR1 REG32(0x07300014u)
#define GPGCON    REG32(0x07A00060u)
#define GPGDAT    REG32(0x07A00064u)

#define WIDTH  131
#define HEIGHT 64
#define STRIDE 20

volatile uint32_t hpclang_e2e_result;

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

/* Exact HP39gs APLET-key matrix mapping, already hardware-validated in 39gsX. */
static int aplet_pressed(void)
{
    GPGCON = (1u << 30) | 0xAAA9u;
    delay(100);

    int pressed = (GPGDAT & (1u << 1)) == 0;

    GPGCON = 0x5555AAA9u;
    delay(100);
    return pressed;
}

int main(void)
{
    const struct hpclang_boot_info *boot = hpclang_boot_info();

    uint32_t old_cpsr;
    uint32_t saved_intmsk = INTMSK;
    uint32_t saved_gpgcon = GPGCON;
    uint32_t saved_gpgdat = GPGDAT;

    __asm__ volatile("mrs %0, cpsr" : "=r"(old_cpsr));
    uint32_t masked = old_cpsr | 0xc0u;
    __asm__ volatile("msr cpsr_c, %0" : : "r"(masked) : "memory");
    INTMSK = 0xffffffffu;

    volatile uint8_t *fb = framebuffer();
    clear(fb);

    text(fb, 20, 12, "HPC1 PASS");
    text(fb, 8, 28, "MODEL");
    hex8(fb, 36, 28, boot->model);
    text(fb, 8, 38, "RW");
    hex8(fb, 20, 38, boot->rw_bytes);
    text(fb, 8, 52, "APLET TO EXIT");

    hpclang_e2e_result = 0x50415353u;

    while (aplet_pressed())
        delay(1000);
    while (!aplet_pressed())
        delay(1000);
    while (aplet_pressed())
        delay(1000);

    GPGCON = saved_gpgcon;
    GPGDAT = saved_gpgdat;
    INTMSK = saved_intmsk;
    __asm__ volatile("msr cpsr_c, %0" : : "r"(old_cpsr) : "memory");

    return 0;
}
