#ifndef HPCLANG_INPUT_H
#define HPCLANG_INPUT_H
#include <stddef.h>
#include <stdint.h>
#include <hpclang/keyboard.h>
#include <hpclang/target.h>
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

    /* Look up the printed Latin Alpha letter (A-Z/a-z) or Alpha-space for a
     * physical key on an explicit model. lowercase must be 0 or 1. Returns
     * 1/mapped, 0/no letter, -1/EINVAL for invalid arguments, -1/ENOSYS for an
     * unknown model. Output changes only on 1; success preserves errno.
     * Pure lookup: no event polling, held-modifier inference, sticky Alpha,
     * punctuation/command planes or Stock UI state. The caller chooses when an
     * Alpha plane applies. HP39 Alpha-Plus maps to space. Enter has no letter. */
    int hpclang_text_alpha_lookup(uint32_t model, uint16_t key, int lowercase, uint32_t *codepoint);

    /* Text planes are application policy, separate from physical held modifiers.
     * A shifted command or symbol plane is not a Latin-letter lookup. */
    enum hpclang_text_plane
    {
        HPCLANG_TEXT_PLAIN = 0,
        HPCLANG_TEXT_LEFT_SHIFT,
        HPCLANG_TEXT_RIGHT_SHIFT,
        HPCLANG_TEXT_ALPHA_UPPER,
        HPCLANG_TEXT_ALPHA_LOWER,
        HPCLANG_TEXT_ALPHA_SYMBOL
    };
    enum hpclang_text_lock
    {
        HPCLANG_TEXT_UNLOCKED = 0,
        HPCLANG_TEXT_LOCK_UPPER,
        HPCLANG_TEXT_LOCK_LOWER
    };
    /* Caller-owned state; fields are private. Initialize per input session and
     * feed its complete ordered event stream, including modifier UP events.
     * No global keyboard state, allocation, polling or Stock UI is changed. */
    struct hpclang_text_mode
    {
        uint32_t model;
        uint32_t alpha;
        uint32_t lowercase;
        uint32_t shift;
        uint32_t held;
        uint32_t used;
        uint32_t suppressed;
        uint32_t suspended;
    };
    struct hpclang_text_mode_status
    {
        enum hpclang_text_plane plane;
        enum hpclang_text_lock lock;
    };
    /* Unknown model: ENOSYS. Invalid arguments: EINVAL. Errors change nothing;
     * all successful mode operations preserve errno. */
    int hpclang_text_mode_init(struct hpclang_text_mode *mode, uint32_t model);
    /* Explicit application/menu action, including HP39 Notes-style A..Z lock.
     * Clears pending one-shot shifts and Alpha suspension. Held modifiers used
     * to select this action are suppressed until released. */
    int hpclang_text_mode_set_lock(struct hpclang_text_mode *mode, enum hpclang_text_lock lock);
    int hpclang_text_mode_get(const struct hpclang_text_mode *mode,
                              struct hpclang_text_mode_status *status);
    /* Return 1 with the selected plane for a non-modifier DOWN/REPEAT event;
     * 0 for consumed modifiers/UP; -1/EINVAL for invalid input. Output changes
     * only on 1. The original physical event is never changed. Shift+Alpha
     * outside classic Alpha mode is a shifted command and also returns 1.
     * One-shot state is consumed by the key even if the application cannot
     * translate/insert it. This layer selects planes; it does not insert text,
     * map symbols/commands, or decide how Enter/ON changes an editor session. */
    int hpclang_text_mode_event(struct hpclang_text_mode *mode,
                                const struct hpclang_keyboard_event *event,
                                enum hpclang_text_plane *plane);

    /* Synchronous bounded line session. The caller owns input/buffer and context.
     * next returns 1/event, 0/idle or -1/errno; it may wait, or poll the physical
     * event service. Idle calls do not redraw. No global keyboard reset/timing,
     * display acquisition, allocation or implicit stdio binding is performed.
     * edit optionally replaces text_input_key (e.g. model-specific Alpha policy),
     * returning the same input_result or -1/errno. ENOSPC is recoverable.
     * accept optionally validates the proposed line: 0 accepts, 1 rejects with
     * errno, -1 aborts. Rejection preserves the editor for correction.
     * update is optional: initial notification and each non-idle event, with
     * result EDITING/ACCEPTED/CANCELLED and error 0 or the recoverable errno.
     * It may render/present; return 0 or -1/errno to abort. It must not mutate
     * input. All callbacks must not reenter this session or free its state.
     * Callback descriptors are copied; borrowed context stays live until return.
     * Run returns ACCEPTED/CANCELLED or -1/errno, retaining text on every path.
     * Missing callback errno becomes EIO; invalid callback results become EINVAL.
     * Success/cancel preserve entry errno. next is required. */
    struct hpclang_text_input_callbacks
    {
        int (*next)(void *context, struct hpclang_keyboard_event *event);
        int (*edit)(void *context, struct hpclang_text_input *input,
                    const struct hpclang_keyboard_event *event);
        int (*accept)(void *context, const struct hpclang_text_input *input);
        int (*update)(void *context, const struct hpclang_text_input *input, int result, int error);
        void *context;
    };
    int hpclang_text_input_run(struct hpclang_text_input *input,
                               const struct hpclang_text_input_callbacks *callbacks);
#ifdef __cplusplus
}
#endif
#endif
