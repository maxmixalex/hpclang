#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <hpclang/device.h>
#include <hpclang/display.h>
#include <hpclang/font.h>
#include <hpclang/keyboard.h>

extern const struct hpclang_font hpclang_system_font;
volatile uint32_t hpclang_e2e_result;

static int prompt(const char *title, const char *first, const char *second)
{
    struct hpclang_surface *screen = hpclang_display_surface();
    hpclang_surface_clear(screen, 0);
    hpclang_surface_draw_text(screen, &hpclang_system_font, 2, 4, title, 255);
    hpclang_surface_draw_text(screen, &hpclang_system_font, 2, 20, first, 255);
    hpclang_surface_draw_text(screen, &hpclang_system_font, 2, 30, second, 255);
    hpclang_surface_draw_text(screen, &hpclang_system_font, 2, 50,
                              HPCLANG_UI_TEXT("ENTER: next   ON: exit"), 255);
    uint16_t selected = HPCLANG_KEY_UNKNOWN;
    for (;;) {
        struct hpclang_keyboard_event event;
        if (!hpclang_keyboard_event_poll(&event))
            continue;
        if (event.type == HPCLANG_KEYBOARD_EVENT_DOWN &&
            (event.key == HPCLANG_KEY_ENTER || event.key == HPCLANG_KEY_ON))
            selected = event.key;
        if (event.type == HPCLANG_KEYBOARD_EVENT_UP && event.key == selected)
            return selected == HPCLANG_KEY_ENTER;
    }
}

static unsigned check_card(void)
{
    if (!prompt(HPCLANG_UI_TEXT("SD CARD CHECK"), HPCLANG_UI_TEXT("Use the SAME card throughout."),
                HPCLANG_UI_TEXT("Creates /HPCSDCHK.BIN")))
        return 0;
    int fd = open("/HPCSDCHK.BIN", O_WRONLY | O_CREAT | O_EXCL, 0600);
    if (fd < 0)
        return errno == EEXIST ? 1u : 2u;
    ssize_t written = write(fd, "ORIGINAL", 8);
    int closed = close(fd);
    if (written != 8 || closed)
        return 3;
    FILE *old = fopen("/HPCSDCHK.BIN", "r+b");
    char pending[32];
    if (!old)
        return 4;
    if (setvbuf(old, pending, _IOFBF, sizeof pending) || fwrite("CHANGED!", 1, 8, old) != 8) {
        fclose(old);
        return 4;
    }
    /* This wait performs only keyboard I/O, never a card-presence query. */
    if (!prompt(HPCLANG_UI_TEXT("REMOVE AND REINSERT"),
                HPCLANG_UI_TEXT("Remove SD, then reinsert it."),
                HPCLANG_UI_TEXT("Press ENTER after reinsertion."))) {
        fclose(old);
        return 0;
    }
    if (fclose(old) != EOF || errno != EBADF)
        return 5;
    char bytes[8];
    FILE *fresh = fopen("/HPCSDCHK.BIN", "rb");
    if (!fresh)
        return 6;
    size_t count = fread(bytes, 1, sizeof bytes, fresh);
    closed = fclose(fresh);
    if (count != 8 || closed || memcmp(bytes, "ORIGINAL", 8))
        return 7;
    fresh = fopen("/HPCSDCHK.BIN", "r+b");
    if (!fresh)
        return 8;
    count = fwrite("VERIFIED", 1, 8, fresh);
    closed = fclose(fresh);
    if (count != 8 || closed)
        return 8;
    fresh = fopen("/HPCSDCHK.BIN", "rb");
    if (!fresh)
        return 9;
    count = fread(bytes, 1, sizeof bytes, fresh);
    closed = fclose(fresh);
    if (count != 8 || closed || memcmp(bytes, "VERIFIED", 8) || remove("/HPCSDCHK.BIN"))
        return 9;
    hpclang_e2e_result = 0x50415353u;
    (void)prompt(HPCLANG_UI_TEXT("PASS"), HPCLANG_UI_TEXT("Old write rejected; new I/O OK."),
                 HPCLANG_UI_TEXT("Test file removed. Exit now."));
    return 0;
}

int main(void)
{
    hpclang_keyboard_events_set_timing(20000, 0, 0);
    hpclang_keyboard_events_reset();
    if (hpclang_device_info()->family != HPCLANG_FAMILY_HP4950)
        return 1;
    unsigned result = check_card();
    if (result) {
        const char *digits = HPCLANG_UI_TEXT("0123456789");
        char code[] = HPCLANG_UI_TEXT("CODE 0");
        code[5] = digits[result];
        hpclang_e2e_result = 0x46410000u | result;
        (void)prompt(HPCLANG_UI_TEXT("SD CHECK FAILED"), code,
                     HPCLANG_UI_TEXT("Record code; keep the test file."));
    }
    return (int)result;
}
