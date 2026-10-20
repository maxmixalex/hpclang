#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <hpclang/abi.h>

unsigned char __bss_end[1];
static struct hpclang_boot_info boot;
static int available;

const struct hpclang_boot_info *hpclang_boot_info(void)
{
    return available ? &boot : 0;
}

/* Exercise the libc implementation with host addresses, without replacing the
 * host allocator's sbrk or dereferencing any simulated heap address. */
#undef HPCLANG_BOOT_BASE
#define HPCLANG_BOOT_BASE ((uintptr_t)__bss_end - 4096u)
#define sbrk fixture_sbrk
#include "../../runtime/libc/sbrk.c"
#undef sbrk

static void rejected(intptr_t increment, uintptr_t expected)
{
    errno = 0;
    assert(fixture_sbrk(increment) == (void *)-1);
    assert(errno == ENOMEM);
    assert((uintptr_t)fixture_sbrk(0) == expected);
}

int main(void)
{
    const uintptr_t start = (uintptr_t)__bss_end;
    assert(fixture_sbrk(0) == (void *)-1 && errno == ENOMEM);
    available = 1;
    boot.rw_bytes = HPCLANG_STACK_MIN - 1;
    assert(fixture_sbrk(0) == (void *)-1 && errno == ENOMEM);
    boot.rw_bytes = HPCLANG_STACK_MIN + 4095;
    assert(fixture_sbrk(0) == (void *)-1 && errno == ENOMEM);
    boot.rw_bytes = HPCLANG_STACK_MIN + 4096 + 256;
    assert((uintptr_t)fixture_sbrk(0) == start);
    rejected(-1, start);
    rejected(INTPTR_MIN, start);
    rejected(INTPTR_MAX, start);
    assert((uintptr_t)fixture_sbrk(128) == start);
    rejected(-129, start + 128);
    assert((uintptr_t)fixture_sbrk(-128) == start + 128);
    assert((uintptr_t)fixture_sbrk(256) == start);
    rejected(1, start + 256);
    rejected(INTPTR_MAX, start + 256);
    rejected(INTPTR_MIN, start + 256);
    assert((uintptr_t)fixture_sbrk(-256) == start + 256);
    assert((uintptr_t)fixture_sbrk(0) == start);
    return 0;
}
