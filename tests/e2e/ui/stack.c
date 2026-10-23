#include <hpclang/abi.h>
#include <stdint.h>
#include <hpclang/time.h>

/* Test-only watermark: includes instrumentation overhead and reports touched
 * stack words, not the maximum SP excursion of untouched local storage. */
#define STACK_PATTERN UINT32_C(0xa55a96c3)
volatile uint32_t hpclang_ui_stack_witness[3];
static uintptr_t stack_bottom, stack_top;
extern volatile uint32_t hpclang_e2e_result;
extern volatile uint32_t hpclang_ui_heap_witness[2];
extern volatile uint32_t hpclang_ui_update_witness[4];
static volatile uint32_t report[10];
static volatile uint32_t updates[7];

uint32_t hpclang_ui_stack_sample(void)
{
    volatile const uint32_t *cursor = (volatile const uint32_t *)stack_bottom;
    while ((uintptr_t)cursor < stack_top && *cursor == STACK_PATTERN)
        ++cursor;
    return (uint32_t)(stack_top - (uintptr_t)cursor);
}

int __real_hpclang_start(const struct hpclang_boot_info *boot);
int __wrap_hpclang_start(const struct hpclang_boot_info *boot)
{
    uintptr_t current_sp;
    __asm__ volatile("mov %0, sp" : "=r"(current_sp));
    stack_top = HPCLANG_BOOT_BASE + boot->rw_bytes;
    stack_bottom = stack_top - HPCLANG_STACK_MIN;
    /* _start has already cleared BSS and saved its caller registers. Never
     * overwrite that frame or this wrapper's own live local storage. */
    for (volatile uint32_t *p = (volatile uint32_t *)stack_bottom; (uintptr_t)p < current_sp; ++p)
        *p = STACK_PATTERN;
    hpclang_ui_stack_witness[0] = (uint32_t)(stack_top - current_sp);
    int status = __real_hpclang_start(boot);
    hpclang_ui_stack_witness[2] = hpclang_ui_stack_sample();
    /* Observe after libc finalizers and runtime resource cleanup, before the
     * launcher removes the application's mappings. Observation overhead after
     * this sample belongs to the test, not the application's measured work. */
    report[0] = 1;
    report[1] = 8;
    report[2] = 1;
    report[3] = hpclang_ui_stack_witness[0];
    report[4] = hpclang_ui_stack_witness[1];
    report[5] = hpclang_ui_stack_witness[2];
    report[6] = HPCLANG_STACK_MIN;
    report[7] = hpclang_ui_heap_witness[0];
    report[8] = hpclang_ui_heap_witness[1];
    report[9] = boot->rw_bytes;
    uint32_t offset = (uint32_t)((uintptr_t)report - HPCLANG_BOOT_BASE);
    hpclang_e2e_result = 0x4d100000u | offset;
    uint64_t start = hpclang_time_monotonic_us();
    while (hpclang_e2e_result != (0x4d200000u | offset)) {
        if (hpclang_time_monotonic_us() - start > 2000000u) {
            hpclang_e2e_result = 0x4641190du;
            for (;;)
                __asm__ volatile("nop");
        }
    }
    /* Keep performance observations separate from the stable memory schema.
     * Frame/input timers exclude host handshakes; reporting is not timed. */
    updates[0] = 2;
    updates[1] = 5;
    updates[2] = 1;
    for (unsigned i = 0; i < 4; ++i)
        updates[3 + i] = hpclang_ui_update_witness[i];
    offset = (uint32_t)((uintptr_t)updates - HPCLANG_BOOT_BASE);
    hpclang_e2e_result = 0x4d100000u | offset;
    start = hpclang_time_monotonic_us();
    while (hpclang_e2e_result != (0x4d200000u | offset)) {
        if (hpclang_time_monotonic_us() - start > 2000000u) {
            hpclang_e2e_result = 0x4641190eu;
            for (;;)
                __asm__ volatile("nop");
        }
    }
    return status;
}
