#include <stdint.h>

#include <hpclang/abi.h>

#define REG32(address) (*(volatile uint32_t *)(uintptr_t)(address))
#define LCDSADDR1 REG32(0x07300014u)

#define WIDTH 131
#define HEIGHT 64
#define STRIDE 20

static const struct hpclang_boot_info *boot_info;

extern unsigned char __tls_base[];

void _set_tls(void *tls);
void __libc_init_array(void);
void __libc_fini_array(void);
int main(void);

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
    case 'E': return 0x79a7u;
    case 'P': return 0x6ba4u;
    case 'S': return 0x388eu;
    case 'T': return 0x7492u;
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
    case 'A': return 0x2bedu;
    case 'B': return 0x6baeu;
    case 'C': return 0x3923u;
    case 'D': return 0x6b6eu;
    case 'F': return 0x79a4u;
    default: return 0;
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

static void clear(volatile uint8_t *fb)
{
    for (int y = 0; y < HEIGHT; ++y) {
        for (int x = 0; x < 16; ++x)
            fb[y * STRIDE + x] = 0;
        fb[y * STRIDE + 16] &= 0xf8u;
    }
}

void hpclang_diag_step(uint32_t step)
{
    static const char digits[] = "0123456789ABCDEF";
    volatile uint8_t *fb = framebuffer();

    clear(fb);
    glyph(fb, 50, 28, 'S');
    glyph(fb, 54, 28, 'T');
    glyph(fb, 58, 28, 'E');
    glyph(fb, 62, 28, 'P');
    glyph(fb, 70, 28, digits[(step >> 4) & 15u]);
    glyph(fb, 74, 28, digits[step & 15u]);
}

int hpclang_start(const struct hpclang_boot_info *boot)
{
    boot_info = boot;

    hpclang_diag_step(0x01);
    _set_tls(__tls_base);
    hpclang_diag_step(0x02);

    hpclang_diag_step(0x03);
    __libc_init_array();
    hpclang_diag_step(0x04);

    int result = main();

    if (result == 0)
        hpclang_diag_step(0xf0);

    __libc_fini_array();

    if (result == 0)
        hpclang_diag_step(0xff);

    return result;
}

const struct hpclang_boot_info *hpclang_boot_info(void)
{
    return boot_info;
}
