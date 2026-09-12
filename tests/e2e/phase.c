#include <stdint.h>

#include <hpclang/keyboard.h>
#include <hpclang/time.h>

/*
 * Machine-readable synchronization shared by Direct and Stock E2E.
 *
 * Stock launchers use a calculator key (for example PrRUN/F1) to enter HPC1.
 * That launch key can still be physically down when the payload performs its
 * first raw keyboard read. It belongs to launcher choreography, not to the
 * 51-key payload test, so the test-only wrapper waits for the initial keyboard
 * state to become idle before the event engine is reset. A genuinely stuck key
 * still reaches the normal E2E failure path after the bounded wait.
 *
 * The reset wrapper then reuses hpclang_e2e_result as the single payload state
 * channel: KBRD while the payload is waiting for the canonical 51-key sequence,
 * KACK(n) after each complete DOWN->UP transition, followed by PASS/FAIL.
 * Direct may still inject its deterministic sequence eagerly; Stock uses the
 * acknowledgements to serialize host-realtime injection against calculator
 * Timer0 so the two clocks cannot outrun each other.
 */
#define HPCLANG_E2E_PHASE_KEYBOARD_READY UINT32_C(0x4b425244) /* KBRD */
#define HPCLANG_E2E_PHASE_KEYBOARD_ACK UINT32_C(0x4b410000)   /* KA + count */
#define HPCLANG_E2E_LAUNCH_RELEASE_TIMEOUT_US UINT64_C(2000000)

extern volatile uint32_t hpclang_e2e_result;

void __real_hpclang_keyboard_read(struct hpclang_keyboard_state *state);
void __real_hpclang_keyboard_events_reset(void);
int __real_hpclang_keyboard_event_poll(struct hpclang_keyboard_event *event);

static int first_keyboard_read = 1;
static uint32_t keyboard_ack_count;

void __wrap_hpclang_keyboard_read(struct hpclang_keyboard_state *state)
{
    if (!first_keyboard_read) {
        __real_hpclang_keyboard_read(state);
        return;
    }

    uint64_t deadline = hpclang_time_monotonic_us() +
        HPCLANG_E2E_LAUNCH_RELEASE_TIMEOUT_US;
    do {
        __real_hpclang_keyboard_read(state);
        if (!hpclang_keyboard_any_is_down(state))
            break;
    } while (hpclang_time_monotonic_us() < deadline);

    first_keyboard_read = 0;
}

void __wrap_hpclang_keyboard_events_reset(void)
{
    __real_hpclang_keyboard_events_reset();
    keyboard_ack_count = 0u;
    hpclang_e2e_result = HPCLANG_E2E_PHASE_KEYBOARD_READY;
}

int __wrap_hpclang_keyboard_event_poll(struct hpclang_keyboard_event *event)
{
    int available = __real_hpclang_keyboard_event_poll(event);
    if (available && event->type == HPCLANG_KEYBOARD_EVENT_UP) {
        ++keyboard_ack_count;
        hpclang_e2e_result =
            HPCLANG_E2E_PHASE_KEYBOARD_ACK | (keyboard_ack_count & 0xffffu);
    }
    return available;
}
