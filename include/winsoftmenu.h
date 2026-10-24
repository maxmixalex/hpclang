#ifndef HPCLANG_WINSOFTMENU_H
#define HPCLANG_WINSOFTMENU_H
#include <winwidget.h>
#define WIN_SOFTMENU_MAX_ITEMS 30
#ifdef __cplusplus
extern "C"
{
#endif
    typedef struct
    {
        unsigned lshift : 2;
        unsigned rshift : 2;
        void *extra_data;
    } win_softmenu_event_t;
    /* Labels are copied, truncated to five bytes; extra_data is borrowed.
     * Events are synchronous and their payload is valid only during dispatch.
     * Six entries per page; NXT advances, left-shift NXT goes back. Alpha and
     * right-shift NXT fall through. Geometry follows the screen's bottom row.
     * Indices must be 0..29. Full add returns -1 without latching an error.
     */
    win_widget_t *win_softmenu_new(void);
    void win_softmenu_set_item(win_widget_t *, int, char *, int, void *);
    int win_softmenu_add_item(win_widget_t *, char *, int, void *);
    void win_softmenu_insert_item(win_widget_t *, int, char *, int, void *);
    void win_softmenu_delete_item(win_widget_t *, int, int);
    void win_softmenu_set_colors(win_widget_t *, int, unsigned char, unsigned char);
    void win_softmenu_set_disable_colors(win_widget_t *, int, unsigned char, unsigned char);
    void win_softmenu_set_enabled(win_widget_t *, int, int);
#ifdef __cplusplus
}
#endif
#endif
