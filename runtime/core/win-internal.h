#ifndef HPCLANG_WIN_INTERNAL_H
#define HPCLANG_WIN_INTERNAL_H
#include "win-focus.h"
#include <winwidget.h>
extern struct hpclang_win_focus hpclang_win_widgets;
extern unsigned hpclang_win_widget_callbacks;
int hpclang_win_fail(int error);
int hpclang_win_dispatch_active(void);
int hpclang_win_widget_installed(win_widget_t *);
void *hpclang_win_widget_data(win_widget_t *, void (*destroy)(win_widget_t *));
#endif
