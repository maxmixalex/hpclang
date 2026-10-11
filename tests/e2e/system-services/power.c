#include <errno.h>
#include <stdint.h>
#include <stddef.h>
#include <hpclang/device.h>
#include <hpclang/display.h>
#include <hpclang/power.h>
#include <hpclang/sound.h>
#include <hpclang/time.h>
#include <hpclang/timer.h>

#define REG(address) (*(volatile uint32_t *)(uintptr_t)(address))

static int reject_clock_change(void *context)
{
    int *checked = context;
    *checked = hpclang_power_set_profile(HPCLANG_POWER_HOST) == -1 && errno == EBUSY &&
               hpclang_power_get_profile() == HPCLANG_POWER_ECONOMY;
    return 1;
}

unsigned hpclang_e2e_power_test(void)
{
    uint32_t original_hz, current_hz;
    if (hpclang_device_capability(HPCLANG_CAPABILITY_POWER_PROFILE) !=
            HPCLANG_CAPABILITY_SUPPORTED ||
        hpclang_power_cpu_frequency_hz(&original_hz) || !original_hz ||
        hpclang_power_get_profile() != HPCLANG_POWER_HOST)
        return 1;
    if (hpclang_power_set_profile((enum hpclang_power_profile)99) != -1 || errno != EINVAL ||
        hpclang_power_cpu_frequency_hz(NULL) != -1 || errno != EINVAL)
        return 2;
    const uintptr_t registers[] = {0x07200004u, 0x07200010u, 0x07200014u, 0x07600000u,
                                   0x07600004u, 0x07600008u, 0x0760000cu};
    uint32_t before[sizeof registers / sizeof registers[0]];
    for (unsigned i = 0; i < sizeof before / sizeof before[0]; ++i)
        before[i] = REG(registers[i]);
    uint32_t lcd = REG(0x07300000u) & 0x3ffffu;
    unsigned failure = 3;
    struct hpclang_timer deadline = {0};
    uint64_t start, end, expirations;
    if (hpclang_time_read_us(&start) || hpclang_timer_start(&deadline, 500000, 0) ||
        hpclang_power_set_profile(HPCLANG_POWER_ECONOMY))
        goto done;
    failure = 4;
    if (hpclang_power_get_profile() != HPCLANG_POWER_ECONOMY ||
        hpclang_power_cpu_frequency_hz(&current_hz) || current_hz > 12000000u ||
        current_hz > original_hz || hpclang_power_set_profile(HPCLANG_POWER_ECONOMY))
        goto done;
    hpclang_delay_us(20000);
    if (hpclang_time_read_us(&end) || end < start + 20000 || end - start > 250000 ||
        hpclang_timer_poll(&deadline, &expirations) || expirations != 0)
        goto done;
    failure = 5;
    /* A display acquired at low speed must later restore the host divider,
     * even if speed is restored before the display session ends. */
    if (hpclang_display_begin(4) || hpclang_display_present(200000) ||
        hpclang_power_set_profile(HPCLANG_POWER_HOST))
        goto done;
    hpclang_display_end();
    failure = 6;
    if (hpclang_display_begin(16) || hpclang_power_set_profile(HPCLANG_POWER_ECONOMY))
        goto done;
    hpclang_display_end();
    int sound_checked = 0;
    failure = 7;
    if (hpclang_sound_play(1000, 20, reject_clock_change, &sound_checked) != 1 || !sound_checked ||
        hpclang_power_set_profile(HPCLANG_POWER_HOST))
        goto done;
    failure = 8;
    if (hpclang_power_cpu_frequency_hz(&current_hz) || current_hz != original_hz ||
        hpclang_power_get_profile() != HPCLANG_POWER_HOST || (REG(0x07300000u) & 0x3ffffu) != lcd)
        goto done;
    for (unsigned i = 0; i < sizeof before / sizeof before[0]; ++i)
        if (REG(registers[i]) != before[i])
            goto done;
    failure = 9;
    if (hpclang_time_read_us(&end) || end < start || end - start > 400000)
        goto done;
    /* Timer start samples time after 'start'; wait for its actual deadline. */
    if (end < deadline.deadline_us)
        hpclang_delay_us(deadline.deadline_us - end);
    if (hpclang_timer_poll(&deadline, &expirations) || expirations != 1)
        goto done;
    failure = 0;
done:
    hpclang_timer_cancel(&deadline);
    hpclang_display_end();
    (void)hpclang_power_set_profile(HPCLANG_POWER_HOST);
    return failure;
}
