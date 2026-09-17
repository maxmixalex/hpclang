#ifndef HPCLANG_E2E_STOCK_RETURN_PHASE_H
#define HPCLANG_E2E_STOCK_RETURN_PHASE_H

#include <stdint.h>
#include <stdio.h>

#include "for_qemu.h"

#define HPCLANG_STOCK_STAGE_L1_SLOT_ADDRESS UINT32_C(0x07f00240)

static inline int hpclang_stock_debug_read32(
    CPUARMState *cpu, uint32_t address, uint32_t *value)
{
    uint8_t b[4];

    if (cpu_memory_rw_debug(cpu, address, b, sizeof(b), 0) != 0)
        return 0;

    *value = (uint32_t)b[0] |
             ((uint32_t)b[1] << 8) |
             ((uint32_t)b[2] << 16) |
             ((uint32_t)b[3] << 24);
    return 1;
}

static inline uint32_t hpclang_stock_physical_read32(uint32_t address)
{
    uint8_t b[4];

    cpu_physical_memory_read(address, b, sizeof(b));
    return (uint32_t)b[0] |
           ((uint32_t)b[1] << 8) |
           ((uint32_t)b[2] << 16) |
           ((uint32_t)b[3] << 24);
}

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
 *
 * Also inspect the physical coarse L2 table named by L1[0x90]. FSR 0x17 on the
 * ARM920T is a page translation fault, so recording the stage1 code descriptor
 * and the descriptor for the current data-fault page distinguishes corrupted
 * or unpublished L2 entries from a fault caused by some other MMU state.
 */
static inline uint32_t hpclang_stock_stage_l1_slot_probe(hdw_t *hdw)
{
    static unsigned probe_count;
    CPUARMState *cpu = hdw->cpu;

    ++probe_count;
    if (probe_count == 1u || (probe_count % 25u) == 0u) {
        uint32_t stage_l1 = 0;
        uint32_t l2_phys = 0;
        uint32_t l2_code = 0;
        uint32_t l2_workspace = 0;
        uint32_t l2_fault = 0;
        uint32_t fault_page = (cpu->cp15.c6_data >> 12) & UINT32_C(0xff);

        if (hpclang_stock_debug_read32(
                cpu, HPCLANG_STOCK_STAGE_L1_SLOT_ADDRESS, &stage_l1) &&
            (stage_l1 & UINT32_C(3)) == UINT32_C(1)) {
            l2_phys = stage_l1 & UINT32_C(0xfffffc00);
            l2_code = hpclang_stock_physical_read32(l2_phys);
            l2_workspace = hpclang_stock_physical_read32(l2_phys + 4u);
            l2_fault = hpclang_stock_physical_read32(
                l2_phys + fault_page * 4u);
        }

        fprintf(
            stderr,
            "stock launcher: HP48 ARM probe[%u] "
            "pc=0x%08x lr=0x%08x sp=0x%08x cpsr=0x%08x spsr=0x%08x "
            "exception=%d irq=0x%08x halted=%u "
            "ifsr=0x%08x ifar=0x%08x dfsr=0x%08x dfar=0x%08x "
            "stage_l1=0x%08x l2_phys=0x%08x l2[00]=0x%08x "
            "l2[01]=0x%08x l2[%02x]=0x%08x\n",
            probe_count,
            cpu->regs[15], cpu->regs[14], cpu->regs[13],
            cpsr_read(cpu), cpu->spsr,
            cpu->exception_index, cpu->interrupt_request, cpu->halted,
            cpu->cp15.c5_insn, cpu->cp15.c6_insn,
            cpu->cp15.c5_data, cpu->cp15.c6_data,
            stage_l1, l2_phys, l2_code, l2_workspace,
            fault_page, l2_fault);
    }

    return HPCLANG_STOCK_STAGE_L1_SLOT_ADDRESS;
}

/* This macro is used only inside hp48_wait_for_stage_teardown(), where `s` is
 * the local Stock harness state. Keep the probe coupled to that one read-only
 * phase witness rather than instrumenting the general memory-read helper. */
#define HPCLANG_STOCK_STAGE_L1_SLOT \
    hpclang_stock_stage_l1_slot_probe(s->hdw)
#define HPCLANG_STOCK_TEARDOWN_DEADLINE_US INT64_C(2000000)

#endif
