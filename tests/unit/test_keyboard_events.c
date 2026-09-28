#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <hpclang/keyboard.h>

#include "keyboard-event-engine.h"

#define BIT(n) (UINT64_C(1) << (n))

static void expect_none(struct hpclang_keyboard_event_engine *engine, uint64_t bits, uint64_t now)
{
    struct hpclang_keyboard_event event;
    if (hpclang_keyboard_event_engine_step(engine, bits, now, &event)) {
        fprintf(stderr, "unexpected event type=%u scan=%u at %llu\n", event.type, event.scan_code,
                (unsigned long long)now);
        exit(EXIT_FAILURE);
    }
}

static struct hpclang_keyboard_event expect_event(struct hpclang_keyboard_event_engine *engine,
                                                  uint64_t bits, uint64_t now, unsigned type,
                                                  unsigned scan)
{
    struct hpclang_keyboard_event event;
    if (!hpclang_keyboard_event_engine_step(engine, bits, now, &event) || event.type != type ||
        event.scan_code != scan) {
        fprintf(stderr, "missing event type=%u scan=%u at %llu\n", type, scan,
                (unsigned long long)now);
        exit(EXIT_FAILURE);
    }
    return event;
}

int main(void)
{
    struct hpclang_keyboard_event_engine engine = {0};

    /* Initial physical state is a baseline, not a synthetic key press. */
    expect_none(&engine, 0u, 0u);

    /* Press bounce does not escape until one snapshot is stable for 20 ms. */
    expect_none(&engine, BIT(7), 1000u);
    expect_none(&engine, 0u, 5000u);
    expect_none(&engine, BIT(7), 7000u);
    expect_none(&engine, BIT(7), 26000u);
    struct hpclang_keyboard_event down =
        expect_event(&engine, BIT(7), 27000u, HPCLANG_KEYBOARD_EVENT_DOWN, 7u);
    if (down.timestamp_us != 27000u || down.modifiers != 0u)
        return EXIT_FAILURE;

    /* No repeat before the initial delay; one repeat per poll afterwards. */
    expect_none(&engine, BIT(7), 526999u);
    expect_event(&engine, BIT(7), 527000u, HPCLANG_KEYBOARD_EVENT_REPEAT, 7u);
    expect_none(&engine, BIT(7), 606999u);
    expect_event(&engine, BIT(7), 607000u, HPCLANG_KEYBOARD_EVENT_REPEAT, 7u);

    /* Dedicated inputs participate in DOWN/UP ordering but never repeat. */
    uint64_t shifted = BIT(7) | BIT(HPCLANG_KEYBOARD_GPF5);
    expect_none(&engine, shifted, 700000u);
    expect_event(&engine, shifted, 720000u, HPCLANG_KEYBOARD_EVENT_DOWN, HPCLANG_KEYBOARD_GPF5);

    uint64_t chord = shifted | BIT(8);
    expect_none(&engine, chord, 800000u);
    expect_event(&engine, chord, 820000u, HPCLANG_KEYBOARD_EVENT_DOWN, 8u);

    /* The most recently pressed matrix key becomes the repeat source. */
    expect_none(&engine, chord, 1319999u);
    expect_event(&engine, chord, 1320000u, HPCLANG_KEYBOARD_EVENT_REPEAT, 8u);

    /* Leave only the dedicated input held; it must never autorepeat. */
    uint64_t shift_only = BIT(HPCLANG_KEYBOARD_GPF5);
    expect_none(&engine, shift_only, 1400000u);
    expect_event(&engine, shift_only, 1420000u, HPCLANG_KEYBOARD_EVENT_UP, 7u);
    expect_event(&engine, shift_only, 1420000u, HPCLANG_KEYBOARD_EVENT_UP, 8u);
    expect_none(&engine, shift_only, 2000000u);

    expect_none(&engine, 0u, 2100000u);
    expect_event(&engine, 0u, 2120000u, HPCLANG_KEYBOARD_EVENT_UP, HPCLANG_KEYBOARD_GPF5);

    /* Timing is configurable and repeat can be disabled without reset. */
    hpclang_keyboard_event_engine_set_timing(&engine, 0u, 100u, 0u);
    expect_none(&engine, BIT(7) | BIT(8), 2200000u);
    expect_event(&engine, BIT(7) | BIT(8), 2200000u, HPCLANG_KEYBOARD_EVENT_DOWN, 7u);
    expect_event(&engine, BIT(7) | BIT(8), 2200000u, HPCLANG_KEYBOARD_EVENT_DOWN, 8u);
    expect_none(&engine, BIT(7) | BIT(8), 2500000u);

    /* Simultaneous releases are drained deterministically, lowest scan first. */
    expect_none(&engine, 0u, 2600000u);
    expect_event(&engine, 0u, 2600000u, HPCLANG_KEYBOARD_EVENT_UP, 7u);
    expect_event(&engine, 0u, 2600000u, HPCLANG_KEYBOARD_EVENT_UP, 8u);
    expect_none(&engine, 0u, 2600000u);

    puts("PASS: polled keyboard event debounce/repeat engine.");
    return EXIT_SUCCESS;
}
