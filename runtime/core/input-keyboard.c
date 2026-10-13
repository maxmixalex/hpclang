#include <errno.h>
#include <hpclang/input.h>
#include "input-event-plan.h"

int hpclang_input_plan_event(struct hpclang_text_mode *mode,
                             const struct hpclang_keyboard_event *event,
                             struct hpclang_input_plan *plan)
{
    if (!plan) {
        errno = EINVAL;
        return -1;
    }
    enum hpclang_text_plane plane;
    int result = hpclang_text_mode_event(mode, event, &plane);
    if (result <= 0)
        return result;
    enum hpclang_input_action action = HPCLANG_INPUT_INSERT;
    switch (event->key) {
    case HPCLANG_KEY_ENTER:
        action = HPCLANG_INPUT_ACCEPT;
        break;
    case HPCLANG_KEY_ON:
        action = HPCLANG_INPUT_CANCEL;
        break;
    case HPCLANG_KEY_LEFT:
        action = HPCLANG_INPUT_LEFT;
        break;
    case HPCLANG_KEY_RIGHT:
        action = HPCLANG_INPUT_RIGHT;
        break;
    case HPCLANG_KEY_HP39_DEL:
    case HPCLANG_KEY_HP48_BACKSPACE:
    case HPCLANG_KEY_HP4950_BACKSPACE:
        action = HPCLANG_INPUT_BACKSPACE;
        break;
    default:
        break;
    }
    if (action != HPCLANG_INPUT_INSERT) {
        *plan = (struct hpclang_input_plan){.action = action};
        return 1;
    }
    uint32_t codepoint;
    result = hpclang_text_key_lookup(mode->model, event->key, plane, event->modifiers, &codepoint);
    if (result <= 0)
        return result;
    uint32_t closing = 0;
    if (mode->model >= HPCLANG_MODEL_HP48GII) {
        if (plane == HPCLANG_TEXT_LEFT_SHIFT) {
            if (codepoint == '[')
                closing = ']';
            else if (codepoint == '(')
                closing = ')';
            else if (codepoint == '{')
                closing = '}';
            else if (codepoint == ':')
                closing = ':';
        } else if (plane == HPCLANG_TEXT_RIGHT_SHIFT || plane == HPCLANG_TEXT_ALPHA_SYMBOL) {
            if (codepoint == 0x00ab)
                closing = 0x00bb;
            else if (codepoint == '"')
                closing = '"';
        }
    }
    *plan = (struct hpclang_input_plan){HPCLANG_INPUT_INSERT, codepoint, closing};
    return 1;
}

int hpclang_input_plan_apply(struct hpclang_text_input *input,
                             const struct hpclang_input_plan *plan)
{
    if (!plan) {
        errno = EINVAL;
        return -1;
    }
    if (hpclang_text_input_edit(input, HPCLANG_INPUT_CANCEL, 0) < 0)
        return -1;
    if (plan->closing) {
        /* Stock inserts delimiter pairs with the cursor inside. Preflight the
         * complete UTF-8 pair so a full buffer cannot retain only its opener. */
        size_t bytes = plan->codepoint == 0x00ab ? 4 : 2;
        if (input->capacity - input->length - 1 < bytes) {
            errno = ENOSPC;
            return -1;
        }
        if (hpclang_text_input_edit(input, HPCLANG_INPUT_INSERT, plan->codepoint) < 0 ||
            hpclang_text_input_edit(input, HPCLANG_INPUT_INSERT, plan->closing) < 0)
            return -1;
        return hpclang_text_input_edit(input, HPCLANG_INPUT_LEFT, 0);
    }
    return hpclang_text_input_edit(input, plan->action, plan->codepoint);
}

int hpclang_text_input_event(struct hpclang_text_input *input, struct hpclang_text_mode *mode,
                             const struct hpclang_keyboard_event *event)
{
    if (hpclang_text_input_edit(input, HPCLANG_INPUT_CANCEL, 0) < 0)
        return -1;
    struct hpclang_input_plan plan;
    int result = hpclang_input_plan_event(mode, event, &plan);
    return result <= 0 ? result : hpclang_input_plan_apply(input, &plan);
}
