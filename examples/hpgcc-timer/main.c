#include <errno.h>
#include <stdio.h>
#include <hpsys.h>
#include <hpclang/display.h>
#include <hpclang/font.h>

extern const struct hpclang_font report_font;

int main(void)
{
    sys_timer_t timer = SYS_TIMER_INITIALIZER;
    errno = 0;
    (void)sys_updateTimer(&timer);
    if (errno || !timer.inited)
        return 1;
    int start = sys_RTC_seconds();
    if (start < 0)
        return 1;

    sys_sleep(25);       /* milliseconds */
    sys_waitTicks(7500); /* 10 ms at the historical 750 kHz rate */
    if (errno)
        return 1;
    (void)sys_updateTimer(&timer);
    if (errno)
        return 1;
    int end = sys_RTC_seconds();
    if (end < 0)
        return 1;

    char report[64];
    int length = snprintf(report, sizeof report, "%llu us, %d civil seconds", timer.current / 1000,
                          sys_delta_seconds(start, end));
    if (length < 0 || (size_t)length >= sizeof report)
        return 1;
    struct hpclang_surface *screen = hpclang_display_surface();
    if (!screen)
        return 1;
    hpclang_surface_clear(screen, 0);
    hpclang_surface_draw_text_n(screen, &report_font, 0, 0, report, (size_t)length, 255);
    return 0;
}
