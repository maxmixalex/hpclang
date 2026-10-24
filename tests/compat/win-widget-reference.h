#ifndef HPCLANG_WIN_WIDGET_REFERENCE_H
#define HPCLANG_WIN_WIDGET_REFERENCE_H
#include <wincore.h>
#include <stdlib.h>
#include <string.h>
/* Host-only structural glue for compiling caller-supplied original functions.
 * No original implementation or calculator ABI is provided here. */
struct win_widget
{
    void (*install)(win_widget_t *), (*deinstall)(win_widget_t *), (*destroy)(win_widget_t *);
    void (*pref_size)(win_widget_t *, int *, int *);
    void (*focus_lost)(win_widget_t *), (*focus_gained)(win_widget_t *);
    int x, y, w, h;
    unsigned char bgcolor, fgcolor;
    hpg_font_t *font;
    unsigned focusable : 1;
    unsigned transparent : 1;
    unsigned focus : 1;
    void *widget_data;
};
struct win_focus_node
{
    win_widget_t *widget;
    struct win_focus_node *next, *prev;
};
extern struct win_focus_node *_win_focus_list;
#endif
