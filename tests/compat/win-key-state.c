#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <hpkeyb49.h>
#ifdef REFERENCE
#include "winpriv.h"
extern char _win_alpha, _win_lshift, _win_rshift;
extern char _win_alpha_p, _win_lshift_p, _win_rshift_p;
int _win_default_key_handler(void *, void *);
typedef win_keytyped_t typed_key;
#else
#include "win-key-state.h"
typedef struct hpclang_win_typed_key typed_key;
static struct hpclang_win_key_state state;
#endif
static unsigned nested;
static int feed(unsigned key, int pressed);
static int deliver(int event, void *data)
{
    if (event == 3) {
        const typed_key *key = data;
        printf("T %u %u %u %u %u\n", key->keycode, (unsigned char)key->ascii, key->alpha,
               key->lshift, key->rshift);
        if (nested == 1 || nested == 2) {
            unsigned code = nested == 1 ? KB_ALPHA : KB_LSHIFT;
            nested = 0;
            feed(code, 1);
            feed(code, 0);
        }
        return key->keycode % 4 == 0 ? -1 : key->keycode % 4 == 1 ? 17 : 0;
    }
    printf("E %d %u\n", event, (unsigned)(uintptr_t)data);
    if (event == 4 && nested == 3) {
        nested = 0;
        feed(KB_RSHIFT, 1);
        feed(KB_RSHIFT, 0);
    }
    return 5;
}
#ifdef REFERENCE
int win_post_event(int event, void *data)
{
    return deliver(event, data);
}
static void reset(void)
{
    _win_alpha = _win_lshift = _win_rshift = 0;
    _win_alpha_p = _win_lshift_p = _win_rshift_p = 0;
    nested = 0;
}
#else
static int post(void *context, int event, void *data, int *result)
{
    (void)context;
    *result = deliver(event, data);
    return 0;
}
static void reset(void)
{
    assert(!hpclang_win_key_state_init(&state, post, NULL));
    nested = 0;
}
#endif
static int feed(unsigned key, int pressed)
{
    printf("K %u %d\n", key, pressed);
#ifdef REFERENCE
    int result =
        _win_default_key_handler((void *)(uintptr_t)key, (void *)(uintptr_t)(pressed ? 1 : 2));
    printf("R %d %d %d %d %d %d %d\n", result, _win_alpha, _win_lshift, _win_rshift, _win_alpha_p,
           _win_lshift_p, _win_rshift_p);
#else
    int result;
    assert(!hpclang_win_key_state_feed(&state, key, pressed, &result));
    printf("R %d %u %u %u %u %u %u\n", result, state.latched[0], state.latched[1], state.latched[2],
           state.held[0], state.held[1], state.held[2]);
#endif
    return result;
}
static void tap(unsigned key)
{
    feed(key, 1);
    feed(key, 0);
}
int main(void)
{
    for (unsigned alpha = 0; alpha < 3; ++alpha)
        for (unsigned left = 0; left < 3; ++left)
            for (unsigned right = 0; right < 3; ++right)
                for (unsigned key = 0; key < 64; ++key) {
                    reset();
                    printf("CASE %u %u %u %u\n", alpha, left, right, key);
                    for (unsigned i = 0; i < alpha; ++i)
                        tap(KB_ALPHA);
                    if (left) {
                        feed(KB_LSHIFT, 1);
                        if (left == 1)
                            feed(KB_LSHIFT, 0);
                    }
                    if (right) {
                        feed(KB_RSHIFT, 1);
                        if (right == 1)
                            feed(KB_RSHIFT, 0);
                    }
                    tap(key);
                    tap(KB_A);
                    tap(KB_1);
                    feed(KB_ALPHA, 0);
                    feed(KB_LSHIFT, 0);
                    feed(KB_RSHIFT, 0);
                }
    /* Held alpha and shifted alpha have distinct latch/payload semantics. */
    for (unsigned alpha = 0; alpha < 3; ++alpha)
        for (unsigned shift = 0; shift < 3; ++shift)
            for (unsigned key = 0; key < 64; ++key) {
                reset();
                for (unsigned i = 0; i < alpha; ++i)
                    tap(KB_ALPHA);
                feed(KB_ALPHA, 1);
                if (shift)
                    feed(shift == 1 ? KB_LSHIFT : KB_RSHIFT, 1);
                tap(key);
                tap(KB_A);
                tap(KB_B);
                feed(KB_ALPHA, 0);
                feed(KB_LSHIFT, 0);
                feed(KB_RSHIFT, 0);
            }
    for (unsigned mode = 1; mode <= 3; ++mode) {
        reset();
        tap(KB_ALPHA);
        tap(KB_LSHIFT);
        nested = mode;
        tap(KB_A);
        tap(KB_ALPHA);
        tap(KB_B);
        tap(KB_LSHIFT);
        tap(KB_C);
    }
    return 0;
}
