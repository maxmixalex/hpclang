#ifndef HPCLANG_HPGCC_HPCONIO_H
#define HPCLANG_HPGCC_HPCONIO_H
#include <stdio.h>
#ifdef __cplusplus
extern "C"
{
#endif
    /* Including this compatibility header selects lazy calculator-byte output for
     * unbound standard streams. Explicit hpclang_stdio_bind owners take priority.
     * HPG's mode/buffering policy is shared; its double-buffered modes require flip.
     * Input/numeric-format adapters are not supplied by this output group yet. */
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
