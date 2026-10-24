#ifndef HPCLANG_HPG_INTERNAL_H
#define HPCLANG_HPG_INTERNAL_H
#include <stddef.h>
#include <hpgraphics.h>
#include <hpclang/display.h>

struct hpg_font
{
    const unsigned char *buffer;
    unsigned count, height, advance;
    int borrowed_descriptor;
};
struct hpg_pattern
{
    const unsigned char *buffer;
    unsigned height;
    int fixed;
};
struct hpg_graphics
{
    struct hpclang_surface *surface;
    struct hpclang_surface *mask;
    int width, height, left, top, right, bottom;
    unsigned char color, mode;
    struct hpg_pattern *pattern;
    struct hpg_font *font;
};
struct hpg_graphics *hpclang_hpg_resolve(struct hpg_graphics *g);
struct hpclang_surface *hpclang_hpg_surface(struct hpg_graphics *g);
/* Share HPG's display lease with historical text output. First acquisition
 * preserves Stock pixels/icons; an existing HPG mode and buffering policy stay
 * intact. A separately owned clean display session fails with EBUSY. */
struct hpg_graphics *hpclang_hpg_acquire_screen(void);
/* Interactive console updates publish an owned HPG page. In double-buffer mode,
 * retain the displayed contents in the new back page for the next edit. */
int hpclang_hpg_console_present(void);
/* Checked window repaint hooks: borrow/acquire the existing HPG lease, clear
 * the current drawing page, then publish/flip after event callbacks. Preserve
 * an existing mode and buffering policy; cleanup remains hpg_cleanup's job.
 * Context is unused. Drawing through legacy single-buffer HPG primitives may
 * publish before the final hook; this is not an atomic-frame guarantee. */
int hpclang_hpg_frame_begin(void *context);
int hpclang_hpg_frame_present(void *context);
void hpclang_hpg_finish(struct hpg_graphics *g);
void hpclang_hpg_builtin_glyph(struct hpclang_surface *surface, hpg_font_t *font,
                               unsigned char code, int x, int y);
#endif
