#ifndef HPCLANG_WIN_KEYBOARD_H
#define HPCLANG_WIN_KEYBOARD_H
#include "win-events.h"
#include <stdint.h>

struct hpclang_win_keyboard
{
    struct hpclang_win_events *events;
    uint64_t previous;
    int pressed, released, failure;
};
/* Fresh/inactive contexts only. Starts with the historical all-released state.
 * The event bus and all callbacks remain borrowed. No input mode or repeat
 * engine is installed; codes are physical historical matrix bits, not text. */
int hpclang_win_keyboard_init(struct hpclang_win_keyboard *, struct hpclang_win_events *,
                              int pressed, int released);
/* Ascending changed-bit order; update previous before synchronous callbacks so
 * nested polling observes the new baseline. Consuming one event does not hide
 * other key changes. Payload is the historical integer encoded as void *.
 * A dispatch failure latches its errno: partial input must not appear complete
 * or be replayed. The owner must stop the loop and reinitialize after unwinding.
 */
int hpclang_win_keyboard_feed(struct hpclang_win_keyboard *, uint64_t matrix);
/* Read through the checked six-model keymatrix adapter. Read failure preserves
 * the baseline and may be retried. Returns 0 or -1/errno, never a callback result. */
int hpclang_win_keyboard_poll(struct hpclang_win_keyboard *);
#endif
