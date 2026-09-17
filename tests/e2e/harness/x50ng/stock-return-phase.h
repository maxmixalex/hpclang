#ifndef HPCLANG_E2E_STOCK_RETURN_PHASE_H
#define HPCLANG_E2E_STOCK_RETURN_PHASE_H

#include <stdint.h>
#include <stdio.h>

#include "for_qemu.h"

/*
 * ARMToolbox stage0 owns L1[0x90] at this stock-mapped address while its
 * temporary 0x09000000 alias is live. The slot is cleared only after stage1
 * and the common bootstrap have returned to stage0, immediately before the
 * final KOS-release / launcher-return tail. This is therefore a stronger
 * HP48gII teardown witness than disappearance of hpclang_e2e_result, which
 * happens earlier when the common bootstrap restores the stock MMU.
 *
 * While this witness is under investigation, sample the emulator's ARM state
 * without mutating it. The stock launcher polls every 20 ms; emit the first
 * sample and then every 25 polls (~0.5 s) so a teardown timeout records whether
 * the core is in IRQ/SVC/abort state, halted, or simply stuck at one PC.
 */
static inline uint32_t hpclang_stock_stage_l1_slot_probe(hdw_t *hdw)
{
    static unsigned probe_count;
    CPUARMState *cpu = hdw->cpu;

    ++probe_count;
    if (probe_count == 1u || (probe_count % 25u) == 0u) {
        fprintf(
            stderr,
            "stock launcher: HP48 ARM probe[%u] "
            "pc=0x%08x lr=0x%08x sp=0x%08x cpsr=0x%08x spsr=0x%08x "
            "exception=%d irq=0x%08x halted=%u "
            "ifsr=0x%08x ifar=0x%08x dfsr=0x%08x dfar=0x%08x\n",
            probe_count,
            cpu->regs[15], cpu->regs[14], cpu->regs[13],
            cpsr_read(cpu), cpu->spsr,
            cpu->exception_index, cpu->interrupt_request, cpu->halted,
            cpu->cp15.c5_insn, cpu->cp15.c6_insn,
            cpu->cp15.c5_data, cpu->cp15.c6_data);
    }

    return UINT32_C(0x07f00240);
}

/* This macro is used only inside hp48_wait_for_stage_teardown(), where `s` is
 * the local Stock harness state. Keep the probe coupled to that one read-only
 * phase witness rather than instrumenting the general memory-read helper. */
#define HPCLANG_STOCK_STAGE_L1_SLOT \
    hpclang_stock_stage_l1_slot_probe(s->hdw)
#define HPCLANG_STOCK_TEARDOWN_DEADLINE_US INT64_C(2000000)

#endif
