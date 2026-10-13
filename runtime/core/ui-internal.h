#ifndef HPCLANG_UI_INTERNAL_H
#define HPCLANG_UI_INTERNAL_H

#include <hpclang/ui.h>

#define UI_ROOT UINT32_MAX

struct node
{
    struct hpclang_ui_node view;
    uint32_t parent_index;
    /* Preferred sizes are consumed before this node distributes space to its
     * children. Padding/gap/grow remain live in the trailing metrics fields. */
    union
    {
        struct hpclang_ui_metrics metrics;
        struct
        {
            int32_t extra, offset;
        };
    };
    union
    {
        int32_t content_width;
        int32_t base_y;
    };
    int32_t content_height;
    union
    {
        uint32_t children;
        int32_t next_scroll;
    };
    uint32_t weights;
};

struct frame
{
    struct node *nodes;
    char *text;
    uint32_t count;
    size_t text_used;
    struct hpclang_ui_rect viewport;
    int laid_out;
};

struct hpclang_ui
{
    struct hpclang_ui_config capacity;
    struct frame frames[2];
    uint32_t parent, epoch, last_id, pending_id, focused;
    unsigned current;
    int building, error, measuring, arranged, focus_cleared;
    char *edit_text;
    uint32_t proposal_owner;
    size_t edit_cursor, edit_anchor;
    int painting, dirty;
    int64_t damage_left, damage_top, damage_right, damage_bottom;
    struct ui_effect *effects;
    uint32_t effect_serial, effect_cursor;
    int dispatching, closed, registered;
    struct hpclang_ui *exit_next;
};

struct ui_effect
{
    struct hpclang_ui_effect_ops ops;
    void *context;
    struct hpclang_ui_action action;
    uint32_t token, owner;
    int state;
};

int hpclang_ui_fail(struct hpclang_ui *ui, int error);
void hpclang_ui_reconcile_focus(struct hpclang_ui *ui);
int hpclang_ui_add(struct hpclang_ui *ui, uint32_t key, enum hpclang_ui_kind kind, uint16_t style,
                   const char *text);
void hpclang_ui_damage(struct hpclang_ui *ui, struct hpclang_ui_rect rect);
void hpclang_ui_damage_commit(struct hpclang_ui *ui, uint32_t old_focus);
void hpclang_ui_damage_focus(struct hpclang_ui *ui, uint32_t old_focus);
void hpclang_ui_effect_reconcile(struct hpclang_ui *ui);
int hpclang_ui_scroll_reveal(struct hpclang_ui *ui, struct frame *frame, uint32_t focus,
                             int damage);

#endif
