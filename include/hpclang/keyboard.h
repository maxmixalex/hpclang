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

#ifdef __cplusplus
}
#endif

#endif
