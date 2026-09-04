#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <hpclang/abi.h>
#include <hpclang/target.h>

#define REG32(address) (*(volatile uint32_t *)(uintptr_t)(address))
#define LCDSADDR1 REG32(0x07300014u)

#define WIDTH  131
#define HEIGHT 64
#define STRIDE 20

void hpclang_diag_step(uint32_t step);

volatile uint32_t hpclang_e2e_result;
volatile uint32_t hpclang_initialized_probe = 0x13579bdfu;
volatile uint32_t hpclang_bss_probe;

static _Thread_local uint32_t hpclang_tls_initialized = 0x2468ace0u;
static _Thread_local uint32_t hpclang_tls_zero;

static volatile uint32_t hpclang_ctor_probe;

static volatile uint32_t hpclang_u32_a = 1000003u;
static volatile uint32_t hpclang_u32_b = 97u;
static volatile uint64_t hpclang_u64_a = UINT64_C(0x123456789abcdef0);
static volatile uint64_t hpclang_u64_b = UINT64_C(1000003);
static volatile double hpclang_double_a = 1.25;
static volatile double hpclang_double_b = 3.5;

__attribute__((constructor))
static void hpclang_runtime_ctor(void)
{
    hpclang_diag_step(0x31);
    hpclang_ctor_probe = 0xc0def00du;
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
    case 'B': return 0x6baeu;
    case 'C': return 0x3923u;
    case 'D': return 0x6b6eu;
    case 'E': return 0x79a7u;
    case 'F': return 0x79a4u;
    case 'G': return 0x396bu;
    case 'H': return 0x5bedu;
    case 'I': return 0x7497u;
    case 'J': return 0x126au;
    case 'K': return 0x5badu;
    case 'L': return 0x4927u;
    case 'M': return 0x5fedu;
    case 'N': return 0x5ffdu;
    case 'O': return 0x2b6au;
    case 'P': return 0x6ba4u;
    case 'Q': return 0x2b7bu;
    case 'R': return 0x6badu;
    case 'S': return 0x388eu;
    case 'T': return 0x7492u;
    case 'U': return 0x5b6fu;
    case 'V': return 0x5b6au;
    case 'W': return 0x5bfdu;
    case 'X': return 0x5aadu;
    case 'Y': return 0x5a92u;
    case 'Z': return 0x72a7u;
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

static void hex2(volatile uint8_t *fb, int x, int y, uint32_t value)
{
    static const char digits[] = "0123456789ABCDEF";

    glyph(fb, x, y, digits[(value >> 4) & 15u]);
    glyph(fb, x + 4, y, digits[value & 15u]);
}

static void clear(volatile uint8_t *fb)
{
    for (int y = 0; y < HEIGHT; ++y) {
        for (int x = 0; x < 16; ++x)
            fb[y * STRIDE + x] = 0;
        fb[y * STRIDE + 16] &= 0xf8u;
    }
}

static void hold_result(void)
{
    /*
     * Finite test-only hold so the LCD has time to scan the final pixels.
     * Bootstrap owns IRQ/FIQ isolation now, so this does not change runtime
     * state and always terminates.
     */
    for (volatile uint32_t n = 12000000u; n != 0; --n)
        __asm__ volatile("nop");
}

static int finish(uint32_t failure)
{
    volatile uint8_t *fb = framebuffer();

    clear(fb);
    if (failure == 0) {
        text(fb, 38, 28, "RUNTIME PASS");
        hpclang_e2e_result = 0x50415353u;
        hold_result();
        return 0;
    }

    text(fb, 30, 24, "RUNTIME FAIL");
    text(fb, 55, 36, "CODE");
    hex2(fb, 75, 36, failure);
    hpclang_e2e_result = 0x46410000u | failure;
    hold_result();
    return (int)failure;
}

int main(void)
{
    hpclang_diag_step(0x10);
    const struct hpclang_boot_info *boot = hpclang_boot_info();

    if ((boot->model != HPCLANG_MODEL_HP39GS &&
         boot->model != HPCLANG_MODEL_HP50G) ||
        boot->rw_bytes < 0x4000u)
        return finish(0x01);

    hpclang_diag_step(0x11);

    if (hpclang_initialized_probe != 0x13579bdfu)
        return finish(0x02);

    if (hpclang_bss_probe != 0u)
        return finish(0x03);

    hpclang_diag_step(0x12);

    if (hpclang_tls_initialized != 0x2468ace0u || hpclang_tls_zero != 0u)
        return finish(0x04);
    hpclang_tls_initialized ^= 0xffffffffu;
    hpclang_tls_zero = 0x11223344u;
    if (hpclang_tls_initialized != 0xdb97531fu ||
        hpclang_tls_zero != 0x11223344u)
        return finish(0x05);

    hpclang_diag_step(0x13);

    if (hpclang_ctor_probe != 0xc0def00du)
        return finish(0x06);

    hpclang_diag_step(0x14);
    uint32_t q32 = hpclang_u32_a / hpclang_u32_b;
    if (q32 != 10309u)
        return finish(0x07);

    hpclang_diag_step(0x15);
    uint64_t q64 = hpclang_u64_a / hpclang_u64_b;
    if (q64 != UINT64_C(1311764532170))
        return finish(0x08);

    hpclang_diag_step(0x16);
    volatile double mul = hpclang_double_a * hpclang_double_b;
    hpclang_diag_step(0x61);
    if (!(mul > 4.37 && mul < 4.38))
        return finish(0x09);

    volatile double fp = mul + 0.5;
    hpclang_diag_step(0x62);
    if (!(fp > 4.87))
        return finish(0x09);

    hpclang_diag_step(0x63);
    if (!(fp < 4.88))
        return finish(0x09);

    hpclang_diag_step(0x64);

    hpclang_diag_step(0x17);
    char src[] = "hpclang";
    char copy[16];
    memset(copy, 0, sizeof(copy));
    memcpy(copy, src, sizeof(src));
    if (strlen(copy) != 7u || strcmp(copy, "hpclang") != 0)
        return finish(0x0a);

    hpclang_diag_step(0x18);
    char *heap = malloc(24);
    if (heap == NULL)
        return finish(0x0b);
    memset(heap, 0, 24);
    memcpy(heap, copy, sizeof(src));
    if (strcmp(heap, "hpclang") != 0)
        return finish(0x0c);

    hpclang_diag_step(0x19);
    char formatted[64];
    int n = snprintf(
        formatted,
        sizeof(formatted),
        "%s:%lu:%llu",
        copy,
        (unsigned long)q32,
        (unsigned long long)q64);
    if (n <= 0 ||
        strcmp(formatted, "hpclang:10309:1311764532170") != 0)
        return finish(0x0d);

    hpclang_diag_step(0x1a);
    free(heap);
    hpclang_diag_step(0x1b);
    return finish(0);
}
