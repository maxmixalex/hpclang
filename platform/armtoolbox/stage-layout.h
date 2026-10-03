#ifndef HPCLANG_ARMTOOLBOX_STAGE_LAYOUT_H
#define HPCLANG_ARMTOOLBOX_STAGE_LAYOUT_H

/* Private stage mapping, adjacent to HPC1's data/code slots. Stock HP39g+
 * interrupts use the byte at 0x07f00240, so L1[0x90] cannot be borrowed while
 * calling Stock. Keep C, assembly, the linker and emulator observers aligned. */
#define HPCLANG_STAGE_WINDOW_BASE 0x01300000
#define HPCLANG_STAGE1_BASE (HPCLANG_STAGE_WINDOW_BASE + 0x400)
#define HPCLANG_STAGE_L1_SLOT (0x07f00000 + ((HPCLANG_STAGE_WINDOW_BASE >> 20) * 4))

#endif
