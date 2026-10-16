#include <hpconio.h>
#include <hpstdio.h>
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
static unsigned scripted_read, scripted_index;
int __real_getc(FILE *stream);
int __wrap_getc(FILE *stream)
{
    if (!scripted_read || stream != stdin)
        return __real_getc(stream);
    /* Only schedule electrical switches; decoding, stdio and editing are real.
     * 123 fills a three-byte line, 9 must be rejected, BS/4 correct it to 124. */
    static const unsigned scans[] = {26, 19, 12, 10, 0, 25, 6};
    if (scripted_index >= sizeof scans / sizeof scans[0]) {
        errno = EIO;
        return EOF;
    }
    unsigned scan = scans[scripted_index++];
    if (key(scan, 1))
        return EOF;
    int result = __real_getc(stream);
    if (key(scan, 0))
        return EOF;
    hpclang_delay_us(30000);
    return result;
}
static int edited_input(int (*checkpoint)(unsigned))
{
    _clear_screen();
    hpg_draw_line(100, 20, 100, 24);
    gotoxy(screen_width() - 2, screen_height() - 1);
    hpclang_keyboard_events_reset();
    struct
    {
        char before;
        char text[4];
        char after;
    } buffer = {'x', "", 'y'};
    scripted_read = 1;
    char *result = hpclang_hpgcc_gets(buffer.text, sizeof buffer.text);
    scripted_read = 0;
    if (result != buffer.text || strcmp(buffer.text, "124") || buffer.before != 'x' ||
        buffer.after != 'y' || scripted_index != 7 || getx() || gety() != screen_height() - 1 ||
        checkpoint(123))
        return -1;
    /* Independent HPG drawing builds the expected final frame after two scrolls. */
    hpg_clear();
    hpg_draw_text("12", (screen_width() - 2) * 4, (screen_height() - 3) * 6);
    hpg_draw_text("4", 0, (screen_height() - 2) * 6);
    hpg_draw_line(100, 8, 100, 12);
    hpg_flip();
    return checkpoint(124);
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

static int numeric_input(void)
{
    home();
    scripted_index = 0;
    scripted_read = 1;
    errno = 0;
    int integer = readint();
    scripted_read = 0;
    if (integer != 1234 || errno || scripted_index != 7)
        return -1;
    scripted_index = 0;
    scripted_read = 1;
    double real = readdouble();
    scripted_read = 0;
    if (real != 1234.0 || errno || scripted_index != 7)
        return -1;
    for (unsigned kind = 0; kind < 4; ++kind) {
        scripted_index = 0;
        scripted_read = 1;
        unsigned long long value = kind == 0   ? readuint()
                                   : kind == 1 ? readhex()
                                   : kind == 2 ? (unsigned long long)readlonglong()
                                               : readulonglong();
        scripted_read = 0;
        if (value != (kind == 1 ? 0x1234u : 1234u) || errno || scripted_index != 7)
            return -1;
    }
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
    failure = 6;
    if (edited_input(checkpoint))
        goto done;
    failure = 7;
    if (numeric_input())
        goto done;
    failure = 0;
done:
    hpg_cleanup();
    return failure;
}
