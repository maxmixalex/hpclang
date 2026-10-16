#include <errno.h>
#include <stdlib.h>
#include <hpclang/device.h>
#include <hpclang/display.h>
#include <hpclang/font.h>
#include <hpclang/time.h>

extern const struct hpclang_font hpclang_system_font;
unsigned hpclang_hpg_graphics_witness(struct hpclang_surface *destination);
unsigned hpclang_historical_graphics(int (*checkpoint)(unsigned));
unsigned hpclang_hpg_adapter(int (*checkpoint)(unsigned));
volatile uint32_t hpclang_e2e_result;
volatile unsigned hpclang_display_failure;
static uint32_t baseline[9];
static const unsigned offsets[] = {0, 1, 2, 4, 5, 6, 7, 10, 19};

__attribute__((destructor)) static void skipped_on_immediate_exit(void)
{
    hpclang_e2e_result = 0x46410072u;
    for (;;)
        __asm__ volatile("nop");
}

static uint32_t reg(unsigned i)
{
    uint32_t value = ((volatile uint32_t *)0x07300000)[offsets[i]];
    return i == 0 ? value & 0x3ffffu : i == 3 ? value & 0x1fffu : value;
}
static int restored(void)
{
    for (unsigned i = 0; i < 9; ++i)
        if (baseline[i] != reg(i))
            return 0;
    return 1;
}
static int frame(unsigned id)
{
    hpclang_e2e_result = 0x46520000u | id;
    uint64_t start = hpclang_time_monotonic_us();
    while (hpclang_e2e_result != (0x464b0000u | id))
        if (hpclang_time_monotonic_us() - start > 2000000u)
            return -1;
    return 0;
}
static void bars(unsigned levels, int reverse)
{
    struct hpclang_surface *surface = hpclang_display_surface();
    unsigned width = hpclang_surface_width(surface), height = hpclang_surface_height(surface);
    for (unsigned y = 0; y < height; ++y)
        for (unsigned x = 0; x < width; ++x) {
            unsigned color = x * levels / width;
            if (reverse)
                color = levels - 1 - color;
            hpclang_surface_set_pixel(surface, x, y, (uint8_t)(color * 255 / (levels - 1)));
        }
    for (unsigned i = 0; i < 6; ++i)
        hpclang_display_indicator_set(i, ((i + (unsigned)reverse) & 1u) ? 255 : 0);
}
static unsigned checks(void)
{
    for (unsigned i = 0; i < 9; ++i)
        baseline[i] = reg(i);
    if (hpclang_display_begin(3) != -1 || errno != EINVAL || !restored())
        return 1;
    if (hpclang_display_present(100000) != -1 || errno != EPERM)
        return 2;
    /* Exercise actual allocator exhaustion, then recovery, without keeping
     * any reservation in the SDK or reducing the launcher's stack guard. */
    void *blocks[256];
    unsigned count = 0;
    while (count < 256 && (blocks[count] = malloc(4096)) != NULL)
        ++count;
    int exhausted = hpclang_display_begin(16);
    int error = errno;
    while (count)
        free(blocks[--count]);
    if (exhausted != -1 || error != ENOMEM || !restored())
        return 3;
    const unsigned modes[] = {2, 4, 16};
    for (unsigned m = 0; m < 3; ++m) {
        unsigned levels = modes[m];
        if (hpclang_display_begin(levels))
            return 10 + m;
        if (hpclang_display_begin(levels) != -1 || errno != EBUSY)
            return 20 + m;
        if (hpclang_surface_levels(hpclang_display_surface()) != levels)
            return 30 + m;
        for (unsigned reverse = 0; reverse < 2; ++reverse) {
            bars(levels, (int)reverse);
            if (hpclang_display_present(200000))
                return 40 + m;
            if (frame(levels * 2 + reverse))
                return 50 + m;
        }
        uint32_t front = reg(4);
        struct hpclang_surface *back = hpclang_display_surface();
        if (hpclang_display_set_enabled(0, 200000) || (reg(0) & 1u) ||
            hpclang_display_present(200000) != -1 || errno != ENODEV || reg(4) != front ||
            hpclang_display_surface() != back)
            return 70 + m;
        if (hpclang_display_set_enabled(1, 200000) || !(reg(0) & 1u) ||
            hpclang_display_wait_frame(200000) || reg(4) != front)
            return 80 + m;
        /* Cleanup must also recover the host when the application exits dark. */
        if (hpclang_display_set_enabled(0, 200000))
            return 90 + m;
        hpclang_display_end();
        hpclang_display_end();
        if (!restored())
            return 60 + m;
    }
    return 0;
}
int main(void)
{
    hpclang_e2e_result = 0x4f530001;
    unsigned failure = checks();
    if (!failure && atexit(skipped_on_immediate_exit))
        failure = 72;
    if (!failure) {
        unsigned graphics = hpclang_hpg_graphics_witness(NULL);
        if (graphics)
            failure = 80u + graphics;
    }
    hpclang_display_end();
    if (!failure) {
        unsigned historical = hpclang_historical_graphics(frame);
        if (historical)
            failure = 100u + historical;
        else if (!restored())
            failure = 104u;
    }
    if (!failure) {
        unsigned adapter = hpclang_hpg_adapter(frame);
        if (adapter)
            failure = 120u + adapter;
        else if (!restored())
            failure = 126u;
    }
    if (!failure && hpclang_display_begin(16))
        failure = 70;
    struct hpclang_surface *surface = hpclang_display_surface();
    hpclang_surface_clear(surface, 0);
    hpclang_surface_draw_text(
        surface, &hpclang_system_font, 25, 28,
        failure ? HPCLANG_UI_TEXT("DISPLAY FAIL") : HPCLANG_UI_TEXT("DISPLAY PASS"), 255);
    if (!failure && hpclang_display_present(200000))
        failure = 71;
    hpclang_display_failure = failure;
    hpclang_e2e_result = failure ? 0x46410000u | failure : 0x50415353;
    if (failure)
        for (;;)
            __asm__ volatile("nop");
    hpclang_delay_us(100000);
    /* Deliberately leave the last session active: runtime shutdown must restore
     * Stock before releasing its allocations and returning to the launcher. */
    /* _Exit skips both callbacks and static finalizers, but must still release
     * hpclang's borrowed display before returning through the launcher. */
    _Exit(0);
}
