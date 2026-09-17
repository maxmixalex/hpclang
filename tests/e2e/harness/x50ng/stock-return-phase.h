#ifndef HPCLANG_E2E_STOCK_RETURN_PHASE_H
#define HPCLANG_E2E_STOCK_RETURN_PHASE_H

#include <stdint.h>

/*
 * ARMToolbox stage0 owns L1[0x90] at this stock-mapped address while its
 * temporary 0x09000000 alias is live. The slot is cleared only after stage1
 * and the common bootstrap have returned to stage0, immediately before the
 * final KOS-release / launcher-return tail. This is therefore a stronger
 * HP48gII teardown witness than disappearance of hpclang_e2e_result, which
 * happens earlier when the common bootstrap restores the stock MMU.
 */
#define HPCLANG_STOCK_STAGE_L1_SLOT UINT32_C(0x07f00240)
#define HPCLANG_STOCK_TEARDOWN_DEADLINE_US INT64_C(2000000)

#endif
