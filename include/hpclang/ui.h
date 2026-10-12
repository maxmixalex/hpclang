#ifndef HPCLANG_UI_H
#define HPCLANG_UI_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    struct hpclang_ui;

    enum hpclang_ui_kind
    {
        HPCLANG_UI_COMPONENT = 1,
        HPCLANG_UI_KEY,
        HPCLANG_UI_COLUMN,
        HPCLANG_UI_ROW,
        HPCLANG_UI_TEXT
    };

    struct hpclang_ui_config
    {
        uint32_t nodes;
        size_t text_bytes;
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
    };

    /* Allocate once, or supply suitably aligned static storage. Initialization and
     * frames never allocate. Size includes two fixed node/text arenas so a failed
     * declaration cannot damage the last committed tree. Zero means invalid size.
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
