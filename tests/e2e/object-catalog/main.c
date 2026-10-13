#include <stdint.h>

#include <hpclang/abi.h>
#include <hpclang/display.h>
#include <hpclang/font.h>
#include <hpclang/power.h>

volatile uint32_t hpclang_e2e_result;
uint32_t hpclang_e2e_catalog(void);
uint32_t hpclang_e2e_catalog_variables(void);
uint32_t hpclang_e2e_port_discovery(void);
extern const struct hpclang_font hpclang_catalog_ui_font;
static const char pass_label[] = HPCLANG_UI_TEXT("CATALOG PASS");
static const char fail_label[] = HPCLANG_UI_TEXT("CATALOG FAIL");

/* Retain adjacent backslashes in both Aplet nibble alignments. Stock's file
 * import must preserve these bytes independently of incidental machine code. */
static const volatile uint8_t transfer_octets[] = {0x5c, 0x5c, 0, 0xc0, 0xc5, 0x05, 0x0d, 0x0a};

int main(void)
{
    /* Capability discovery belongs with metadata on HP39. Keep the separate
     * mutation/lifecycle payload's scarce ARM arena for its large snapshots. */
    uint32_t failure = hpclang_e2e_port_discovery();
    if (!failure &&
        (transfer_octets[0] != 0x5c || transfer_octets[1] != 0x5c || transfer_octets[2] != 0 ||
         transfer_octets[3] != 0xc0 || transfer_octets[4] != 0xc5 || transfer_octets[5] != 0x05 ||
         transfer_octets[6] != 0x0d || transfer_octets[7] != 0x0a))
        failure = 0x3e4u;
    if (!failure)
        failure = hpclang_e2e_catalog_variables();
    if (failure == 0u && hpclang_boot_info()->saturn_state != 0u) {
        if (hpclang_power_set_profile(HPCLANG_POWER_ECONOMY))
            failure = 0x3e1u;
        else
            failure = hpclang_e2e_catalog();
        uint32_t frequency;
        if (!failure && (hpclang_power_cpu_frequency_hz(&frequency) || frequency > 12000000u))
            failure = 0x3e2u;
        if (hpclang_power_set_profile(HPCLANG_POWER_HOST) && !failure)
            failure = 0x3e3u;
    }
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
