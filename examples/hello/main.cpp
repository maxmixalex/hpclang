#include <hpclang/display.hpp>
#include <hpclang/font.h>

extern "C" const struct hpclang_font hello_font;
static const char greeting[] = HPCLANG_UI_TEXT("Hello C++");

int main()
{
    auto *surface = hpclang_display_surface();
    if (!surface) {
        return 1;
    }
    hpclang::surface_ref view(surface);
    view.clear();
    hpclang_surface_draw_text(surface, &hello_font, 4, 4, greeting, 255);
    return view.width() == 0;
}
