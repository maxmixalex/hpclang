#include <hpconio.h>
#include <hpgraphics.h>
#include <hpkeyb49.h>

int print_status(void);

int main(void)
{
    _clear_screen();
    if (puts("HPGCC console") == EOF || print_status() < 0)
        return 1;
    hpg_set_color(HPG_STDSCREEN, HPG_COLOR_BLACK);
    hpg_draw_line(0, 20, 100, 20);
    gotoxy(0, 4);
    if (printf("Cells: %d x %d\n", screen_width(), screen_height()) < 0 ||
        puts("Press a key to return") == EOF)
        return 1;
    return keyb_getkey(1) < 0 ? 1 : 0;
}
