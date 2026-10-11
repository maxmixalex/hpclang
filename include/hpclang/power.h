#ifndef HPCLANG_POWER_H
#define HPCLANG_POWER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /* Read the USB cable voltage-detect input: 0 absent, 1 present, -1/ENOSYS
     * for an unknown model. This does not report enumeration, battery charge,
     * or whether this model actually powers the calculator from USB.
     * No GPIO/controller state is changed. */
    int hpclang_power_usb_present(void);

    enum hpclang_power_profile
    {
        HPCLANG_POWER_HOST = 0,
        HPCLANG_POWER_ECONOMY = 1
    };

    /* Economy selects at most the 12 MHz crystal clock, retaining a slower
     * host setting. HOST restores the pre-economy clock and LCD divider.
     * PLL configuration and bus divisors are never changed. The LCD clock is
     * adjusted without increasing its scan rate; minimum divider constraints
     * may reduce refresh rate. Monotonic deadlines retain their origin.
     * Active sound rejects transitions with EBUSY. Runtime exit restores HOST.
     * All calls are application-thread operations, not interrupt-safe.
     * Lower clock frequency is observable; battery savings require hardware
     * measurement. Unsupported host LCD/clock configurations fail ENOTSUP. */
    int hpclang_power_set_profile(enum hpclang_power_profile profile);
    int hpclang_power_get_profile(void);
    int hpclang_power_cpu_frequency_hz(uint32_t *frequency);

#ifdef __cplusplus
}
#endif
#endif
