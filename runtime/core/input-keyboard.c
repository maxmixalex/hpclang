#include <errno.h>
#include <hpclang/input.h>

int hpclang_text_input_event(struct hpclang_text_input *input, struct hpclang_text_mode *mode,
                             const struct hpclang_keyboard_event *event)
{
    /* CANCEL validates the editor without changing its working text/cursor. */
    if (hpclang_text_input_edit(input, HPCLANG_INPUT_CANCEL, 0) < 0)
        return -1;
    enum hpclang_text_plane plane;
    int result = hpclang_text_mode_event(mode, event, &plane);
    if (result <= 0)
        return result;
    switch (event->key) {
    case HPCLANG_KEY_ENTER:
    case HPCLANG_KEY_ON:
    case HPCLANG_KEY_LEFT:
    case HPCLANG_KEY_RIGHT:
    case HPCLANG_KEY_HP39_DEL:
    case HPCLANG_KEY_HP48_BACKSPACE:
    case HPCLANG_KEY_HP4950_BACKSPACE:
        return hpclang_text_input_key(input, event);
    default:
        break;
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
    if (closing) {
        /* Stock inserts delimiter pairs with the cursor inside. Preflight the
         * complete UTF-8 pair so a full buffer cannot retain only its opener. */
        size_t bytes = codepoint == 0x00ab ? 4 : 2;
        if (input->capacity - input->length - 1 < bytes) {
            errno = ENOSPC;
            return -1;
        }
        if (hpclang_text_input_edit(input, HPCLANG_INPUT_INSERT, codepoint) < 0 ||
            hpclang_text_input_edit(input, HPCLANG_INPUT_INSERT, closing) < 0)
            return -1;
        return hpclang_text_input_edit(input, HPCLANG_INPUT_LEFT, 0);
    }
    return hpclang_text_input_edit(input, HPCLANG_INPUT_INSERT, codepoint);
}
