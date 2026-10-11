#include <assert.h>
#include <errno.h>
#include <string.h>
#include <hpclang/input.h>

static int send(struct hpclang_text_mode *mode, uint16_t key, unsigned type, uint32_t held,
                enum hpclang_text_plane expected, int result)
{
    struct hpclang_keyboard_event event = {.key = key, .type = type, .modifiers = held};
    struct hpclang_keyboard_event before = event;
    enum hpclang_text_plane output = HPCLANG_TEXT_ALPHA_SYMBOL;
    errno = EDOM;
    assert(hpclang_text_mode_event(mode, &event, &output) == result);
    assert(errno == EDOM && memcmp(&event, &before, sizeof(event)) == 0);
    assert(output == (result ? expected : HPCLANG_TEXT_ALPHA_SYMBOL));
    return 0;
}
static void tap(struct hpclang_text_mode *mode, uint16_t key, uint32_t bit)
{
    send(mode, key, HPCLANG_KEYBOARD_EVENT_DOWN, bit, HPCLANG_TEXT_PLAIN, 0);
    send(mode, key, HPCLANG_KEYBOARD_EVENT_UP, 0, HPCLANG_TEXT_PLAIN, 0);
}
static void key(struct hpclang_text_mode *mode, enum hpclang_text_plane expected)
{
    send(mode, HPCLANG_KEY_2, HPCLANG_KEYBOARD_EVENT_DOWN, 0, expected, 1);
    send(mode, HPCLANG_KEY_2, HPCLANG_KEYBOARD_EVENT_UP, 0, expected, 0);
}
static void status(struct hpclang_text_mode *mode, enum hpclang_text_plane plane,
                   enum hpclang_text_lock lock)
{
    struct hpclang_text_mode_status s;
    errno = EDOM;
    assert(hpclang_text_mode_get(mode, &s) == 0 && errno == EDOM);
    assert(s.plane == plane && s.lock == lock);
}
int main(void)
{
    for (uint32_t model = HPCLANG_MODEL_HP39GPLUS; model <= HPCLANG_MODEL_HP50G; ++model) {
        int is39 = model <= HPCLANG_MODEL_HP40GS;
        uint16_t left = is39 ? HPCLANG_KEY_HP39_SHIFT : HPCLANG_KEY_LEFT_SHIFT;
        uint32_t left_bit = is39 ? HPCLANG_KEYBOARD_MOD_SHIFT : HPCLANG_KEYBOARD_MOD_LEFT_SHIFT;
        struct hpclang_text_mode mode;
        assert(hpclang_text_mode_init(&mode, model) == 0);
        status(&mode, HPCLANG_TEXT_PLAIN, HPCLANG_TEXT_UNLOCKED);

        /* Native single Alpha, next key returns to ordinary input. */
        tap(&mode, HPCLANG_KEY_ALPHA, HPCLANG_KEYBOARD_MOD_ALPHA);
        status(&mode, HPCLANG_TEXT_ALPHA_UPPER, HPCLANG_TEXT_UNLOCKED);
        key(&mode, HPCLANG_TEXT_ALPHA_UPPER);
        key(&mode, HPCLANG_TEXT_PLAIN);

        /* Two taps cancel on HP39; classic models lock. */
        tap(&mode, HPCLANG_KEY_ALPHA, HPCLANG_KEYBOARD_MOD_ALPHA);
        tap(&mode, HPCLANG_KEY_ALPHA, HPCLANG_KEYBOARD_MOD_ALPHA);
        key(&mode, is39 ? HPCLANG_TEXT_PLAIN : HPCLANG_TEXT_ALPHA_UPPER);
        status(&mode, is39 ? HPCLANG_TEXT_PLAIN : HPCLANG_TEXT_ALPHA_UPPER,
               is39 ? HPCLANG_TEXT_UNLOCKED : HPCLANG_TEXT_LOCK_UPPER);
        if (!is39) {
            tap(&mode, left, left_bit);
            key(&mode, HPCLANG_TEXT_ALPHA_LOWER);
            key(&mode, HPCLANG_TEXT_ALPHA_UPPER);
            tap(&mode, left, left_bit);
            tap(&mode, HPCLANG_KEY_ALPHA, HPCLANG_KEYBOARD_MOD_ALPHA);
            status(&mode, HPCLANG_TEXT_ALPHA_LOWER, HPCLANG_TEXT_LOCK_LOWER);
            key(&mode, HPCLANG_TEXT_ALPHA_LOWER);
            tap(&mode, left, left_bit);
            key(&mode, HPCLANG_TEXT_ALPHA_UPPER);
            key(&mode, HPCLANG_TEXT_ALPHA_LOWER);
            tap(&mode, left, left_bit);
            tap(&mode, HPCLANG_KEY_ALPHA, HPCLANG_KEYBOARD_MOD_ALPHA);
            status(&mode, HPCLANG_TEXT_ALPHA_UPPER, HPCLANG_TEXT_LOCK_UPPER);
            tap(&mode, HPCLANG_KEY_ALPHA, HPCLANG_KEYBOARD_MOD_ALPHA);
            status(&mode, HPCLANG_TEXT_PLAIN, HPCLANG_TEXT_UNLOCKED);
        }

        /* Held Alpha allows several keys, then releases without a sticky tail. */
        send(&mode, HPCLANG_KEY_ALPHA, HPCLANG_KEYBOARD_EVENT_DOWN, HPCLANG_KEYBOARD_MOD_ALPHA,
             HPCLANG_TEXT_PLAIN, 0);
        for (unsigned i = 0; i < 2; ++i) {
            send(&mode, HPCLANG_KEY_2, HPCLANG_KEYBOARD_EVENT_DOWN, HPCLANG_KEYBOARD_MOD_ALPHA,
                 HPCLANG_TEXT_ALPHA_UPPER, 1);
            send(&mode, HPCLANG_KEY_2, HPCLANG_KEYBOARD_EVENT_UP, HPCLANG_KEYBOARD_MOD_ALPHA,
                 HPCLANG_TEXT_PLAIN, 0);
        }
        send(&mode, HPCLANG_KEY_ALPHA, HPCLANG_KEYBOARD_EVENT_UP, 0, HPCLANG_TEXT_PLAIN, 0);
        key(&mode, HPCLANG_TEXT_PLAIN);

        /* A held snapshot can precede the modifier's queued DOWN event. */
        send(&mode, HPCLANG_KEY_2, HPCLANG_KEYBOARD_EVENT_DOWN, HPCLANG_KEYBOARD_MOD_ALPHA,
             HPCLANG_TEXT_ALPHA_UPPER, 1);
        send(&mode, HPCLANG_KEY_ALPHA, HPCLANG_KEYBOARD_EVENT_DOWN, HPCLANG_KEYBOARD_MOD_ALPHA,
             HPCLANG_TEXT_PLAIN, 0);
        send(&mode, HPCLANG_KEY_ALPHA, HPCLANG_KEYBOARD_EVENT_UP, 0, HPCLANG_TEXT_PLAIN, 0);
        key(&mode, HPCLANG_TEXT_PLAIN);
        send(&mode, HPCLANG_KEY_2, HPCLANG_KEYBOARD_EVENT_DOWN, left_bit, HPCLANG_TEXT_LEFT_SHIFT,
             1);
        send(&mode, left, HPCLANG_KEYBOARD_EVENT_DOWN, left_bit, HPCLANG_TEXT_PLAIN, 0);
        send(&mode, left, HPCLANG_KEYBOARD_EVENT_UP, 0, HPCLANG_TEXT_PLAIN, 0);
        key(&mode, HPCLANG_TEXT_PLAIN);

        /* Menu-controlled lock and single-character override, without changing
         * the lower physical event API. */
        assert(hpclang_text_mode_set_lock(&mode, HPCLANG_TEXT_LOCK_LOWER) == 0);
        tap(&mode, left, left_bit);
        key(&mode, HPCLANG_TEXT_ALPHA_UPPER);
        key(&mode, HPCLANG_TEXT_ALPHA_LOWER);
        if (is39) {
            tap(&mode, HPCLANG_KEY_ALPHA, HPCLANG_KEYBOARD_MOD_ALPHA);
            key(&mode, HPCLANG_TEXT_PLAIN);
            key(&mode, HPCLANG_TEXT_ALPHA_LOWER);
        } else {
            tap(&mode, HPCLANG_KEY_RIGHT_SHIFT, HPCLANG_KEYBOARD_MOD_RIGHT_SHIFT);
            key(&mode, HPCLANG_TEXT_ALPHA_SYMBOL);
            key(&mode, HPCLANG_TEXT_ALPHA_LOWER);
        }
        assert(hpclang_text_mode_set_lock(&mode, HPCLANG_TEXT_UNLOCKED) == 0);

        /* Modifier autorepeat must not cycle mode or lock accidentally. */
        send(&mode, HPCLANG_KEY_ALPHA, HPCLANG_KEYBOARD_EVENT_DOWN, HPCLANG_KEYBOARD_MOD_ALPHA,
             HPCLANG_TEXT_PLAIN, 0);
        send(&mode, HPCLANG_KEY_ALPHA, HPCLANG_KEYBOARD_EVENT_REPEAT, HPCLANG_KEYBOARD_MOD_ALPHA,
             HPCLANG_TEXT_PLAIN, 0);
        send(&mode, HPCLANG_KEY_ALPHA, HPCLANG_KEYBOARD_EVENT_UP, 0, HPCLANG_TEXT_PLAIN, 0);
        key(&mode, HPCLANG_TEXT_ALPHA_UPPER);
        status(&mode, HPCLANG_TEXT_PLAIN, HPCLANG_TEXT_UNLOCKED);

        if (!is39) {
            /* Native Shift+Alpha is a shifted command unless Alpha is locked. */
            for (unsigned once = 0; once < 2; ++once) {
                if (once)
                    tap(&mode, HPCLANG_KEY_ALPHA, HPCLANG_KEYBOARD_MOD_ALPHA);
                tap(&mode, left, left_bit);
                send(&mode, HPCLANG_KEY_ALPHA, HPCLANG_KEYBOARD_EVENT_DOWN,
                     HPCLANG_KEYBOARD_MOD_ALPHA, HPCLANG_TEXT_LEFT_SHIFT, 1);
                send(&mode, HPCLANG_KEY_ALPHA, HPCLANG_KEYBOARD_EVENT_UP, 0, HPCLANG_TEXT_PLAIN, 0);
                key(&mode, HPCLANG_TEXT_PLAIN);
            }
            assert(hpclang_text_mode_set_lock(&mode, HPCLANG_TEXT_LOCK_UPPER) == 0);
            tap(&mode, HPCLANG_KEY_RIGHT_SHIFT, HPCLANG_KEYBOARD_MOD_RIGHT_SHIFT);
            send(&mode, HPCLANG_KEY_ALPHA, HPCLANG_KEYBOARD_EVENT_DOWN, HPCLANG_KEYBOARD_MOD_ALPHA,
                 HPCLANG_TEXT_RIGHT_SHIFT, 1);
            send(&mode, HPCLANG_KEY_ALPHA, HPCLANG_KEYBOARD_EVENT_UP, 0, HPCLANG_TEXT_PLAIN, 0);
            key(&mode, HPCLANG_TEXT_ALPHA_UPPER);
        } else {
            /* Native HOME: Shift then held Alpha produces yY, not yy. Shift
             * is consumed by the first character; Alpha remains physically held. */
            tap(&mode, left, left_bit);
            send(&mode, HPCLANG_KEY_ALPHA, HPCLANG_KEYBOARD_EVENT_DOWN, HPCLANG_KEYBOARD_MOD_ALPHA,
                 HPCLANG_TEXT_PLAIN, 0);
            send(&mode, HPCLANG_KEY_2, HPCLANG_KEYBOARD_EVENT_DOWN, HPCLANG_KEYBOARD_MOD_ALPHA,
                 HPCLANG_TEXT_ALPHA_LOWER, 1);
            send(&mode, HPCLANG_KEY_2, HPCLANG_KEYBOARD_EVENT_UP, HPCLANG_KEYBOARD_MOD_ALPHA,
                 HPCLANG_TEXT_PLAIN, 0);
            send(&mode, HPCLANG_KEY_2, HPCLANG_KEYBOARD_EVENT_DOWN, HPCLANG_KEYBOARD_MOD_ALPHA,
                 HPCLANG_TEXT_ALPHA_UPPER, 1);
            send(&mode, HPCLANG_KEY_ALPHA, HPCLANG_KEYBOARD_EVENT_UP, 0, HPCLANG_TEXT_PLAIN, 0);
            key(&mode, HPCLANG_TEXT_PLAIN);
        }

        /* A menu action consumes its held modifiers until release. */
        assert(hpclang_text_mode_init(&mode, model) == 0);
        send(&mode, left, HPCLANG_KEYBOARD_EVENT_DOWN, left_bit, HPCLANG_TEXT_PLAIN, 0);
        assert(hpclang_text_mode_set_lock(&mode, HPCLANG_TEXT_LOCK_LOWER) == 0);
        send(&mode, HPCLANG_KEY_2, HPCLANG_KEYBOARD_EVENT_DOWN, left_bit, HPCLANG_TEXT_ALPHA_LOWER,
             1);
        send(&mode, left, HPCLANG_KEYBOARD_EVENT_UP, 0, HPCLANG_TEXT_PLAIN, 0);
        key(&mode, HPCLANG_TEXT_ALPHA_LOWER);
    }

    struct hpclang_text_mode mode;
    assert(hpclang_text_mode_init(&mode, HPCLANG_MODEL_HP50G) == 0);
    struct hpclang_text_mode before = mode;
    enum hpclang_text_plane plane = HPCLANG_TEXT_ALPHA_SYMBOL;
    struct hpclang_keyboard_event event = {.type = 99};
    assert(hpclang_text_mode_event(&mode, &event, &plane) == -1 && errno == EINVAL);
    assert(memcmp(&mode, &before, sizeof(mode)) == 0 && plane == HPCLANG_TEXT_ALPHA_SYMBOL);
    assert(hpclang_text_mode_init(&mode, 0) == -1 && errno == ENOSYS);
    assert(memcmp(&mode, &before, sizeof(mode)) == 0);
    assert(hpclang_text_mode_set_lock(&mode, (enum hpclang_text_lock)99) == -1 && errno == EINVAL);
    assert(memcmp(&mode, &before, sizeof(mode)) == 0);
    assert(hpclang_text_mode_get(&mode, NULL) == -1 && errno == EINVAL);
    assert(hpclang_text_mode_init(NULL, HPCLANG_MODEL_HP50G) == -1 && errno == EINVAL);
    assert(hpclang_text_mode_event(&mode, NULL, &plane) == -1 && errno == EINVAL);
    return 0;
}
