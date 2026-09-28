#ifndef HPCLANG_ABI_H
#define HPCLANG_ABI_H

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

/*
 * hpclang v1 execution ABI.
 *
 * All supported ARM calculators expose the application through the same
 * virtual layout.  Calculator-specific launchers are responsible for building
 * the mappings and filling the boot page before entering _start.
 */
#define HPCLANG_CODE_BASE 0x01220000u
#define HPCLANG_BOOT_BASE 0x01100000u
#define HPCLANG_DATA_BASE 0x01101000u
#define HPCLANG_PAGE_SIZE 4096u
#define HPCLANG_STACK_MIN 8192u

    struct hpclang_image_header
    {
        uint8_t magic[4];        /* "HPC1" */
        uint32_t image_size;     /* initialized bytes mapped at CODE_BASE */
        uint32_t entry_offset;   /* entry relative to CODE_BASE */
        uint32_t rw_static_size; /* .data + .bss footprint at DATA_BASE */
    };

    struct hpclang_boot_info
    {
        uint32_t model;
        uint32_t rw_bytes;
        uintptr_t saturn_state;
        uintptr_t platform_cookie;
    };

    const struct hpclang_boot_info *hpclang_boot_info(void);

#ifdef __cplusplus
}
#endif

#endif
