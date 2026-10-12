#include <hpclang/display.h>
#include <hpclang/font.h>

extern const struct hpclang_font hello_font;
static const char greeting[] = HPCLANG_UI_TEXT("Hello C");

int main(void)
{
    struct hpclang_surface *surface = hpclang_display_surface();
    if (!surface) {
        return 1;
    }
    hpclang_surface_clear(surface, 0);
    hpclang_surface_draw_text(surface, &hello_font, 4, 4, greeting, 255);
    return 0;
}
