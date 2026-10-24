#ifndef HPCLANG_WIN_KEY_STATE_H
#define HPCLANG_WIN_KEY_STATE_H
#include <stdint.h>

/* Internal source-compatibility semantics for historical KB_* codes, not the
 * model-native text-input engine. Payloads are borrowed for synchronous posts.
 * State indices are alpha/left/right; effective values are off/on/lock/held.
 * No public window generator or modifier indicator is installed here. */
struct hpclang_win_typed_key
{
    unsigned keycode;
    char ascii;
    unsigned char lshift, rshift, alpha;
};
typedef int (*hpclang_win_key_post)(void *context, int event, void *data, int *result);
struct hpclang_win_key_state
{
    hpclang_win_key_post post;
    void *context;
    unsigned char latched[3], held[3];
    int failure;
};
/* Initialize only a fresh/inactive context. Post returns dispatch status;
 * its output is the independent callback result, including a valid -1. */
int hpclang_win_key_state_init(struct hpclang_win_key_state *, hpclang_win_key_post, void *);
/* Physical press/release after higher-priority handlers have had their chance
 * to consume it. Typed event 3 precedes modifier events 4..6 and repaint 7.
 * Nested posts observe state transitions immediately. Failed posts latch errno
 * after any partial delivery; callers must unwind before reinitializing.
 * Returns 0 and the typed callback result, or -1/errno with result untouched. */
int hpclang_win_key_state_feed(struct hpclang_win_key_state *, unsigned key, int pressed,
                               int *result);
#endif
