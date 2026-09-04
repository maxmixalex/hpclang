#ifndef HPCLANG_LAUNCHER_V3_H
#define HPCLANG_LAUNCHER_V3_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define HPCLANG_L3_SIMPLE_MARKER      0x0000B34Cu
#define HPCLANG_L3_VIRTUAL_MARKER     0x0000A94Cu
#define HPCLANG_L3_LINKER_MARKER      0x3176B34Cu

/* Launcher V3 v1.2 memory-requirement word. */
#define HPCLANG_L3_MEM_BYTES_MASK     0x000FFFFFu
#define HPCLANG_L3_FLAG_S             (1u << 20)
#define HPCLANG_L3_FLAG_U             (1u << 21)
#define HPCLANG_L3_FLAG_M             (1u << 22)
#define HPCLANG_L3_FLAG_R             (1u << 23)
#define HPCLANG_L3_STACK_SHIFT        24u
#define HPCLANG_L3_STACK_MASK         (0xFu << HPCLANG_L3_STACK_SHIFT)
#define HPCLANG_L3_FLAG_E             (1u << 28)
#define HPCLANG_L3_FLAG_A             (1u << 29)
#define HPCLANG_L3_FLAG_V             (1u << 30)
#define HPCLANG_L3_FLAG_B             (1u << 31)

#define HPCLANG_L3_STACK_BLOCKS(n) \
    ((((uint32_t)(n)) & 0xFu) << HPCLANG_L3_STACK_SHIFT)

/*
 * Baseline hpclang common-launch mode: build an MMU page list, include extra
 * OS/KOS ARM RAM where the family launcher supports it, and reserve one
 * 16-byte calculator-stack block.
 */
#define HPCLANG_L3_DEFAULT_MR \
    (HPCLANG_L3_FLAG_M | HPCLANG_L3_FLAG_R | HPCLANG_L3_STACK_BLOCKS(1))

#ifdef __cplusplus
}
#endif

#endif
