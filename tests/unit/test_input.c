#include <assert.h>
#include <errno.h>
#include <string.h>
#include <hpclang/input.h>

int main(void)
{
    char guarded[10];
    memset(guarded, 'X', sizeof(guarded));
    struct hpclang_text_input input;
    assert(hpclang_text_input_init(&input, guarded + 1, 8) == 0);
    assert(hpclang_text_input_edit(&input, HPCLANG_INPUT_INSERT, 'A') == 0);
    assert(hpclang_text_input_edit(&input, HPCLANG_INPUT_INSERT, 0x03a9) == 0);
    assert(hpclang_text_input_edit(&input, HPCLANG_INPUT_INSERT, 0x1f600) == 0);
    assert(input.length == 7 && input.cursor == 7 && guarded[0] == 'X' && guarded[9] == 'X');
    char saved[8];
    memcpy(saved, input.buffer, 8);
    assert(hpclang_text_input_edit(&input, HPCLANG_INPUT_INSERT, 'B') == -1 && errno == ENOSPC);
    assert(memcmp(saved, input.buffer, 8) == 0 && input.cursor == 7);
    assert(hpclang_text_input_edit(&input, HPCLANG_INPUT_LEFT, 0) == 0 && input.cursor == 3);
    assert(hpclang_text_input_edit(&input, HPCLANG_INPUT_BACKSPACE, 0) == 0 && input.cursor == 1);
    assert(input.length == 5 && (unsigned char)input.buffer[1] == 0xf0);
    assert(hpclang_text_input_edit(&input, HPCLANG_INPUT_DELETE, 0) == 0 && input.length == 1);
    assert(strcmp(input.buffer, "A") == 0);
    assert(hpclang_text_input_edit(&input, HPCLANG_INPUT_HOME, 0) == 0);
    assert(hpclang_text_input_edit(&input, HPCLANG_INPUT_BACKSPACE, 0) == 0 && input.cursor == 0);
    assert(hpclang_text_input_edit(&input, HPCLANG_INPUT_INSERT, 0x4e2d) == 0 && input.cursor == 3);
    assert(hpclang_text_input_edit(&input, HPCLANG_INPUT_RIGHT, 0) == 0 && input.cursor == 4);
    assert(hpclang_text_input_edit(&input, HPCLANG_INPUT_INSERT, 0xd800) == -1 && errno == EINVAL);
    assert(hpclang_text_input_edit(&input, HPCLANG_INPUT_INSERT, '\n') == -1 && errno == EINVAL);
    assert(hpclang_text_input_edit(&input, HPCLANG_INPUT_INSERT, 0x110000) == -1 &&
           errno == EINVAL);
    assert(hpclang_text_input_edit(&input, HPCLANG_INPUT_ACCEPT, 0) == HPCLANG_INPUT_ACCEPTED);
    assert(hpclang_text_input_edit(&input, HPCLANG_INPUT_CANCEL, 0) == HPCLANG_INPUT_CANCELLED);
    assert(input.length == 4);
    assert(hpclang_text_input_init(&input, guarded + 1, 1) == 0);
    assert(hpclang_text_input_edit(&input, HPCLANG_INPUT_INSERT, 'A') == -1 && errno == ENOSPC);
    assert(input.buffer[0] == 0);
    assert(hpclang_text_input_init(&input, guarded + 1, 8) == 0);
    struct hpclang_keyboard_event event = {.type = HPCLANG_KEYBOARD_EVENT_DOWN,
                                           .key = HPCLANG_KEY_7};
    assert(hpclang_text_input_key(&input, &event) == 0 && strcmp(input.buffer, "7") == 0);
    event.type = HPCLANG_KEYBOARD_EVENT_UP;
    assert(hpclang_text_input_key(&input, &event) == 0 && input.length == 1);
    event.type = HPCLANG_KEYBOARD_EVENT_DOWN;
    event.modifiers = HPCLANG_KEYBOARD_MOD_ALPHA;
    assert(hpclang_text_input_key(&input, &event) == 0 && input.length == 1);
    event.key = HPCLANG_KEY_ENTER;
    assert(hpclang_text_input_key(&input, &event) == HPCLANG_INPUT_ACCEPTED);
    event.key = HPCLANG_KEY_ON;
    assert(hpclang_text_input_key(&input, &event) == HPCLANG_INPUT_CANCELLED);
    assert(hpclang_text_input_key(&input, NULL) == -1 && errno == EINVAL);
    assert(hpclang_text_input_init(NULL, guarded, 1) == -1 && errno == EINVAL);
    assert(guarded[0] == 'X' && guarded[9] == 'X');
    return 0;
}
