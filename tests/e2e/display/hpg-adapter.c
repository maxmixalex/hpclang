/* Exercise installed historical spellings, including the mixed-font scene and
 * double-buffer drawing pattern from historical HPG examples. No private HPG
 * structure, ROM bitmap or historical implementation is embedded here. */
#include <hpgraphics.h>
#include <hpclang/display.h>

unsigned hpclang_hpg_adapter(int (*checkpoint)(unsigned))
{
    unsigned failure = 1;
    char *xpm[] = {"3 2 3 2", "aa c #000000", "bb c #ffffff", "cc c #808080", "aabbcc", "ccbbaa"};
    hpg_t *image = hpg_load_xpm_gray16(xpm);
    if (!image)
        return failure;
    hpg_set_mode_gray16(0);
    if (hpclang_surface_levels(hpclang_display_surface()) != 16)
        goto done;
    hpg_clear();
    hpg_set_font(HPG_STDSCREEN, hpg_get_minifont());
    hpg_draw_text("minifont", 0, 0);
    hpg_set_font(HPG_STDSCREEN, hpg_get_bigfont());
    hpg_draw_text("big font", 0, 10);
    char symbols[] = {(char)0x80, (char)0x87, (char)0x9a, (char)0x9f, 0};
    hpg_draw_text(symbols, 0, 20);
    hpg_set_color(HPG_STDSCREEN, HPG_COLOR_GRAY_8);
    hpg_fill_rect(50, 0, 60, 10);
    hpg_set_color(HPG_STDSCREEN, HPG_COLOR_BLACK);
    hpg_draw_line(70, 5, 70, 25);
    hpg_blit(image, 0, 0, 3, 2, HPG_STDSCREEN, 80, 5);
    hpg_set_indicator(HPG_INDICATOR_ALPHA, HPG_COLOR_BLACK);
    failure = 2;
    if (checkpoint(110))
        goto done; /* No flip: single-buffer publication. */
    hpg_set_mode_mono(1);
    failure = 3;
    if (hpclang_surface_levels(hpclang_display_surface()) != 2)
        goto done;
    hpg_clear();
    hpg_fill_rect(2, 2, 8, 8);
    hpg_set_indicator(HPG_INDICATOR_LSHIFT, HPG_COLOR_BLACK);
    hpg_flip();
    if (checkpoint(111))
        goto done;
    hpg_clear();
    hpg_fill_rect(20, 2, 26, 8);
    hpg_set_indicator(HPG_INDICATOR_RSHIFT, HPG_COLOR_BLACK);
    hpg_flip();
    failure = 4;
    if (checkpoint(112))
        goto done;
    if (hpg_get_pixel(HPG_STDSCREEN, 3, 3) != 255 || hpg_get_pixel(HPG_STDSCREEN, 21, 3) != 0)
        goto done;
    hpg_flip();
    if (checkpoint(113))
        goto done;
    hpg_set_mode_gray4(0);
    failure = 5;
    if (hpclang_surface_levels(hpclang_display_surface()) != 4)
        goto done;
    hpg_clear();
    hpg_set_color(HPG_STDSCREEN, 85);
    hpg_fill_rect(1, 1, 10, 10);
    if (checkpoint(114))
        goto done;
    failure = 0;
done:
    hpg_free_image(image);
    hpg_cleanup();
    return failure;
}
