#ifndef HPCLANG_INPUT_H
#define HPCLANG_INPUT_H
#include <stddef.h>
#include <stdint.h>
#include <hpclang/keyboard.h>
#ifdef __cplusplus
extern "C"
{
#endif

    /* Caller owns both state and buffer. Initialize once; do not mutate fields or
     * buffer while editing. Length/cursor are byte offsets at UTF-8 boundaries.
     * Capacity includes the trailing NUL. No allocation, blocking, or global UI. */
    struct hpclang_text_input
    {
        char *buffer;
        size_t capacity;
        size_t length;
        size_t cursor;
    };
    enum hpclang_input_action
    {
        HPCLANG_INPUT_INSERT = 1,
        HPCLANG_INPUT_LEFT,
        HPCLANG_INPUT_RIGHT,
        HPCLANG_INPUT_HOME,
        HPCLANG_INPUT_END,
        HPCLANG_INPUT_BACKSPACE,
        HPCLANG_INPUT_DELETE,
        HPCLANG_INPUT_ACCEPT,
        HPCLANG_INPUT_CANCEL
    };
    enum hpclang_input_result
    {
        HPCLANG_INPUT_EDITING = 0,
        HPCLANG_INPUT_ACCEPTED = 1,
        HPCLANG_INPUT_CANCELLED = 2
    };
    /* Initialize an empty line. Invalid arguments leave state/buffer untouched. */
    int hpclang_text_input_init(struct hpclang_text_input *input, char *buffer, size_t capacity);
    /* Insert one printable Unicode scalar, or apply an editing action. Returns an
     * input_result, or -1/EINVAL (invalid argument/scalar/action), -1/ENOSPC (full).
     * Errors change nothing. Accept/cancel preserve the working text; the caller
     * decides when to stop and whether to retain it. No normalization/grapheme
     * composition: cursor/delete operate on Unicode scalar values. */
    int hpclang_text_input_edit(struct hpclang_text_input *input, enum hpclang_input_action action,
                                uint32_t codepoint);
    /* Convenience event mapping: arrows, Enter, ON/cancel, family backspace,
     * unmodified digits, arithmetic punctuation and space. UP events are ignored.
     * Alpha/Shift text translation remains application/adapter policy: insert the
     * resulting scalar through text_input_edit(). No keyboard state is consumed. */
    int hpclang_text_input_key(struct hpclang_text_input *input,
                               const struct hpclang_keyboard_event *event);
#ifdef __cplusplus
}
#endif
#endif
