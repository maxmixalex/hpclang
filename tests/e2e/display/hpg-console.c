#include <hpconio.h>
#include <hpgraphics.h>
#include <hpclang/display.h>

unsigned hpclang_hpg_console(int (*checkpoint)(unsigned))
{
    unsigned failure = 1;
    _clear_screen();
    if (screen_width() != 32 || screen_height() < 10 || getx() != 0 || gety() != 0)
        goto done;
    hpg_set_font(HPG_STDSCREEN, hpg_get_minifont());
    hpg_set_color(HPG_STDSCREEN, HPG_COLOR_BLACK);
    hpg_draw_text("AB", 0, 18);
    hpg_draw_line(100, 8, 100, 12);
    if (printf("A%c%s", 'B', "junk") != 6)
        goto done;
    gotoxy(2, 0);
    clreol();
    failure = 2;
    if (getx() != 2 || gety() != 0 || checkpoint(120))
        goto done;
    gotoxy(0, screen_height() - 1);
    if (putchar('\n') == EOF)
        goto done;
    failure = 3;
    if (getx() != 0 || checkpoint(121))
        goto done;
    hpg_set_mode_gray4(1);
    hpg_clear();
    home();
    if (printf("%s", "AB") != 2)
        goto done;
    hpg_flip();
    failure = 4;
    if (checkpoint(122))
        goto done;
    failure = 0;
done:
    hpg_cleanup();
    return failure;
}
