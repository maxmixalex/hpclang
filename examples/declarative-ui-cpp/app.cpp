#include "app.hpp"
#include <hpclang/ui.hpp>
#include <cstdio>

static int counter(hpclang_ui *ui, uint32_t key, unsigned value)
{
    hpclang::ui::scope component{ui, key, HPCLANG_UI_COMPONENT};
    if (!component)
        return -1;
    char label[32];
    std::snprintf(label, sizeof(label), "Count: %u", value);
    return hpclang_ui_text(ui, 1, 0, label);
}

int counter_view(hpclang_ui *ui, hpclang_ui_surface *renderer, hpclang_ui_rect viewport,
                 unsigned value)
{
    hpclang::ui::declaration frame{ui};
    if (!frame)
        return -1;
    {
        hpclang::ui::scope column{ui, 1, HPCLANG_UI_COLUMN};
        if (!column || counter(ui, 1, value) || hpclang_ui_button(ui, 2, 0, "Add one", {1, 0}, 1) ||
            hpclang_ui_button(ui, 3, 0, "Reset", {2, 0}, 1) ||
            hpclang_ui_button(ui, 4, 0, "Quit", {3, 0}, 1))
            return -1;
    }
    if (hpclang_ui_layout(ui, viewport, hpclang_ui_surface_measure, renderer))
        return -1;
    return frame.commit();
}
