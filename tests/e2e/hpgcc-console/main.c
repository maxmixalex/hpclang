#include <stdint.h>
#include <hpclang/time.h>
#include <hpclang/display.h>

volatile uint32_t hpclang_e2e_result;
unsigned hpclang_hpg_console(int (*checkpoint)(unsigned));

static int frame(unsigned id)
{
    hpclang_e2e_result = 0x46520000u | id;
    uint64_t start = hpclang_time_monotonic_us();
    while (hpclang_e2e_result != (0x464b0000u | id))
        if (hpclang_time_monotonic_us() - start > 2000000u)
            return -1;
    return 0;
}

int main(void)
{
    const unsigned offsets[] = {0, 1, 2, 4, 5, 6, 7, 10, 19};
    volatile uint32_t *lcd = (volatile uint32_t *)0x07300000;
    uint32_t saved[9];
    for (unsigned i = 0; i < 9; ++i)
        saved[i] = lcd[offsets[i]];
    unsigned failure = hpclang_hpg_console(frame);
    for (unsigned i = 0; i < 9; ++i) {
        uint32_t mask = i == 0 ? 0x3ffffu : i == 3 ? 0x1fffu : UINT32_MAX;
        if (!failure && (saved[i] & mask) != (lcd[offsets[i]] & mask))
            failure = 8;
    }
    /* Leave a distinct application frame until runtime shutdown. The return
     * oracle must observe a transition back to Stock, not an already-restored
     * frame captured after the explicit cleanup check above. */
    if (!failure && hpclang_display_begin(2))
        failure = 9;
    if (!failure) {
        hpclang_surface_clear(hpclang_display_surface(), 0);
        hpclang_surface_fill_rect(hpclang_display_surface(), 20, 20, 90, 24, 255);
        if (hpclang_display_present(200000))
            failure = 10;
    }
    hpclang_e2e_result = failure ? 0x46410000u | failure : 0x50415353u;
    if (failure)
        for (;;)
            __asm__ volatile("nop");
    hpclang_delay_us(100000);
    return 0;
}
