#include "win-key-state.h"
#include <hpkeyb49.h>
#include <errno.h>
#include <stddef.h>

static unsigned effective(const struct hpclang_win_key_state *state, unsigned index)
{
    return state->held[index] ? 3u : state->latched[index];
}
static char character(const struct hpclang_win_key_state *state, unsigned key)
{
    static const char common[3][64] = {
        {[KB_0] = '0',
         [KB_1] = '1',
         [KB_2] = '2',
         [KB_3] = '3',
         [KB_4] = '4',
         [KB_5] = '5',
         [KB_6] = '6',
         [KB_7] = '7',
         [KB_8] = '8',
         [KB_9] = '9',
         [KB_BKS] = '\b',
         [KB_MUL] = '*',
         [KB_ADD] = '+',
         [KB_SUB] = '-',
         [KB_DOT] = '.',
         [KB_SPC] = ' ',
         [KB_ENT] = '\n',
         [KB_ON] = '\x1b'},
        {[KB_3] = '#',
         [KB_BKS] = '\x7f',
         [KB_MUL] = '[',
         [KB_ADD] = '{',
         [KB_SUB] = '(',
         [KB_DOT] = ':'},
        {[KB_MUL] = '"', [KB_SUB] = '_', [KB_DOT] = '\r', [KB_SPC] = ','}};
    static const char letters[64] = {
        [KB_A] = 'A', [KB_B] = 'B', [KB_C] = 'C', [KB_D] = 'D', [KB_E] = 'E', [KB_F] = 'F',
        [KB_G] = 'G', [KB_H] = 'H', [KB_I] = 'I', [KB_J] = 'J', [KB_K] = 'K', [KB_L] = 'L',
        [KB_M] = 'M', [KB_N] = 'N', [KB_O] = 'O', [KB_P] = 'P', [KB_Q] = 'Q', [KB_R] = 'R',
        [KB_S] = 'S', [KB_T] = 'T', [KB_U] = 'U', [KB_V] = 'V', [KB_W] = 'W', [KB_X] = 'X',
        [KB_Y] = 'Y', [KB_Z] = 'Z'};
    unsigned plane = state->latched[1] ? 1u : state->latched[2] ? 2u : 0u;
    if (common[plane][key])
        return common[plane][key];
    if (state->latched[0]) {
        char value = letters[key];
        return plane == 2 ? 0 : value && plane == 1 ? (char)(value + ('a' - 'A')) : value;
    }
    if (!plane) {
        if (key == KB_O)
            return '\'';
        if (key == KB_Q)
            return '^';
        if (key == KB_X)
            return 'X';
        if (key == KB_Z)
            return '/';
    } else if (plane == 1) {
        if (key == KB_I)
            return 'i';
    } else {
        if (key == KB_I)
            return '|';
        if (key == KB_W)
            return '=';
        if (key == KB_X)
            return '<';
        if (key == KB_Y)
            return '>';
    }
    return 0;
}
int hpclang_win_key_state_init(struct hpclang_win_key_state *state, hpclang_win_key_post post,
                               void *context)
{
    if (!state || !post) {
        errno = EINVAL;
        return -1;
    }
    *state = (struct hpclang_win_key_state){.post = post, .context = context};
    return 0;
}
static int post(struct hpclang_win_key_state *state, int event, void *data, int *result)
{
    if (state->post(state->context, event, data, result))
        state->failure = state->failure ? state->failure : errno ? errno : EIO;
    if (state->failure) {
        errno = state->failure;
        return -1;
    }
    return 0;
}
int hpclang_win_key_state_feed(struct hpclang_win_key_state *state, unsigned key, int pressed,
                               int *result)
{
    if (!state || !state->post || !result || key >= 64 || (pressed != 0 && pressed != 1)) {
        errno = EINVAL;
        return -1;
    }
    if (state->failure) {
        errno = state->failure;
        return -1;
    }
    unsigned before[3];
    for (unsigned i = 0; i < 3; ++i)
        before[i] = effective(state, i);
    int typed_result = 0;
    if (key == KB_ALPHA && !state->latched[1] && !state->latched[2]) {
        state->held[0] = (unsigned char)pressed;
        if (pressed)
            state->latched[0] = (unsigned char)((state->latched[0] + 1u) % 3u);
    } else if (key == KB_LSHIFT || key == KB_RSHIFT) {
        unsigned index = key == KB_LSHIFT ? 1u : 2u;
        state->held[index] = (unsigned char)pressed;
        if (pressed) {
            state->latched[3 - index] = 0;
            state->latched[index] = (unsigned char)!state->latched[index];
        }
    } else if (pressed) {
        struct hpclang_win_typed_key event = {
            key, character(state, key), (unsigned char)effective(state, 1),
            (unsigned char)effective(state, 2), (unsigned char)effective(state, 0)};
        for (unsigned i = 0; i < 3; ++i)
            if (state->latched[i] == 1)
                state->latched[i] = 0;
        if (post(state, 3, &event, &typed_result))
            return -1;
    }
    /* Snapshot all effective states after typed callbacks, before the first
     * modifier callback. Reentrant modifier delivery cannot rewrite this set. */
    unsigned after[3];
    for (unsigned i = 0; i < 3; ++i)
        after[i] = effective(state, i);
    for (unsigned i = 0; i < 3; ++i) {
        int ignored;
        if (before[i] != after[i] &&
            (post(state, 4 + (int)i, (void *)(uintptr_t)after[i], &ignored) ||
             post(state, 7, NULL, &ignored)))
            return -1;
    }
    *result = typed_result;
    return 0;
}
