#ifndef HPCLANG_WINTEXT_H
#define HPCLANG_WINTEXT_H
#include <winwidget.h>
#ifdef __cplusplus
extern "C"
{
#endif
    /* Byte-oriented historical label/entry controls, using HPG font encoding.
     * Entry length is the keyboard insertion limit. Programmatic replacement may
     * exceed it without changing that limit or automatically resizing the widget.
     * Returned text is borrowed until the next window call; modifications may not
     * extend its current length. Use set_text for growth (including aliased input).
     * Failed allocation preserves the old text and cursor. Negative limits/null
     * strings and geometry overflow use the window error latch.
     */
    win_widget_t *win_text_new_label(char *text);
    win_widget_t *win_text_new_entry(int length);
    void win_text_set_text(win_widget_t *, char *text);
    char *win_text_get_text(win_widget_t *);
#ifdef __cplusplus
}
#endif
#endif
