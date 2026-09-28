#include <assert.h>
#include <stdint.h>
#include <hpclang/keyboard.h>

volatile uint32_t hpclang_e2e_result;
static int next_event;
void __wrap_hpclang_keyboard_events_reset(void);
int __wrap_hpclang_keyboard_event_poll(struct hpclang_keyboard_event *event);
void __real_hpclang_keyboard_events_reset(void)
{
}
void __real_hpclang_keyboard_read(struct hpclang_keyboard_state *state)
{
    state->bits = 0;
}
int hpclang_keyboard_any_is_down(const struct hpclang_keyboard_state *state)
{
    return state->bits != 0;
}
uint64_t hpclang_time_monotonic_us(void)
{
    return 0;
}
int __real_hpclang_keyboard_event_poll(struct hpclang_keyboard_event *event)
{
    if (!next_event)
        return 0;
    event->type = next_event;
    next_event = 0;
    return 1;
}
int main(void)
{
    struct hpclang_keyboard_event event;
    hpclang_e2e_result = 0;
    __wrap_hpclang_keyboard_events_reset();
    /* The caller may still be clearing/drawing its initial keyboard frame. */
    assert(hpclang_e2e_result == 0);
    assert(!__wrap_hpclang_keyboard_event_poll(&event));
    assert(hpclang_e2e_result == UINT32_C(0x4b425244));
    next_event = HPCLANG_KEYBOARD_EVENT_UP;
    assert(__wrap_hpclang_keyboard_event_poll(&event));
    /* An UP is acknowledged only after caller validation and redraw. */
    assert(hpclang_e2e_result == UINT32_C(0x4b425244));
    assert(!__wrap_hpclang_keyboard_event_poll(&event));
    assert(hpclang_e2e_result == UINT32_C(0x4b410001));
    return 0;
}
