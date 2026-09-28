#include <stdint.h>
#include <hpclang/abi.h>

extern unsigned char __bss_end[];

static uintptr_t heap_next;

void *sbrk(intptr_t increment)
{
    const struct hpclang_boot_info *boot = hpclang_boot_info();

    if (!heap_next)
        heap_next = (uintptr_t)__bss_end;

    uintptr_t next = heap_next + (uintptr_t)increment;
    uintptr_t limit = HPCLANG_BOOT_BASE + (uintptr_t)boot->rw_bytes - HPCLANG_STACK_MIN;

    if (next > limit)
        return (void *)-1;

    void *result = (void *)heap_next;
    heap_next = next;
    return result;
}
