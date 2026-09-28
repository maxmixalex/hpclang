#ifndef HPCLANG_KEYBOARD_EVENT_ENGINE_H
#define HPCLANG_KEYBOARD_EVENT_ENGINE_H

#include <stdint.h>

#include <hpclang/keyboard.h>

struct hpclang_keyboard_event_engine
{
    uint64_t stable_bits;
    uint64_t candidate_bits;
    uint64_t candidate_since_us;
    uint64_t pending_down;
    uint64_t pending_up;
    uint64_t pending_timestamp_us;
    uint64_t repeat_due_us;
    uint32_t debounce_us;
    uint32_t repeat_delay_us;
    uint32_t repeat_interval_us;
    int repeat_scan_code;
    int initialized;
};

void hpclang_keyboard_event_engine_init(struct hpclang_keyboard_event_engine *engine, uint64_t bits,
                                        uint64_t now_us);

void hpclang_keyboard_event_engine_set_timing(struct hpclang_keyboard_event_engine *engine,
                                              uint32_t debounce_us, uint32_t repeat_delay_us,
                                              uint32_t repeat_interval_us);

int hpclang_keyboard_event_engine_step(struct hpclang_keyboard_event_engine *engine, uint64_t bits,
                                       uint64_t now_us, struct hpclang_keyboard_event *event);

#endif
