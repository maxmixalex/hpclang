#include <hpconio.h>
#include <hpgraphics.h>
#include <hpclang/display.h>
#include <limits.h>
#include <errno.h>
#include <hpclang/device.h>
#include <hpclang/keyboard.h>
#include <hpclang/time.h>

extern volatile uint32_t hpclang_e2e_result;
static int key(unsigned scan, unsigned down)
{
    unsigned argument = scan | (down << 8);
    hpclang_e2e_result = 0x4b490000u | argument;
    uint64_t start = hpclang_time_monotonic_us();
    while (hpclang_e2e_result != (0x4b4a0000u | argument))
        if (hpclang_time_monotonic_us() - start > 2000000u)
            return -1;
    return 0;
}
static int input(void)
{
    hpclang_keyboard_events_set_timing(20000, 0, 0);
    hpclang_keyboard_events_reset();
    const unsigned scans[] = {24, 0, 6};
    const int expected[] = {'7', '\b', '\n'};
    for (unsigned i = 0; i < 3; ++i) {
        if (key(scans[i], 1) || getchar() != expected[i] || key(scans[i], 0))
            return -1;
        hpclang_delay_us(30000);
    }
    int hp39 = hpclang_device_info()->model <= HPCLANG_MODEL_HP40GS;
    unsigned alpha = hp39 ? 58 : 57, letter = hp39 ? 28 : 35;
    if (key(alpha, 1) || key(letter, 1) || getchar() != 'A' || key(letter, 0) || key(alpha, 0))
        return -1;
    hpclang_delay_us(30000);
    if (key(56, 1))
        return -1;
    errno = 0;
    if (getchar() != EOF || !ferror(stdin) || feof(stdin) || errno != ECANCELED || key(56, 0))
        return -1;
    clearerr(stdin);
    return 0;
}

unsigned hpclang_hpg_console(int (*checkpoint)(unsigned))
{
    unsigned failure = 1;
    if (strcmp(ints(INT_MIN, 10), "-2147483648") || strcmp(uints(UINT_MAX, 36), "1z141z3") ||
        strcmp(ints(-255, 16), "-ff"))
        goto done;
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
    failure = 5;
    if (input())
        goto done;
    failure = 0;
done:
    hpg_cleanup();
    return failure;
}
