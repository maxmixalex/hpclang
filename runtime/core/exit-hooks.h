#ifndef HPCLANG_EXIT_HOOKS_H
#define HPCLANG_EXIT_HOOKS_H

/* Optional services publish cleanup only when acquired. Keeping dispatch in a
 * separate translation unit lets the linker discard services an app never uses.
 * Power cleanup also runs on _Exit; stdio cleanup is normal-exit-only. */
struct hpclang_exit_hooks
{
    void (*power)(void);
    void (*stdio)(void);
};
extern struct hpclang_exit_hooks hpclang_exit_hooks;

#endif
