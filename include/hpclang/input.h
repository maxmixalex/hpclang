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
    /* Convert the entire current line without changing text, cursor or edit
     * state. Call after ACCEPT, or while editing to validate a proposed value.
     * Return 0 on success (preserving errno), -1/EINVAL for invalid arguments
     * or incomplete/invalid syntax, -1/ERANGE for a value outside the range.
     * Errors leave the output unchanged. No whitespace, separators, expressions,
     * hex, NaN or infinity are accepted. Signs are optional leading '+'/'-'.
     * int64: one or more ASCII decimal digits, with exact signed 64-bit bounds.
     * double: decimal digits with an optional '.', at least one mantissa digit,
     * and optional e/E followed by a signed decimal exponent. Uses the SDK's
     * C-locale strtod rounding; finite subnormals and signed zero are supported.
     * Overflow and a nonzero mantissa rounded to zero are ERANGE. These calls
     * do not accept/cancel the editor or select any keyboard/text-plane policy. */
    int hpclang_text_input_parse_int64(const struct hpclang_text_input *input, int64_t *value);
    int hpclang_text_input_parse_double(const struct hpclang_text_input *input, double *value);
#ifdef __cplusplus
}
#endif
#endif
