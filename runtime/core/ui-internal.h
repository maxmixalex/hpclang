#ifndef HPCLANG_UI_INTERNAL_H
#define HPCLANG_UI_INTERNAL_H

#include <hpclang/ui.h>

#define UI_ROOT UINT32_MAX

struct node
{
    struct hpclang_ui_node view;
    uint32_t parent_index;
    uint32_t layout_parent;
    struct hpclang_ui_metrics metrics;
    int32_t content_width, content_height;
    uint32_t children, weights;
    int32_t offset, extra;
};

struct frame
{
    struct node *nodes;
    char *text;
    uint32_t count;
    size_t text_used;
};

struct hpclang_ui
{
    struct hpclang_ui_config capacity;
    struct frame frames[2];
    uint32_t parent, epoch, last_id, pending_id, focused;
    unsigned current;
    int building, error, measuring, arranged, focus_cleared;
    char *edit_text;
    uint32_t edit_pending;
    size_t edit_cursor, edit_anchor;
};

int hpclang_ui_fail(struct hpclang_ui *ui, int error);
void hpclang_ui_reconcile_focus(struct hpclang_ui *ui);
int hpclang_ui_add(struct hpclang_ui *ui, uint32_t key, enum hpclang_ui_kind kind, uint16_t style,
                   const char *text);

#endif
