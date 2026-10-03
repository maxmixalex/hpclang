#include <stdint.h>
#include <hpclang/display.h>
#include <hpclang/font.h>

extern const struct hpclang_font hpclang_system_font;
static const char pass_label[] = HPCLANG_UI_TEXT("SYSTEM PASS");
static const char fail_label[] = HPCLANG_UI_TEXT("SYSTEM FAIL");

volatile uint32_t hpclang_e2e_result;
uint32_t hpclang_e2e_system_test(void);

int main(void)
{
    uint32_t failure = hpclang_e2e_system_test();
    struct hpclang_surface *surface = hpclang_display_surface();
    hpclang_surface_clear(surface, 0u);
    hpclang_surface_draw_text_n(surface, &hpclang_system_font, 28, 28,
                                failure ? fail_label : pass_label,
                                failure ? sizeof(fail_label) - 1 : sizeof(pass_label) - 1, 255u);
    hpclang_e2e_result = failure ? UINT32_C(0x46410000) | failure : UINT32_C(0x50415353);
    if (failure)
        for (;;)
            __asm__ volatile("nop");
    for (volatile uint32_t n = 12000000u; n != 0u; --n)
        __asm__ volatile("nop");
    return 0;
}
