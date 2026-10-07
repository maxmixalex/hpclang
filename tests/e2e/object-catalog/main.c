#include <stdint.h>

#include <hpclang/abi.h>
#include <hpclang/display.h>
#include <hpclang/font.h>

volatile uint32_t hpclang_e2e_result;
uint32_t hpclang_e2e_catalog(void);
uint32_t hpclang_e2e_catalog_variables(void);
extern const struct hpclang_font hpclang_catalog_ui_font;
static const char pass_label[] = HPCLANG_UI_TEXT("CATALOG PASS");
static const char fail_label[] = HPCLANG_UI_TEXT("CATALOG FAIL");

int main(void)
{
    uint32_t failure = hpclang_e2e_catalog_variables();
    if (failure == 0u && hpclang_boot_info()->saturn_state != 0u)
        failure = hpclang_e2e_catalog();
    struct hpclang_surface *surface = hpclang_display_surface();
    hpclang_surface_clear(surface, 0u);
    hpclang_surface_draw_text(surface, &hpclang_catalog_ui_font, 28, 28,
                              failure == 0u ? pass_label : fail_label, 255u);
    hpclang_e2e_result = failure == 0u ? UINT32_C(0x50415353) : UINT32_C(0x46410000) | failure;
    if (failure != 0u) {
        for (;;)
            __asm__ volatile("nop");
    }
    for (volatile uint32_t n = 12000000u; n != 0u; --n)
        __asm__ volatile("nop");
    return 0;
}
