#ifndef HPCLANG_KEYBOARD_H
#define HPCLANG_KEYBOARD_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define HPCLANG_KEYBOARD_COLUMNS 8u
#define HPCLANG_KEYBOARD_ROWS 7u

/*
 * Raw keyboard snapshot.
 *
 * Bits 0..55 represent the physical S3C2410 key matrix:
 *
 *     bit = column * HPCLANG_KEYBOARD_ROWS + row
 *
 * Matrix coordinates describe scan positions, not model-neutral key meanings.
 * A given coordinate may label a different key on another calculator family.
 *
 * Bits 56..59 represent the four dedicated keyboard inputs below. This raw
 * encoding is hpclang-specific and is deliberately not the historical HPGCC
 * keymatrix layout.
 */
struct hpclang_keyboard_state {
    uint64_t bits;
};

enum hpclang_keyboard_dedicated_key {
    HPCLANG_KEYBOARD_ON = 56,
    HPCLANG_KEYBOARD_ALPHA = 57,
    HPCLANG_KEYBOARD_LEFT_SHIFT = 58,
    HPCLANG_KEYBOARD_RIGHT_SHIFT = 59
};

/*
 * Read the instantaneous physical keyboard state without installing an IRQ,
 * changing clock/timer configuration, or retaining GPIO changes after return.
 */
void hpclang_keyboard_read(struct hpclang_keyboard_state *state);

int hpclang_keyboard_any_is_down(
    const struct hpclang_keyboard_state *state);

int hpclang_keyboard_matrix_is_down(
    const struct hpclang_keyboard_state *state,
    unsigned column,
    unsigned row);

int hpclang_keyboard_dedicated_is_down(
    const struct hpclang_keyboard_state *state,
    enum hpclang_keyboard_dedicated_key key);

enum hpclang_keyboard_event_type {
    HPCLANG_KEYBOARD_EVENT_DOWN = 1,
    HPCLANG_KEYBOARD_EVENT_UP = 2,
    HPCLANG_KEYBOARD_EVENT_REPEAT = 3
};

enum hpclang_keyboard_modifier {
    HPCLANG_KEYBOARD_MOD_ON = 1u << 0,
    HPCLANG_KEYBOARD_MOD_ALPHA = 1u << 1,
    HPCLANG_KEYBOARD_MOD_LEFT_SHIFT = 1u << 2,
    HPCLANG_KEYBOARD_MOD_RIGHT_SHIFT = 1u << 3
};

/*
 * Model-neutral physical key identities.
 *
 * These values name only keys whose raw scan positions have been measured on
 * HP39gs, HP40gs, HP48gII, HP49g+, and HP50g. HP39g+ remains UNKNOWN until a
 * trustworthy real-machine fixture is available.
 *
 * This layer does not perform Alpha/Shift-plane or text translation.
 */
enum hpclang_key {
    HPCLANG_KEY_UNKNOWN = 0,
    HPCLANG_KEY_F1 = 1,
    HPCLANG_KEY_F2 = 2,
    HPCLANG_KEY_F3 = 3,
    HPCLANG_KEY_F4 = 4,
    HPCLANG_KEY_F5 = 5,
    HPCLANG_KEY_F6 = 6,
    HPCLANG_KEY_UP = 7,
    HPCLANG_KEY_DOWN = 8,
    HPCLANG_KEY_LEFT = 9,
    HPCLANG_KEY_RIGHT = 10,
    HPCLANG_KEY_ENTER = 11,
    HPCLANG_KEY_0 = 12,
    HPCLANG_KEY_1 = 13,
    HPCLANG_KEY_2 = 14,
    HPCLANG_KEY_3 = 15,
    HPCLANG_KEY_4 = 16,
    HPCLANG_KEY_5 = 17,
    HPCLANG_KEY_6 = 18,
    HPCLANG_KEY_7 = 19,
    HPCLANG_KEY_8 = 20,
    HPCLANG_KEY_9 = 21,
    HPCLANG_KEY_PERIOD = 22,
    HPCLANG_KEY_DIVIDE = 23,
    HPCLANG_KEY_MULTIPLY = 24,
    HPCLANG_KEY_MINUS = 25,
    HPCLANG_KEY_PLUS = 26,
    HPCLANG_KEY_ALPHA = 27,
    HPCLANG_KEY_LEFT_SHIFT = 28,
    HPCLANG_KEY_RIGHT_SHIFT = 29,
    HPCLANG_KEY_ON = 30
};

/*
 * Debounced keyboard event.
 *
 * scan_code uses the same hpclang raw encoding as hpclang_keyboard_state:
 * 0..55 are matrix positions and 56..59 are the dedicated keys. It is a
 * physical scan identity, not a model-neutral key label.
 *
 * modifiers describes the stable held modifier state after this event.
 *
 * key is a fixed-width ABI slot containing one enum hpclang_key value. It is
 * HPCLANG_KEY_UNKNOWN when this scan position has no verified portable
 * identity on the current model.
 */
struct hpclang_keyboard_event {
    uint64_t timestamp_us;
    uint32_t modifiers;
    uint8_t type;
    uint8_t scan_code;
    uint16_t key;
};

/*
 * Reset the polled event service to the current physical state. Keys already
 * held at reset become the baseline and do not generate synthetic DOWN events.
 */
void hpclang_keyboard_events_reset(void);

/*
 * Configure debounce and autorepeat timing. repeat_interval_us == 0 disables
 * repeat. The default is 20 ms debounce, 500 ms initial repeat delay, and
 * 80 ms between repeats.
 */
void hpclang_keyboard_events_set_timing(
    uint32_t debounce_us,
    uint32_t repeat_delay_us,
    uint32_t repeat_interval_us);

/*
 * Poll the physical keyboard and return one event when available.
 *
 * This function is synchronous: hpclang installs no keyboard IRQ or background
 * timer. Applications that use events must call it regularly.
 */
int hpclang_keyboard_event_poll(struct hpclang_keyboard_event *event);

#ifdef __cplusplus
}
#endif

#endif
