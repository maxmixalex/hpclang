#include "win-key-state.h"
#include <hpkeyb49.h>
#include <assert.h>
#include <errno.h>
#include <stddef.h>

static unsigned calls, fail_at;
static struct hpclang_win_typed_key typed;
static struct hpclang_win_key_state *nested;
static int post(void *context, int event, void *data, int *result)
{
    assert(context == &calls);
    if (++calls == fail_at) {
        errno = ENOSPC;
        return -1;
    }
    if (event == 3)
        typed = *(struct hpclang_win_typed_key *)data;
    if (event == 3 && nested) {
        struct hpclang_win_key_state *current = nested;
        nested = NULL;
        int value = 42;
        assert(hpclang_win_key_state_feed(current, KB_1, 1, &value) == -1);
        assert(value == 42 && errno == ENOSPC);
    }
    *result = -1;
    return 0;
}
int main(void)
{
    struct hpclang_win_key_state state;
    int result = 99;
    assert(hpclang_win_key_state_init(NULL, post, &calls) == -1 && errno == EINVAL);
    assert(hpclang_win_key_state_init(&state, NULL, &calls) == -1 && errno == EINVAL);
    assert(!hpclang_win_key_state_init(&state, post, &calls));
    assert(hpclang_win_key_state_feed(&state, 64, 1, &result) == -1 && result == 99 && !calls);
    assert(hpclang_win_key_state_feed(&state, KB_A, 2, &result) == -1 && result == 99 && !calls);
    assert(hpclang_win_key_state_feed(&state, KB_A, 1, NULL) == -1 && !calls);
    assert(!hpclang_win_key_state_feed(&state, KB_1, 1, &result));
    assert(result == -1 && !state.failure && typed.ascii == '1');
    /* Fail each stage of a one-shot alpha key: typed, modifier, repaint. */
    for (unsigned stage = 1; stage <= 3; ++stage) {
        calls = fail_at = 0;
        assert(!hpclang_win_key_state_init(&state, post, &calls));
        assert(!hpclang_win_key_state_feed(&state, KB_ALPHA, 1, &result));
        assert(!hpclang_win_key_state_feed(&state, KB_ALPHA, 0, &result));
        assert(state.latched[0] == 1);
        calls = 0;
        fail_at = stage;
        result = 99;
        assert(hpclang_win_key_state_feed(&state, KB_A, 1, &result) == -1 && errno == ENOSPC);
        assert(result == 99 && state.failure == ENOSPC && calls == stage && !state.latched[0]);
        unsigned before = calls;
        assert(hpclang_win_key_state_feed(&state, KB_B, 1, &result) == -1 && calls == before);
    }
    calls = 0;
    fail_at = 2;
    result = 99;
    assert(!hpclang_win_key_state_init(&state, post, &calls));
    nested = &state;
    assert(hpclang_win_key_state_feed(&state, KB_A, 1, &result) == -1);
    assert(result == 99 && calls == 2 && state.failure == ENOSPC && errno == ENOSPC);
    return 0;
}
