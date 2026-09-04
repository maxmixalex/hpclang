#include <stdint.h>

/*
 * Clean runtime entry.
 *
 * This file is intentionally tiny while the calculator state-save/restore
 * layer is being reconstructed. It proves that writable C globals work through
 * stock LLVM RWPI without MMUglobals.o, libgcc, or an HPGCC crt0 object.
 */

struct hp39_launch_context {
    uintptr_t saturn_regs;
};

volatile uint32_t hpclang_runtime_probe;

int main(void);

int hpclang_start(void *saturn_regs)
{
    struct hp39_launch_context ctx = {
        .saturn_regs = (uintptr_t)saturn_regs,
    };

    hpclang_runtime_probe = (uint32_t)ctx.saturn_regs;

    /*
     * The production runtime will save/restore LCD, clock and interrupt state
     * here. Keep main() as the application boundary from the start.
     */
    return main();
}
