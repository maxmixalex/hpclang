#include <stdint.h>

#include <hpclang/display.h>
#include <hpclang/font.h>

volatile uint32_t hpclang_e2e_result;
uint32_t hpclang_e2e_catalog(void);

int main(void)
{
    uint32_t failure = hpclang_e2e_catalog();
    struct hpclang_surface *surface = hpclang_display_surface();
    hpclang_surface_clear(surface, 0u);
    hpclang_surface_draw_text(surface, hpclang_font_mini(), 28, 28,
                              failure == 0u ? "CATALOG PASS" : "CATALOG FAIL", 255u);
    hpclang_e2e_result = failure == 0u ? UINT32_C(0x50415353) : UINT32_C(0x46410000) | failure;
    if (failure != 0u) {
        for (;;)
            __asm__ volatile("nop");
    }
    for (volatile uint32_t n = 12000000u; n != 0u; --n)
        __asm__ volatile("nop");
    return 0;
}
