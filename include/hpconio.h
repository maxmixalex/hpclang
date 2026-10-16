#ifndef HPCLANG_HPGCC_HPCONIO_H
#define HPCLANG_HPGCC_HPCONIO_H
#include <stdio.h>
#include <hpstring.h>
#ifdef __cplusplus
extern "C"
{
#endif
    /* Including this compatibility header selects lazy calculator-byte output for
     * unbound standard streams. Explicit hpclang_stdio_bind owners take priority.
     * HPG's mode/buffering policy is shared; its double-buffered modes require flip.
     * getchar blocks for a model-native text key without echo: Enter returns LF,
     * deletion returns BS, ON reports EOF/ferror with ECANCELED. Alpha/Shift state
     * persists across reads; physical event timings/queue are not reset.
     * Line editing and numeric-format policy are not supplied by this group yet. */
    int hpclang_hpgcc_console_enable(void);
    void home(void);
    int getx(void);
    int gety(void);
    void gotoxy(int column, int row);
    void clreol(void);
    void _clear_screen(void);
    int screen_width(void);
    int screen_height(void);
    int get_tabsize(void);
    void set_tabsize(int width);
    void beep(void);
#ifdef __cplusplus
}
#endif
#ifndef HPCLANG_HPCONIO_NO_AUTOINIT
static void __attribute__((constructor)) hpclang_hpgcc_console_autoinit(void)
{
    (void)hpclang_hpgcc_console_enable();
}
#endif
#endif
