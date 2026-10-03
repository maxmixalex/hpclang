#ifndef HPCLANG_POWER_H
#define HPCLANG_POWER_H

#ifdef __cplusplus
extern "C"
{
#endif

    /* Read the USB cable voltage-detect input: 0 absent, 1 present, -1/ENOSYS
     * for an unknown model. This does not report enumeration, battery charge,
     * or whether this model actually powers the calculator from USB.
     * No GPIO/controller state is changed. */
    int hpclang_power_usb_present(void);

#ifdef __cplusplus
}
#endif
#endif
