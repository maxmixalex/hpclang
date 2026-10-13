#ifndef HPCLANG_UI_SURFACE_H
#define HPCLANG_UI_SURFACE_H

#include <hpclang/ui.h>
#include <hpclang/font.h>
#include <hpclang/display.h>

#ifdef __cplusplus
extern "C"
{
#endif

    struct hpclang_ui_surface;
    struct hpclang_ui_surface_style
    {
        const struct hpclang_font *font;
        uint8_t foreground, background, disabled, accent;
        uint16_t padding_x, padding_y, gap, grow;
        int32_t minimum_width, minimum_height;
    };

    /* Create once. Copies styles, borrows fonts, and owns two bounded raster pages.
     * Style IDs index this table. No allocation occurs during measure/render/copy.
     * Geometry must fit signed drawing coordinates; levels are 2, 4 or 16.
     * This renderer never acquires or presents the calculator display. */
    struct hpclang_ui_surface *
    hpclang_ui_surface_create(uint32_t width, uint32_t height, uint32_t levels,
                              const struct hpclang_ui_surface_style *styles, size_t count,
                              uint8_t background);
    void hpclang_ui_surface_destroy(struct hpclang_ui_surface *renderer);

    /* Pass directly to hpclang_ui_layout with renderer as context. */
    int hpclang_ui_surface_measure(void *context, const struct hpclang_ui_node *node,
                                   struct hpclang_ui_metrics *metrics);
    /* Paint dirty regions into a private page and accept it only on success.
     * Canvas paint_context is a borrowed, clipped hpclang_surface, valid only in
     * that callback. It must not be retained or destroyed. Returns 1/updated,
     * 0/unchanged, -1/errno. Changing UI contexts forces full invalidation. */
    int hpclang_ui_surface_render(struct hpclang_ui_surface *renderer, struct hpclang_ui *ui);
    /* Copy the complete accepted image to a same-sized target (levels may differ).
     * After render>=0, copy to each newly borrowed LCD back page before present.
     * A failed render never changes this accepted image. */
    int hpclang_ui_surface_copy(struct hpclang_ui_surface *renderer,
                                struct hpclang_surface *target);

#ifdef __cplusplus
}
#endif
#endif
