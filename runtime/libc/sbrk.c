#include <stdint.h>
#include <errno.h>
#include <hpclang/abi.h>

extern unsigned char __bss_end[];

static uintptr_t heap_next;

void *sbrk(intptr_t increment)
{
    const struct hpclang_boot_info *boot = hpclang_boot_info();

    const uintptr_t start = (uintptr_t)__bss_end;
    const uintptr_t current = heap_next ? heap_next : start;
    if (!boot || boot->rw_bytes < HPCLANG_STACK_MIN ||
        (uintptr_t)boot->rw_bytes - HPCLANG_STACK_MIN > UINTPTR_MAX - HPCLANG_BOOT_BASE)
        goto exhausted;
    const uintptr_t limit = HPCLANG_BOOT_BASE + ((uintptr_t)boot->rw_bytes - HPCLANG_STACK_MIN);
    if (start > limit || current < start || current > limit)
        goto exhausted;

    uintptr_t next;
    if (increment >= 0) {
        if ((uintptr_t)increment > limit - current)
            goto exhausted;
        next = current + (uintptr_t)increment;
    } else {
        /* Unsigned magnitude also handles INTPTR_MIN without signed overflow. */
        uintptr_t amount = (uintptr_t)0 - (uintptr_t)increment;
        if (amount > current - start)
            goto exhausted;
        next = current - amount;
    }
    void *result = (void *)current;
    heap_next = next;
    return result;

exhausted:
    errno = ENOMEM;
    return (void *)-1;
}
