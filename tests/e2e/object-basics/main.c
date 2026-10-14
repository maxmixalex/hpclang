#include <stdint.h>
#include <stddef.h>

#include <hpclang/bootstrap.h>
#include <hpclang/device.h>
#include <hpclang/display.h>
#include <hpclang/font.h>
#include <hpclang/target.h>

volatile uint32_t hpclang_e2e_result;
uint32_t hpclang_e2e_object_stack(uint32_t model);
uint32_t hpclang_e2e_object_store(uint32_t model);
int hpclang_e2e_saturn_compat_test(void);
int hpclang_e2e_object_store_owners(void);
extern const struct hpclang_font hpclang_object_basics_ui_font;
static const char pass_label[] = HPCLANG_UI_TEXT("OBJECT PASS");
static const char fail_label[] = HPCLANG_UI_TEXT("OBJECT FAIL");

static void hold_success(void)
{
    for (volatile uint32_t n = 12000000u; n != 0u; --n)
        __asm__ volatile("nop");
}

static void hold_failure(void)
{
    for (;;)
        __asm__ volatile("nop");
}

static int finish(uint32_t failure)
{
    struct hpclang_surface *surface = hpclang_display_surface();
    hpclang_surface_clear(surface, 0u);
    hpclang_surface_draw_text(surface, &hpclang_object_basics_ui_font, 28, 28,
                              failure == 0u ? pass_label : fail_label, 255u);

    if (failure == 0u) {
        hpclang_e2e_result = UINT32_C(0x50415353);
        hold_success();
        return 0;
    }

    hpclang_e2e_result = UINT32_C(0x46410000) | failure;
    hold_failure();
    return (int)failure;
}

int main(void)
{
    const struct hpclang_boot_info *boot = hpclang_boot_info();
    if (boot == NULL)
        return finish(0xe0u);
    if (hpclang_e2e_saturn_compat_test() != 0)
        return finish(0x12u);
    uint32_t failure = hpclang_e2e_object_stack(boot->model);
    if (failure != 0u)
        return finish(failure);
    failure = hpclang_e2e_object_store(boot->model);
    if (failure != 0u)
        return finish(failure);
    return finish(hpclang_e2e_object_store_owners() ? 0xd0u : 0u);
}
