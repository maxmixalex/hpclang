#include <stdint.h>
#include <stddef.h>

#include <hpclang/bootstrap.h>
#include <hpclang/device.h>
#include <hpclang/display.h>
#include <hpclang/font.h>
#include <hpclang/target.h>

volatile uint32_t hpclang_e2e_result;
uint32_t hpclang_e2e_classic_mutation(void);
uint32_t hpclang_e2e_hp39_mutation(void);

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
    hpclang_surface_draw_text(surface, hpclang_font_mini(), 28, 28,
                              failure == 0u ? "OBJECT PASS" : "OBJECT FAIL", 255u);

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
#if HPCLANG_E2E_HP39
    if (boot->model == HPCLANG_MODEL_HP39GPLUS || boot->model == HPCLANG_MODEL_HP39GS ||
        boot->model == HPCLANG_MODEL_HP40GS) {
        if (hpclang_device_capability(HPCLANG_CAPABILITY_APLET_VARIABLES) !=
            HPCLANG_CAPABILITY_SUPPORTED)
            return finish(0xe1u);
        return finish(hpclang_e2e_hp39_mutation());
    }
#endif
#if HPCLANG_E2E_CLASSIC
    if (boot->model == HPCLANG_MODEL_HP48GII || boot->model == HPCLANG_MODEL_HP49GPLUS ||
        boot->model == HPCLANG_MODEL_HP50G) {
        if (hpclang_device_capability(HPCLANG_CAPABILITY_RPL_STACK_MUTATION) !=
                HPCLANG_CAPABILITY_SUPPORTED ||
            hpclang_device_capability(HPCLANG_CAPABILITY_RPL_OBJECT_STORE_MUTATION) !=
                HPCLANG_CAPABILITY_SUPPORTED)
            return finish(0xe2u);
        return finish(hpclang_e2e_classic_mutation());
    }
#endif
    return finish(0xe0u);
}
