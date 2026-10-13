#ifndef HPCLANG_UI_H
#define HPCLANG_UI_H

#include <stddef.h>
#include <stdint.h>
#include <hpclang/input.h>

#ifdef __cplusplus
extern "C"
{
#endif

    struct hpclang_ui;

    struct hpclang_ui_rect
    {
        int32_t x, y, width, height;
    };

    struct hpclang_ui_metrics
    {
        int32_t width, height;
        uint16_t padding_x, padding_y, gap, grow;
    };

    typedef int (*hpclang_ui_canvas_paint_fn)(void *paint_context, const void *state,
                                              struct hpclang_ui_rect bounds,
                                              struct hpclang_ui_rect clip);

    enum hpclang_ui_kind
    {
        HPCLANG_UI_COMPONENT = 1,
        HPCLANG_UI_KEY,
        HPCLANG_UI_COLUMN,
        HPCLANG_UI_ROW,
        HPCLANG_UI_TEXT,
        HPCLANG_UI_BUTTON,
        HPCLANG_UI_INPUT,
        HPCLANG_UI_CANVAS
    };

    struct hpclang_ui_config
    {
        uint32_t nodes;
        size_t text_bytes;
    };

    struct hpclang_ui_action
    {
        uint32_t type, arg;
    };

    enum hpclang_ui_event_kind
    {
        HPCLANG_UI_ACTIVATE = 1,
        HPCLANG_UI_CHANGE,
        HPCLANG_UI_SELECTION,
        HPCLANG_UI_ACCEPT,
        HPCLANG_UI_CANCEL
    };

    struct hpclang_ui_event
    {
        uint32_t source;
        struct hpclang_ui_action action;
        enum hpclang_ui_event_kind kind;
        const char *text;
        size_t cursor, anchor;
    };

    /* Committed metadata; text is owned by the context. Pointers returned by
     * node_at remain valid until the next successful commit or reinitialization.
     * id is a lifetime token, parent is another token (zero denotes the root).
     * Keys identify siblings; changing a node's kind gives it a new lifetime. */
    struct hpclang_ui_node
    {
        uint32_t id, parent, key;
        enum hpclang_ui_kind kind;
        uint16_t style;
        const char *text;
        struct hpclang_ui_rect bounds, clip;
        struct hpclang_ui_action action;
        int enabled;
        size_t capacity, cursor, anchor;
        hpclang_ui_canvas_paint_fn paint;
        const void *canvas_state;
        uint32_t revision;
        int32_t preferred_height;
    };

    /* Allocate once, or supply suitably aligned static storage. Initialization and
     * frames never allocate. Size includes two fixed node/text arenas plus one
     * text-budget-sized edit buffer. Failed declarations preserve the committed
     * tree. Zero means invalid size.
     * Storage must have max_align_t alignment and live as long as the context. */
    size_t hpclang_ui_storage_size(struct hpclang_ui_config config);
    struct hpclang_ui *hpclang_ui_init(void *storage, size_t bytes,
                                       struct hpclang_ui_config config);
    int hpclang_ui_begin(struct hpclang_ui *ui);
    int hpclang_ui_commit(struct hpclang_ui *ui);
    void hpclang_ui_abort(struct hpclang_ui *ui);
    size_t hpclang_ui_node_count(const struct hpclang_ui *ui);
    const struct hpclang_ui_node *hpclang_ui_node_at(const struct hpclang_ui *ui, size_t index);

    struct hpclang_ui_scope
    {
        struct hpclang_ui *ui;
        uint32_t epoch, node;
        int open;
    };

    struct hpclang_ui_scope hpclang_ui_scope_begin(struct hpclang_ui *ui, uint32_t key,
                                                   enum hpclang_ui_kind kind, uint16_t style);
    void hpclang_ui_scope_close(struct hpclang_ui_scope *scope);
    int hpclang_ui_text(struct hpclang_ui *ui, uint32_t key, uint16_t style, const char *text);
    int hpclang_ui_button(struct hpclang_ui *ui, uint32_t key, uint16_t style, const char *text,
                          struct hpclang_ui_action action, int enabled);
    /* State is borrowed through painting/removal; update revision when mutable
     * state changes. Callback/state addresses are not dereferenced during
     * declaration. Bounds are layout-owned; painting must honor clip. */
    int hpclang_ui_canvas(struct hpclang_ui *ui, uint32_t key, uint16_t style,
                          hpclang_ui_canvas_paint_fn paint, const void *state, uint32_t revision,
                          int32_t preferred_height);
    /* Single-line controlled UTF-8 input. capacity includes NUL and must fit the
     * context's text budget. Selection offsets are UTF-8 byte boundaries.
     * Edit emits a proposal without changing displayed/model text. Commit a new
     * declaration to accept, transform or reject it before requesting another
     * edit. Failed/aborted declarations leave that proposal outstanding. Event
     * text belongs to ui and lasts until the next edit call or reinitialization.
     * Cursor-only navigation emits SELECTION; it changes no application value. */
    int hpclang_ui_text_input(struct hpclang_ui *ui, uint32_t key, uint16_t style,
                              const char *value, size_t capacity, struct hpclang_ui_action action,
                              int enabled);
    int hpclang_ui_select(struct hpclang_ui *ui, size_t anchor, size_t cursor);
    int hpclang_ui_edit(struct hpclang_ui *ui, enum hpclang_input_action action, uint32_t codepoint,
                        struct hpclang_ui_event *event);
    /* Feed all physical events to the existing model-aware text mode. Up/down
     * traverse controls; input left/right edit selection, button left/right
     * traverse. Modifier state advances on ENOSPC. EBUSY for an outstanding
     * proposal consumes nothing: commit its response and retry that event.
     * Returns 1/application event, 0/consumed/no action, or -1/errno. */
    int hpclang_ui_key(struct hpclang_ui *ui, struct hpclang_text_mode *mode,
                       const struct hpclang_keyboard_event *key, struct hpclang_ui_event *event);
    /* Focus follows stable lifetime tokens across reordering. Removed/disabled
     * focus falls to the next surviving old neighbor, then the previous one,
     * then the first eligible new node. Step traverses declaration order and
     * wraps; direction must be -1 or +1. id=0 explicitly clears focus. Calls
     * that change focus or dispatch an action require no active declaration.
     * Activation returns 1/event, 0/no eligible focus, -1/errno. No model update
     * or callback occurs; the application decides how to handle the action. */
    uint32_t hpclang_ui_focused(const struct hpclang_ui *ui);
    int hpclang_ui_focus(struct hpclang_ui *ui, uint32_t id);
    int hpclang_ui_focus_step(struct hpclang_ui *ui, int direction);
    int hpclang_ui_activate(struct hpclang_ui *ui, struct hpclang_ui_event *event);

    /* Arrange a balanced candidate before commit. The callback supplies leaf
     * preferred sizes and container padding/gaps; grow distributes spare space
     * on the parent's main axis. Children stretch on the cross axis. Oversized
     * content keeps its preferred size and is clipped, never silently shrunk.
     * COMPONENT/KEY are transparent. Root children form a column. The callback
     * must not reenter or mutate ui. Return 0, or -1 with errno. Invalid metrics
     * or coordinate overflow reject the frame, retaining the accepted tree. */
    typedef int (*hpclang_ui_measure_fn)(void *context, const struct hpclang_ui_node *node,
                                         struct hpclang_ui_metrics *metrics);
    int hpclang_ui_layout(struct hpclang_ui *ui, struct hpclang_ui_rect viewport,
                          hpclang_ui_measure_fn measure, void *context);

    struct hpclang_ui_painter
    {
        /* begin clears/prepares the damaged region. node paints ordinary nodes
         * in declaration order. Canvas callbacks receive canvas_context.
         * After a successful begin, end is always called, even on draw failure;
         * completed=0 means release resources without claiming a good frame. */
        int (*begin)(void *context, struct hpclang_ui_rect damage);
        int (*node)(void *context, const struct hpclang_ui_node *node, struct hpclang_ui_rect clip,
                    int focused);
        int (*end)(void *context, int completed);
        void *context;
        void *canvas_context;
    };
    /* Commit, focus and selection changes accumulate bounded damage. Explicit
     * invalidation covers exposure/theme changes. Paint clips to the complete
     * target surface, retries all damage after failure, and does nothing when
     * clean. Returns 1/painted, 0/clean or off-surface, -1/errno. Callbacks must
     * not mutate/reenter ui. No display acquisition or publication is implicit. */
    int hpclang_ui_invalidate(struct hpclang_ui *ui, struct hpclang_ui_rect rect);
    int hpclang_ui_paint(struct hpclang_ui *ui, struct hpclang_ui_rect surface,
                         const struct hpclang_ui_painter *painter);

#ifdef __cplusplus
}
#endif

/* Clang/GCC cleanup closes on fall-through, break, continue, return and outward
 * goto. longjmp across declarations is unsupported; explicitly abort a frame
 * after nonlocal recovery. Manual begin/close is available without the macros.
 * Declaration failures are sticky: commit rejects and discards that frame. */
#define HPCLANG_UI_SCOPE(expression)                                                               \
    for (struct hpclang_ui_scope _hpclang_scope                                                    \
         __attribute__((cleanup(hpclang_ui_scope_close))) = (expression);                          \
         _hpclang_scope.open; hpclang_ui_scope_close(&_hpclang_scope))
#define HPCLANG_UI_WITH(ui, key, kind, style)                                                      \
    HPCLANG_UI_SCOPE(hpclang_ui_scope_begin((ui), (key), (kind), (style)))

#endif
