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
 * Bits 56..59 represent the four dedicated keyboard inputs below. Bit 56 is
 * ON on all validated models. Bits 57..59 are the physical GPF4/5/6 inputs;
 * their key labels differ on HP39 versus HP48/49/50. This raw encoding is
 * hpclang-specific and is deliberately not the historical HPGCC keymatrix
 * layout.
 */
struct hpclang_keyboard_state {
    uint64_t bits;
};

enum hpclang_keyboard_dedicated_key {
    HPCLANG_KEYBOARD_ON = 56,
    HPCLANG_KEYBOARD_GPF4 = 57,
    HPCLANG_KEYBOARD_GPF5 = 58,
    HPCLANG_KEYBOARD_GPF6 = 59
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
    HPCLANG_KEYBOARD_MOD_RIGHT_SHIFT = 1u << 3,

    /*
     * Normalized physical Shift state. This bit is set for the single HP39
     * Shift key and for either HP48/49/50 Shift key. The LEFT/RIGHT bits above
     * remain available when the distinction exists on the current model.
     */
    HPCLANG_KEYBOARD_MOD_SHIFT = 1u << 4
};

/*
 * Physical key identities.
 *
 * Common values name keys that are semantically the same across supported
 * layouts. Model/family-prefixed values name primary keys whose physical
 * position or label differs between calculator families.
 *
 * HP39g+ remains UNKNOWN until a trustworthy real-machine fixture is
 * available. This layer does not perform Alpha/Shift-plane or text
 * translation.
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
    HPCLANG_KEY_ON = 30,

    HPCLANG_KEY_HP39_SYMB = 0x0100,
    HPCLANG_KEY_HP39_PLOT,
    HPCLANG_KEY_HP39_NUM,
    HPCLANG_KEY_HP39_HOME,
    HPCLANG_KEY_HP39_APLET,
    HPCLANG_KEY_HP39_VIEWS,
    HPCLANG_KEY_HP39_VARS,
    HPCLANG_KEY_HP39_MATH,
    HPCLANG_KEY_HP39_DERIVATIVE,
    HPCLANG_KEY_HP39_X_THETA_T,
    HPCLANG_KEY_HP39_DEL,
    HPCLANG_KEY_HP39_SIN,
    HPCLANG_KEY_HP39_COS,
    HPCLANG_KEY_HP39_TAN,
    HPCLANG_KEY_HP39_LN,
    HPCLANG_KEY_HP39_LOG,
    HPCLANG_KEY_HP39_SQUARE,
    HPCLANG_KEY_HP39_POWER,
    HPCLANG_KEY_HP39_LEFT_PAREN,
    HPCLANG_KEY_HP39_RIGHT_PAREN,
    HPCLANG_KEY_HP39_COMMA,
    HPCLANG_KEY_HP39_SHIFT,
    HPCLANG_KEY_HP39_NEGATE,

    HPCLANG_KEY_HP48_APPS = 0x0200,
    HPCLANG_KEY_HP48_MODE,
    HPCLANG_KEY_HP48_TOOL,
    HPCLANG_KEY_HP48_VAR,
    HPCLANG_KEY_HP48_STO,
    HPCLANG_KEY_HP48_NXT,
    HPCLANG_KEY_HP48_HIST,
    HPCLANG_KEY_HP48_EVAL,
    HPCLANG_KEY_HP48_QUOTE,
    HPCLANG_KEY_HP48_SYMB,
    HPCLANG_KEY_HP48_POWER,
    HPCLANG_KEY_HP48_SQRT,
    HPCLANG_KEY_HP48_SIN,
    HPCLANG_KEY_HP48_COS,
    HPCLANG_KEY_HP48_TAN,
    HPCLANG_KEY_HP48_EEX,
    HPCLANG_KEY_HP48_NEGATE,
    HPCLANG_KEY_HP48_X,
    HPCLANG_KEY_HP48_RECIPROCAL,
    HPCLANG_KEY_HP48_BACKSPACE,
    HPCLANG_KEY_HP48_SPACE,

    HPCLANG_KEY_HP4950_APPS = 0x0300,
    HPCLANG_KEY_HP4950_MODE,
    HPCLANG_KEY_HP4950_TOOL,
    HPCLANG_KEY_HP4950_VAR,
    HPCLANG_KEY_HP4950_STO,
    HPCLANG_KEY_HP4950_NXT,
    HPCLANG_KEY_HP4950_HIST,
    HPCLANG_KEY_HP4950_SYMB,
    HPCLANG_KEY_HP4950_POWER,
    HPCLANG_KEY_HP4950_SQRT,
    HPCLANG_KEY_HP4950_SIN,
    HPCLANG_KEY_HP4950_COS,
    HPCLANG_KEY_HP4950_TAN,
    HPCLANG_KEY_HP4950_EEX,
    HPCLANG_KEY_HP4950_NEGATE,
    HPCLANG_KEY_HP4950_X,
    HPCLANG_KEY_HP4950_RECIPROCAL,
    HPCLANG_KEY_HP4950_BACKSPACE,
    HPCLANG_KEY_HP4950_SPACE,

    /* These two primary-key positions differ between HP49g+ and HP50g. */
    HPCLANG_KEY_HP49_CAT = 0x0400,
    HPCLANG_KEY_HP49_EQW,

    HPCLANG_KEY_HP50_EVAL = 0x0500,
    HPCLANG_KEY_HP50_QUOTE
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
 * HPCLANG_KEY_UNKNOWN when this scan position has no supported identity on
 * the current model.
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
