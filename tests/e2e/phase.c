#include <stdint.h>

#include <hpclang/keyboard.h>

/*
 * Machine-readable synchronization point shared by Direct and Stock E2E.
 * The test-only linker wrapper flips this marker immediately before the
 * payload resets the public keyboard event queue and starts waiting for the
 * canonical 51-key sequence. Host harnesses can therefore inject keys only
 * when the payload is ready, independent of launcher/transport latency.
 */
#define HPCLANG_E2E_PHASE_KEYBOARD_READY UINT32_C(0x4b425244) /* KBRD */

volatile uint32_t hpclang_e2e_phase;

void __real_hpclang_keyboard_events_reset(void);

void __wrap_hpclang_keyboard_events_reset(void)
{
    hpclang_e2e_phase = HPCLANG_E2E_PHASE_KEYBOARD_READY;
    __real_hpclang_keyboard_events_reset();
}
