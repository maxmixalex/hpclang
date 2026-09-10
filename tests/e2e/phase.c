#include <stdint.h>

#include <hpclang/keyboard.h>

/*
 * Machine-readable synchronization point shared by Direct and Stock E2E.
 * Reuse hpclang_e2e_result as the single payload state channel: KBRD while
 * the payload is waiting for the canonical 51-key sequence, then PASS/FAIL.
 * The linker wrapper flips KBRD immediately after the public keyboard event
 * queue has been reset, so host injection is independent of launcher latency.
 */
#define HPCLANG_E2E_PHASE_KEYBOARD_READY UINT32_C(0x4b425244) /* KBRD */

extern volatile uint32_t hpclang_e2e_result;

void __real_hpclang_keyboard_events_reset(void);

void __wrap_hpclang_keyboard_events_reset(void)
{
    __real_hpclang_keyboard_events_reset();
    hpclang_e2e_result = HPCLANG_E2E_PHASE_KEYBOARD_READY;
}
