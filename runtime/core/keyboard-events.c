#include <stdint.h>

#include <hpclang/device.h>
#include <hpclang/keyboard.h>
#include <hpclang/time.h>

#include "keyboard-event-engine.h"
#include "keyboard-key-map.h"

static struct hpclang_keyboard_event_engine event_engine;
static uint32_t debounce_us = 20000u;
static uint32_t repeat_delay_us = 500000u;
static uint32_t repeat_interval_us = 80000u;

void hpclang_keyboard_events_reset(void)
{
    struct hpclang_keyboard_state state;
    hpclang_keyboard_read(&state);
    hpclang_keyboard_event_engine_init(
        &event_engine,
        state.bits,
        hpclang_time_monotonic_us());
    hpclang_keyboard_event_engine_set_timing(
        &event_engine,
        debounce_us,
        repeat_delay_us,
        repeat_interval_us);
}

void hpclang_keyboard_events_set_timing(
    uint32_t new_debounce_us,
    uint32_t new_repeat_delay_us,
    uint32_t new_repeat_interval_us)
{
    debounce_us = new_debounce_us;
    repeat_delay_us = new_repeat_delay_us;
    repeat_interval_us = new_repeat_interval_us;

    if (event_engine.initialized)
        hpclang_keyboard_event_engine_set_timing(
            &event_engine,
            debounce_us,
            repeat_delay_us,
            repeat_interval_us);
}

int hpclang_keyboard_event_poll(struct hpclang_keyboard_event *event)
{
    struct hpclang_keyboard_state state;
    hpclang_keyboard_read(&state);

    uint64_t now_us = hpclang_time_monotonic_us();
    if (!event_engine.initialized) {
        hpclang_keyboard_event_engine_init(&event_engine, state.bits, now_us);
        hpclang_keyboard_event_engine_set_timing(
            &event_engine,
            debounce_us,
            repeat_delay_us,
            repeat_interval_us);
        return 0;
    }

    if (!hpclang_keyboard_event_engine_step(
            &event_engine,
            state.bits,
            now_us,
            event))
        return 0;

    uint32_t model = hpclang_device_info()->model;
    event->key = hpclang_keyboard_key_from_scan(model, event->scan_code);
    event->modifiers = hpclang_keyboard_modifiers_from_bits(
        model,
        event_engine.stable_bits);
    return 1;
}
