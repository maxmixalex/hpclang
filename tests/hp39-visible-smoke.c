#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <hpclang/abi.h>
#include <hpclang/device.h>
#include <hpclang/display.h>
#include <hpclang/keyboard.h>
#include <hpclang/target.h>
#include <hpclang/time.h>

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

static void glyph(struct hpclang_surface *surface, int x, int y, char c)
{
    uint32_t rows = glyph_bits(c);

    for (int yy = 0; yy < 5; ++yy) {
        uint32_t row = (rows >> ((4 - yy) * 3)) & 7u;
        for (int xx = 0; xx < 3; ++xx)
            if (row & (1u << (2 - xx)))
                hpclang_surface_set_pixel(surface, x + xx, y + yy, 255u);
    }
}

static void text(struct hpclang_surface *surface, int x, int y, const char *s)
{
    while (*s) {
        if (*s != ' ')
            glyph(surface, x, y, *s);
        x += 4;
        ++s;
    }
}

static void hex2(struct hpclang_surface *surface, int x, int y, uint32_t value)
{
    static const char digits[] = "0123456789ABCDEF";

    glyph(surface, x, y, digits[(value >> 4) & 15u]);
    glyph(surface, x + 4, y, digits[value & 15u]);
}

static int display_height(uint32_t model)
{
    return model == HPCLANG_MODEL_HP49GPLUS ||
           model == HPCLANG_MODEL_HP50G
        ? 80
        : 64;
}

#ifdef HPCLANG_E2E_WAIT_FOR_ENTER
static int enter_pressed(void)
{
    /*
     * The interactive HP49g+/50g validation package uses ENTER, which is
     * matrix column 0 / row 6 on that family. Reuse the public raw scanner
     * instead of carrying a second GPIO implementation in the test payload.
     */
    struct hpclang_keyboard_state state;
    hpclang_keyboard_read(&state);
    return hpclang_keyboard_matrix_is_down(&state, 0u, 6u);
}
#endif

static void hold_result(void)
{
#ifdef HPCLANG_E2E_WAIT_FOR_ENTER
    /*
     * Keep the interactive HP50g result visible until ENTER. Automated stock
     * E2E stops the emulator as soon as it observes PASS, so it does not inject
     * a keyboard interrupt into the launcher return path.
     */
    while (enter_pressed())
        ;
    while (!enter_pressed())
        ;
    while (enter_pressed())
        ;
#else
    /*
     * Existing HP39/40 and canonical gates intentionally remain automatic:
     * they validate return/state restoration independently of keyboard GPIO.
     */
    for (volatile uint32_t n = 12000000u; n != 0; --n)
        __asm__ volatile("nop");
#endif
}

static int finish(uint32_t failure, uint32_t model)
{
    (void)model;
    struct hpclang_surface *surface = hpclang_display_surface();

    hpclang_surface_clear(surface, 0u);
    if (failure == 0) {
        text(surface, 38, 28, "RUNTIME PASS");
#ifdef HPCLANG_E2E_WAIT_FOR_ENTER
        text(surface, 39, 40, "ENTER TO EXIT");
#endif
        hpclang_e2e_result = 0x50415353u;
        hold_result();
        return 0;
    }

    text(surface, 30, 24, "RUNTIME FAIL");
    text(surface, 55, 36, "CODE");
    hex2(surface, 75, 36, failure);
#ifdef HPCLANG_E2E_WAIT_FOR_ENTER
    text(surface, 39, 48, "ENTER TO EXIT");
#endif
    hpclang_e2e_result = 0x46410000u | failure;
    hold_result();
    return (int)failure;
}

static int supported_model(uint32_t model)
{
    return model >= HPCLANG_MODEL_HP39GPLUS &&
           model <= HPCLANG_MODEL_HP50G;
}

static uint32_t expected_family(uint32_t model)
{
    if (model >= HPCLANG_MODEL_HP39GPLUS && model <= HPCLANG_MODEL_HP40GS)
        return HPCLANG_FAMILY_HP39;
    if (model == HPCLANG_MODEL_HP48GII)
        return HPCLANG_FAMILY_HP48;
    if (model == HPCLANG_MODEL_HP49GPLUS || model == HPCLANG_MODEL_HP50G)
        return HPCLANG_FAMILY_HP4950;
    return HPCLANG_FAMILY_UNKNOWN;
}

#ifdef HPCLANG_E2E_KEYBOARD
static int keyboard_transition(void)
{
    struct hpclang_keyboard_state raw;
    hpclang_keyboard_read(&raw);
    if (hpclang_keyboard_any_is_down(&raw))
        return 0;

    hpclang_keyboard_events_reset();

    struct hpclang_keyboard_event event;
    int down_scan = -1;
    uint64_t deadline = hpclang_time_monotonic_us() + UINT64_C(3000000);

    while (hpclang_time_monotonic_us() < deadline) {
        if (!hpclang_keyboard_event_poll(&event))
            continue;
        if (event.type == HPCLANG_KEYBOARD_EVENT_DOWN &&
            event.scan_code < 60u) {
            down_scan = (int)event.scan_code;
            break;
        }
    }

    if (down_scan < 0)
        return 0;

    deadline = hpclang_time_monotonic_us() + UINT64_C(3000000);
    while (hpclang_time_monotonic_us() < deadline) {
        if (!hpclang_keyboard_event_poll(&event))
            continue;
        if (event.type == HPCLANG_KEYBOARD_EVENT_UP &&
            event.scan_code == (uint8_t)down_scan)
            return 1;
    }

    return 0;
}
#endif

int main(void)
{
    hpclang_diag_step(0x10);
    const struct hpclang_boot_info *boot = hpclang_boot_info();

    if (!supported_model(boot->model) || boot->rw_bytes < 0x4000u)
        return finish(0x01, boot->model);

    const struct hpclang_device_info *device = hpclang_device_info();
    if (device == NULL ||
        device->version != HPCLANG_DEVICE_INFO_VERSION ||
        device->size < sizeof(*device) ||
        device->model != boot->model ||
        device->family != expected_family(boot->model) ||
        device->display_width != 131u ||
        device->display_height != (uint32_t)display_height(boot->model))
        return finish(0x0e, boot->model);

    struct hpclang_surface *surface = hpclang_display_surface();
    if (hpclang_surface_width(surface) != device->display_width ||
        hpclang_surface_height(surface) != device->display_height ||
        hpclang_surface_levels(surface) != 2u)
        return finish(0x11, boot->model);

    uint64_t time_before = hpclang_time_monotonic_us();
    hpclang_delay_us(3000u);
    uint64_t time_after = hpclang_time_monotonic_us();
    if (time_after < time_before ||
        time_after - time_before < 3000u ||
        time_after - time_before > 200000u)
        return finish(0x0f, boot->model);

#ifdef HPCLANG_E2E_KEYBOARD
    if (!keyboard_transition())
        return finish(0x10, boot->model);
#endif

    hpclang_diag_step(0x11);

    if (hpclang_initialized_probe != 0x13579bdfu)
        return finish(0x02, boot->model);

    if (hpclang_bss_probe != 0u)
        return finish(0x03, boot->model);

    hpclang_diag_step(0x12);

    if (hpclang_tls_initialized != 0x2468ace0u || hpclang_tls_zero != 0u)
        return finish(0x04, boot->model);
    hpclang_tls_initialized ^= 0xffffffffu;
    hpclang_tls_zero = 0x11223344u;
    if (hpclang_tls_initialized != 0xdb97531fu ||
        hpclang_tls_zero != 0x11223344u)
        return finish(0x05, boot->model);

    hpclang_diag_step(0x13);

    if (hpclang_ctor_probe != 0xc0def00du)
        return finish(0x06, boot->model);

    hpclang_diag_step(0x14);
    uint32_t q32 = hpclang_u32_a / hpclang_u32_b;
    if (q32 != 10309u)
        return finish(0x07, boot->model);

    hpclang_diag_step(0x15);
    uint64_t q64 = hpclang_u64_a / hpclang_u64_b;
    if (q64 != UINT64_C(1311764532170))
        return finish(0x08, boot->model);

    hpclang_diag_step(0x16);
    volatile double mul = hpclang_double_a * hpclang_double_b;
    hpclang_diag_step(0x61);
    if (!(mul > 4.37 && mul < 4.38))
        return finish(0x09, boot->model);

    volatile double fp = mul + 0.5;
    hpclang_diag_step(0x62);
    if (!(fp > 4.87))
        return finish(0x09, boot->model);

    hpclang_diag_step(0x63);
    if (!(fp < 4.88))
        return finish(0x09, boot->model);

    hpclang_diag_step(0x64);

    hpclang_diag_step(0x17);
    char src[] = "hpclang";
    char copy[16];
    memset(copy, 0, sizeof(copy));
    memcpy(copy, src, sizeof(src));
    if (strlen(copy) != 7u || strcmp(copy, "hpclang") != 0)
        return finish(0x0a, boot->model);

    hpclang_diag_step(0x18);
    char *heap = malloc(24);
    if (heap == NULL)
        return finish(0x0b, boot->model);
    memset(heap, 0, 24);
    memcpy(heap, copy, sizeof(src));
    if (strcmp(heap, "hpclang") != 0)
        return finish(0x0c, boot->model);

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
        return finish(0x0d, boot->model);

    hpclang_diag_step(0x1a);
    free(heap);
    hpclang_diag_step(0x1b);
    return finish(0, boot->model);
}
