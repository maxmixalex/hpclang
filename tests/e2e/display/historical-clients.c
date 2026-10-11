/* Flow-level clean-API ports of HPGCC 1.1 hpg/examples/testdoublebuf.c
 * and HPGCC3 headers/hpgraphics.h's mixed-font gray16 example.
 * Busy loops become vsync-paced presentation, the lower shapes follow actual
 * screen height, and unavailable native fonts have explicit generated assets.
 * Original application flow is retained; this is not a binary/source ABI claim. */
#include <hpclang/display.h>
#include <hpclang/font.h>
#include <hpclang/keyboard.h>

extern const struct hpclang_font hpclang_system_font;
extern const struct hpclang_font hpclang_historical_big_font;

static int on_pressed(void)
{
    struct hpclang_keyboard_state keys;
    hpclang_keyboard_read(&keys);
    return hpclang_keyboard_dedicated_is_down(&keys, HPCLANG_KEYBOARD_ON);
}

unsigned hpclang_historical_graphics(int (*checkpoint)(unsigned))
{
    unsigned failure = 1;
    if (hpclang_display_begin(2))
        return failure;
    struct hpclang_surface *surface = hpclang_display_surface();
    int width = (int)hpclang_surface_width(surface);
    int height = (int)hpclang_surface_height(surface);
    unsigned frames = 0;
    for (unsigned cycle = 0; cycle < 5; ++cycle) {
        for (int x = 0; x < width; ++x) {
            surface = hpclang_display_surface();
            hpclang_surface_clear(surface, 0);
            hpclang_surface_draw_line(surface, x, 0, width - 1 - x, height - 1, 255);
            if (hpclang_display_present(200000))
                goto done;
            ++frames;
        }
        if (on_pressed())
            break;
        if (cycle == 4 && checkpoint(100))
            goto done;
        for (int y = 1; y < height; ++y) {
            surface = hpclang_display_surface();
            hpclang_surface_clear(surface, 0);
            hpclang_surface_draw_line(surface, 0, height - 1 - y, width, y, 255);
            if (hpclang_display_present(200000))
                goto done;
            ++frames;
        }
        if (on_pressed())
            break;
    }
    failure = 2;
    if (frames != 5u * (unsigned)(width + height - 1) || checkpoint(101))
        goto done;
    hpclang_display_end();
    failure = 3;
    if (hpclang_display_begin(16))
        return failure;
    surface = hpclang_display_surface();
    const struct hpclang_font *mini = hpclang_font_mini();
    const struct hpclang_font *big = hpclang_font_big();
    if (!hpclang_font_available(mini))
        mini = &hpclang_system_font;
    if (!hpclang_font_available(big))
        big = &hpclang_historical_big_font;
    hpclang_surface_clear(surface, 0);
    hpclang_surface_draw_text(surface, mini, 0, 0, HPCLANG_UI_TEXT("minifont"), 255);
    hpclang_surface_draw_text(surface, big, 0, 20, HPCLANG_UI_TEXT("big font"), 255);
    hpclang_surface_draw_text(surface, mini, 0, 35, HPCLANG_UI_TEXT("minifont again"), 255);
    hpclang_surface_draw_line(surface, 70, 5, 70, 25, 255);
    hpclang_surface_draw_rect(surface, 50, height - 20, 6, 11, 255);
    hpclang_surface_fill_rect(surface, 50, 0, 11, 11, 0x88);
    hpclang_surface_fill_circle(surface, 70, height - 10, 4, 0x55);
    if (hpclang_display_present(200000) || checkpoint(102))
        goto done;
    failure = 0;
done:
    hpclang_display_end();
    return failure;
}
