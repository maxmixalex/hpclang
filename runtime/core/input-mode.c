#include <errno.h>
#include <hpclang/input.h>

enum
{
    ALPHA_OFF,
    ALPHA_ONCE,
    ALPHA_LOCKED
};
enum
{
    SHIFT_OFF,
    SHIFT_LEFT,
    SHIFT_RIGHT
};

static int known_model(uint32_t model)
{
    return model >= HPCLANG_MODEL_HP39GPLUS && model <= HPCLANG_MODEL_HP50G;
}
static int valid(const struct hpclang_text_mode *mode)
{
    return mode && known_model(mode->model) && mode->alpha <= ALPHA_LOCKED &&
           mode->lowercase <= 1 && mode->shift <= SHIFT_RIGHT && mode->suspended <= 1;
}
static int hp39(const struct hpclang_text_mode *mode)
{
    return mode->model <= HPCLANG_MODEL_HP40GS;
}
static uint32_t modifiers(const struct hpclang_text_mode *mode, uint32_t held)
{
    uint32_t result = held & HPCLANG_KEYBOARD_MOD_ALPHA;
    if (hp39(mode)) {
        if (held & HPCLANG_KEYBOARD_MOD_SHIFT)
            result |= HPCLANG_KEYBOARD_MOD_LEFT_SHIFT;
    } else {
        result |= held & (HPCLANG_KEYBOARD_MOD_LEFT_SHIFT | HPCLANG_KEYBOARD_MOD_RIGHT_SHIFT);
    }
    return result;
}
static unsigned shift(const struct hpclang_text_mode *mode)
{
    if (mode->shift)
        return mode->shift;
    uint32_t held = mode->held & ~mode->suppressed;
    if (held & HPCLANG_KEYBOARD_MOD_RIGHT_SHIFT)
        return SHIFT_RIGHT;
    return held & HPCLANG_KEYBOARD_MOD_LEFT_SHIFT ? SHIFT_LEFT : SHIFT_OFF;
}
static enum hpclang_text_plane plane(const struct hpclang_text_mode *mode)
{
    unsigned shifted = shift(mode);
    int alpha =
        mode->alpha != ALPHA_OFF || (mode->held & ~mode->suppressed & HPCLANG_KEYBOARD_MOD_ALPHA);
    if (mode->suspended)
        alpha = 0;
    if (!alpha)
        return shifted == SHIFT_RIGHT  ? HPCLANG_TEXT_RIGHT_SHIFT
               : shifted == SHIFT_LEFT ? HPCLANG_TEXT_LEFT_SHIFT
                                       : HPCLANG_TEXT_PLAIN;
    if (shifted == SHIFT_RIGHT)
        return HPCLANG_TEXT_ALPHA_SYMBOL;
    return (mode->lowercase != (shifted == SHIFT_LEFT)) ? HPCLANG_TEXT_ALPHA_LOWER
                                                        : HPCLANG_TEXT_ALPHA_UPPER;
}
int hpclang_text_mode_init(struct hpclang_text_mode *mode, uint32_t model)
{
    if (!mode) {
        errno = EINVAL;
        return -1;
    }
    if (!known_model(model)) {
        errno = ENOSYS;
        return -1;
    }
    *mode = (struct hpclang_text_mode){.model = model};
    return 0;
}
int hpclang_text_mode_set_lock(struct hpclang_text_mode *mode, enum hpclang_text_lock lock)
{
    if (!valid(mode) || lock < HPCLANG_TEXT_UNLOCKED || lock > HPCLANG_TEXT_LOCK_LOWER) {
        errno = EINVAL;
        return -1;
    }
    mode->alpha = lock == HPCLANG_TEXT_UNLOCKED ? ALPHA_OFF : ALPHA_LOCKED;
    mode->lowercase = lock == HPCLANG_TEXT_LOCK_LOWER;
    mode->shift = SHIFT_OFF;
    mode->suspended = 0;
    mode->suppressed = mode->held;
    return 0;
}
int hpclang_text_mode_get(const struct hpclang_text_mode *mode,
                          struct hpclang_text_mode_status *status)
{
    if (!valid(mode) || !status) {
        errno = EINVAL;
        return -1;
    }
    *status = (struct hpclang_text_mode_status){plane(mode),
                                                mode->alpha != ALPHA_LOCKED ? HPCLANG_TEXT_UNLOCKED
                                                : mode->lowercase ? HPCLANG_TEXT_LOCK_LOWER
                                                                  : HPCLANG_TEXT_LOCK_UPPER};
    return 0;
}
int hpclang_text_mode_event(struct hpclang_text_mode *mode,
                            const struct hpclang_keyboard_event *event,
                            enum hpclang_text_plane *output)
{
    if (!valid(mode) || !event || !output ||
        (event->type != HPCLANG_KEYBOARD_EVENT_DOWN && event->type != HPCLANG_KEYBOARD_EVENT_UP &&
         event->type != HPCLANG_KEYBOARD_EVENT_REPEAT)) {
        errno = EINVAL;
        return -1;
    }
    mode->held = modifiers(mode, event->modifiers);
    mode->used &= mode->held;
    mode->suppressed &= mode->held;
    if (event->type == HPCLANG_KEYBOARD_EVENT_UP)
        return 0;

    uint32_t bit = 0;
    if (event->key == HPCLANG_KEY_ALPHA)
        bit = HPCLANG_KEYBOARD_MOD_ALPHA;
    else if ((hp39(mode) && event->key == HPCLANG_KEY_HP39_SHIFT) ||
             (!hp39(mode) && event->key == HPCLANG_KEY_LEFT_SHIFT))
        bit = HPCLANG_KEYBOARD_MOD_LEFT_SHIFT;
    else if (!hp39(mode) && event->key == HPCLANG_KEY_RIGHT_SHIFT)
        bit = HPCLANG_KEYBOARD_MOD_RIGHT_SHIFT;

    if (bit) {
        if (event->type == HPCLANG_KEYBOARD_EVENT_REPEAT || (mode->used & bit))
            return 0;
        if (bit != HPCLANG_KEYBOARD_MOD_ALPHA) {
            unsigned selected = bit == HPCLANG_KEYBOARD_MOD_LEFT_SHIFT ? SHIFT_LEFT : SHIFT_RIGHT;
            mode->shift = mode->shift == selected ? SHIFT_OFF : selected;
            return 0;
        }
        unsigned shifted = shift(mode);
        if (hp39(mode)) {
            if (mode->alpha == ALPHA_LOCKED)
                mode->suspended = !mode->suspended;
            else {
                mode->alpha = mode->alpha == ALPHA_ONCE ? ALPHA_OFF : ALPHA_ONCE;
                mode->lowercase = 0;
            }
        } else if (shifted != SHIFT_OFF) {
            if (mode->alpha != ALPHA_LOCKED || shifted == SHIFT_RIGHT) {
                *output =
                    shifted == SHIFT_LEFT ? HPCLANG_TEXT_LEFT_SHIFT : HPCLANG_TEXT_RIGHT_SHIFT;
                mode->shift = SHIFT_OFF;
                if (mode->alpha == ALPHA_ONCE)
                    mode->alpha = ALPHA_OFF;
                mode->suppressed |= mode->held;
                mode->used |= mode->held;
                return 1;
            }
            mode->lowercase = !mode->lowercase;
            mode->shift = SHIFT_OFF;
            mode->suppressed |= mode->held & ~HPCLANG_KEYBOARD_MOD_ALPHA;
        } else {
            mode->alpha = mode->alpha == ALPHA_OFF    ? ALPHA_ONCE
                          : mode->alpha == ALPHA_ONCE ? ALPHA_LOCKED
                                                      : ALPHA_OFF;
            mode->lowercase = 0;
        }
        return 0;
    }

    *output = plane(mode);
    if (mode->alpha == ALPHA_ONCE)
        mode->alpha = ALPHA_OFF;
    mode->shift = SHIFT_OFF;
    mode->suspended = 0;
    /* Stable snapshots may report a held modifier before its own DOWN event
     * is dequeued. Do not subsequently latch that already-used press. */
    mode->used |= mode->held;
    return 1;
}
