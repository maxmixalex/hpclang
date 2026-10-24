#include "win-keyboard.h"
#include "hpgcc-keyboard.h"
#include <errno.h>

static int valid(const struct hpclang_win_keyboard *keyboard)
{
    if (!keyboard || !keyboard->events || keyboard->pressed < 0 || keyboard->released < 0) {
        errno = EINVAL;
        return 0;
    }
    if (keyboard->failure) {
        errno = keyboard->failure;
        return 0;
    }
    return 1;
}
int hpclang_win_keyboard_init(struct hpclang_win_keyboard *keyboard,
                              struct hpclang_win_events *events, int pressed, int released)
{
    if (!keyboard || !events || pressed < 0 || released < 0 || pressed == released) {
        errno = EINVAL;
        return -1;
    }
    *keyboard = (struct hpclang_win_keyboard){events, 0, pressed, released, 0};
    return 0;
}
int hpclang_win_keyboard_feed(struct hpclang_win_keyboard *keyboard, uint64_t matrix)
{
    if (!valid(keyboard))
        return -1;
    uint64_t changed = matrix ^ keyboard->previous;
    keyboard->previous = matrix;
    for (unsigned key = 0; key < 64; ++key) {
        uint64_t mask = UINT64_C(1) << key;
        if (!(changed & mask))
            continue;
        int result;
        int event = matrix & mask ? keyboard->pressed : keyboard->released;
        if (hpclang_win_events_post(keyboard->events, event, (void *)(uintptr_t)key, &result)) {
            keyboard->failure = errno;
            return -1;
        }
        if (keyboard->failure) {
            errno = keyboard->failure;
            return -1;
        }
    }
    return 0;
}
int hpclang_win_keyboard_poll(struct hpclang_win_keyboard *keyboard)
{
    if (!valid(keyboard))
        return -1;
    keymatrix matrix;
    if (hpclang_hpgcc_keymatrix_read(&matrix))
        return -1;
    return hpclang_win_keyboard_feed(keyboard, matrix.full);
}
