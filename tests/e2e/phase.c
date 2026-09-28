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
 * The reset wrapper reuses hpclang_e2e_result as the single payload state
 * channel: KBRD while the payload waits for the canonical sequence, KACK(n)
 * after a complete DOWN->UP transition has returned to the caller and the
 * caller has successfully continued to its next poll, followed by PASS/FAIL.
 * Delaying KACK until that next poll matters: an invalid UP never receives an
 * acknowledgement, so Stock cannot advance past a transition the calculator
 * test rejected. The 51st key needs no KACK because PASS/FAIL is the next state.
 */
#define HPCLANG_E2E_PHASE_KEYBOARD_READY UINT32_C(0x4b425244) /* KBRD */
#define HPCLANG_E2E_PHASE_KEYBOARD_ACK UINT32_C(0x4b410000)   /* KA + count */
#define HPCLANG_E2E_LAUNCH_RELEASE_TIMEOUT_US UINT64_C(2000000)

extern volatile uint32_t hpclang_e2e_result;

void __real_hpclang_keyboard_read(struct hpclang_keyboard_state *state);
void __real_hpclang_keyboard_events_reset(void);
int __real_hpclang_keyboard_event_poll(struct hpclang_keyboard_event *event);

static int first_keyboard_read = 1;
static int keyboard_pending_ready;
static uint32_t keyboard_ack_count;
static uint32_t keyboard_pending_ack;

void __wrap_hpclang_keyboard_read(struct hpclang_keyboard_state *state)
{
    if (!first_keyboard_read) {
        __real_hpclang_keyboard_read(state);
        return;
    }

    uint64_t deadline = hpclang_time_monotonic_us() + HPCLANG_E2E_LAUNCH_RELEASE_TIMEOUT_US;
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
    keyboard_pending_ack = 0u;
    keyboard_pending_ready = 1;
}

int __wrap_hpclang_keyboard_event_poll(struct hpclang_keyboard_event *event)
{
    /* The caller has drawn the initial keyboard screen before its first poll.
     * Publishing in reset raced the screen clear/draw with host capture. */
    if (keyboard_pending_ready) {
        hpclang_e2e_result = HPCLANG_E2E_PHASE_KEYBOARD_READY;
        keyboard_pending_ready = 0;
    }
    if (keyboard_pending_ack != 0u) {
        hpclang_e2e_result = HPCLANG_E2E_PHASE_KEYBOARD_ACK | (keyboard_pending_ack & 0xffffu);
        keyboard_pending_ack = 0u;
    }

    int available = __real_hpclang_keyboard_event_poll(event);
    if (available && event->type == HPCLANG_KEYBOARD_EVENT_UP) {
        ++keyboard_ack_count;
        keyboard_pending_ack = keyboard_ack_count;
    }
    return available;
}
