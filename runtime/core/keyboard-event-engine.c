#include "keyboard-event-engine.h"

#define RAW_KEY_COUNT 60u
#define MATRIX_KEY_COUNT 56u

static int first_set(uint64_t bits)
{
    for (unsigned bit = 0u; bit < RAW_KEY_COUNT; ++bit)
        if ((bits & (UINT64_C(1) << bit)) != 0u)
            return (int)bit;
    return -1;
}

static uint32_t modifiers(uint64_t bits)
{
    uint32_t value = 0u;
    if ((bits & (UINT64_C(1) << HPCLANG_KEYBOARD_ON)) != 0u)
        value |= HPCLANG_KEYBOARD_MOD_ON;
    if ((bits & (UINT64_C(1) << HPCLANG_KEYBOARD_ALPHA)) != 0u)
        value |= HPCLANG_KEYBOARD_MOD_ALPHA;
    if ((bits & (UINT64_C(1) << HPCLANG_KEYBOARD_LEFT_SHIFT)) != 0u)
        value |= HPCLANG_KEYBOARD_MOD_LEFT_SHIFT;
    if ((bits & (UINT64_C(1) << HPCLANG_KEYBOARD_RIGHT_SHIFT)) != 0u)
        value |= HPCLANG_KEYBOARD_MOD_RIGHT_SHIFT;
    return value;
}

static void fill_event(
    struct hpclang_keyboard_event *event,
    uint64_t timestamp_us,
    uint64_t state_bits,
    unsigned type,
    unsigned scan_code)
{
    event->timestamp_us = timestamp_us;
    event->modifiers = modifiers(state_bits);
    event->type = (uint8_t)type;
    event->scan_code = (uint8_t)scan_code;
    event->key = HPCLANG_KEY_UNKNOWN;
}

void hpclang_keyboard_event_engine_init(
    struct hpclang_keyboard_event_engine *engine,
    uint64_t bits,
    uint64_t now_us)
{
    engine->stable_bits = bits;
    engine->candidate_bits = bits;
    engine->candidate_since_us = now_us;
    engine->pending_down = 0u;
    engine->pending_up = 0u;
    engine->pending_timestamp_us = now_us;
    engine->repeat_due_us = 0u;
    engine->debounce_us = 20000u;
    engine->repeat_delay_us = 500000u;
    engine->repeat_interval_us = 80000u;
    engine->repeat_scan_code = -1;
    engine->initialized = 1;
}

void hpclang_keyboard_event_engine_set_timing(
    struct hpclang_keyboard_event_engine *engine,
    uint32_t debounce_us,
    uint32_t repeat_delay_us,
    uint32_t repeat_interval_us)
{
    engine->debounce_us = debounce_us;
    engine->repeat_delay_us = repeat_delay_us;
    engine->repeat_interval_us = repeat_interval_us;
    engine->repeat_scan_code = -1;
    engine->repeat_due_us = 0u;
}

static int emit_pending(
    struct hpclang_keyboard_event_engine *engine,
    struct hpclang_keyboard_event *event)
{
    int scan_code = first_set(engine->pending_up);
    if (scan_code >= 0) {
        engine->pending_up &= ~(UINT64_C(1) << (unsigned)scan_code);
        if (engine->repeat_scan_code == scan_code) {
            engine->repeat_scan_code = -1;
            engine->repeat_due_us = 0u;
        }
        fill_event(
            event,
            engine->pending_timestamp_us,
            engine->stable_bits,
            HPCLANG_KEYBOARD_EVENT_UP,
            (unsigned)scan_code);
        return 1;
    }

    scan_code = first_set(engine->pending_down);
    if (scan_code >= 0) {
        engine->pending_down &= ~(UINT64_C(1) << (unsigned)scan_code);
        if ((unsigned)scan_code < MATRIX_KEY_COUNT) {
            engine->repeat_scan_code = scan_code;
            engine->repeat_due_us =
                engine->pending_timestamp_us + engine->repeat_delay_us;
        }
        fill_event(
            event,
            engine->pending_timestamp_us,
            engine->stable_bits,
            HPCLANG_KEYBOARD_EVENT_DOWN,
            (unsigned)scan_code);
        return 1;
    }

    return 0;
}

int hpclang_keyboard_event_engine_step(
    struct hpclang_keyboard_event_engine *engine,
    uint64_t bits,
    uint64_t now_us,
    struct hpclang_keyboard_event *event)
{
    if (!engine->initialized) {
        hpclang_keyboard_event_engine_init(engine, bits, now_us);
        return 0;
    }

    if (emit_pending(engine, event))
        return 1;

    if (bits != engine->candidate_bits) {
        engine->candidate_bits = bits;
        engine->candidate_since_us = now_us;
        return 0;
    }

    if (bits != engine->stable_bits) {
        if (now_us - engine->candidate_since_us < engine->debounce_us)
            return 0;

        uint64_t old_bits = engine->stable_bits;
        engine->stable_bits = bits;
        engine->pending_up = old_bits & ~bits;
        engine->pending_down = bits & ~old_bits;
        engine->pending_timestamp_us = now_us;

        if (engine->repeat_scan_code >= 0 &&
            (bits & (UINT64_C(1) << (unsigned)engine->repeat_scan_code)) == 0u) {
            engine->repeat_scan_code = -1;
            engine->repeat_due_us = 0u;
        }

        if (emit_pending(engine, event))
            return 1;
    }

    if (engine->repeat_scan_code >= 0 &&
        engine->repeat_interval_us != 0u &&
        now_us >= engine->repeat_due_us) {
        unsigned scan_code = (unsigned)engine->repeat_scan_code;
        fill_event(
            event,
            now_us,
            engine->stable_bits,
            HPCLANG_KEYBOARD_EVENT_REPEAT,
            scan_code);
        engine->repeat_due_us = now_us + engine->repeat_interval_us;
        return 1;
    }

    return 0;
}
