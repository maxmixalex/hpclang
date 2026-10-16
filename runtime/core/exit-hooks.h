#ifndef HPCLANG_EXIT_HOOKS_H
#define HPCLANG_EXIT_HOOKS_H

/* Optional services publish cleanup only when acquired. Keeping dispatch in a
 * separate translation unit lets the linker discard services an app never uses.
 * Power and RTC tick cleanup also run on _Exit; stdio is normal-exit-only. */
struct hpclang_exit_hooks
{
    void (*power)(void);
    void (*stdio)(void);
    void (*rtc_tick)(void);
};
extern struct hpclang_exit_hooks hpclang_exit_hooks;
void hpclang_rtc_tick_restore(void);

#endif
