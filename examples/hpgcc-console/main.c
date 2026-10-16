#include <hpconio.h>
#include <hpstdio.h>
#include <hpgraphics.h>
#include <hpkeyb49.h>
#include <errno.h>

int print_status(void);

int main(void)
{
    _clear_screen();
    if (puts("HPGCC console") == EOF || print_status() < 0)
        return 1;
    hpg_set_color(HPG_STDSCREEN, HPG_COLOR_BLACK);
    hpg_draw_line(0, 20, 100, 20);
    gotoxy(0, 4);
    if (printf("Cells: %d x %d\nName: ", screen_width(), screen_height()) < 0)
        return 1;
    char name[16];
    if (!hpclang_hpgcc_gets(name, sizeof name))
        return errno == ECANCELED ? 0 : 1;
    if (printf("Hello, %s\nCount: ", name) < 0)
        return 1;
    errno = 0;
    int count = readint();
    if (errno || ferror(stdin) || feof(stdin))
        return errno == ECANCELED ? 0 : 1;
    if (printf("Value: ") < 0)
        return 1;
    errno = 0;
    double value = readdouble();
    if (errno || ferror(stdin) || feof(stdin))
        return errno == ECANCELED ? 0 : 1;
    if (printf("%d x %g = %g\n", count, value, count * value) < 0 ||
        puts("Press a key to return") == EOF)
        return 1;
    return keyb_getkey(1) < 0 ? 1 : 0;
}
