#ifndef HPCLANG_CONSOLE_H
#define HPCLANG_CONSOLE_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
    struct hpclang_console;
    struct hpclang_font;
    struct hpclang_surface;
    struct hpclang_console_state
    {
        unsigned columns, rows, column, row, tab_width;
    };

    /* Owned fixed-cell Unicode console. Borrows font until destroy. Cell geometry
     * uses its nominal advance and line height; each glyph is clipped to its cell.
     * create allocates the text grid and drawing surfaces, but touches no hardware.
     * Invalid/overflowing geometry: NULL/EINVAL; allocation failure: NULL/ENOMEM.
     * Calls are single-threaded, not IRQ-safe. No ANSI terminal or grapheme policy. */
    struct hpclang_console *hpclang_console_create(unsigned columns, unsigned rows,
                                                   const struct hpclang_font *font);
    void hpclang_console_destroy(struct hpclang_console *console);
    int hpclang_console_clear(struct hpclang_console *console);
    /* Zero-based cells. column == columns denotes the deferred wrap position.
     * Query does not flush UTF-8. Move/erase finish an incomplete sequence as
     * U+FFFD first; invalid moves leave both text and decoder unchanged.
     * Erase clears from the resulting cursor through its row without moving it.
     * These operations allocate nothing; invalid arguments return -1/EINVAL. */
    int hpclang_console_get_state(const struct hpclang_console *console,
                                  struct hpclang_console_state *state);
    int hpclang_console_move(struct hpclang_console *console, unsigned column, unsigned row);
    int hpclang_console_clear_eol(struct hpclang_console *console);
    /* Positive width in cells, initially four. Clear retains the selected width.
     * A tab's work is bounded by the grid size, even for very large widths. */
    int hpclang_console_set_tab_width(struct hpclang_console *console, unsigned width);
    /* Incremental UTF-8 across writes. Malformed sequences become U+FFFD. NUL and
     * unrecognized C0/C1 controls are ignored. LF moves to column 0 of the next row,
     * CR to column 0, BS moves left without erasing, TAB inserts spaces to a configured
     * stop, FF clears. Printing past the last column wraps; the last row scrolls.
     * A full final row is retained until another character/newline needs space.
     * Return 0 or -1/EINVAL. write(NULL,0) is allowed with a valid console. */
    int hpclang_console_write(struct hpclang_console *console, const char *bytes, size_t length);
    /* Finish a pending incomplete UTF-8 sequence as U+FFFD. Render does not flush. */
    int hpclang_console_flush(struct hpclang_console *console);
    int hpclang_console_cell(const struct hpclang_console *console, unsigned column, unsigned row,
                             uint32_t *codepoint);
    /* Draw an opaque black-on-white viewport, clipped to destination; does not
     * present, acquire a display session, or retain destination. Reacquire a display
     * back page after each present. Return 0 or -1/EINVAL. */
    int hpclang_console_render(struct hpclang_console *console, struct hpclang_surface *destination,
                               int32_t x, int32_t y);
    /* Optional hpclang_stdio_callbacks adapters; context is a live console.
     * stdout and stderr share one ordered UTF-8 stream. Flush finishes decoding;
     * the application still calls render/present. Unbind before destroying it. */
    int hpclang_console_stdio_write(void *context, unsigned stream, unsigned char byte);
    int hpclang_console_stdio_flush(void *context, unsigned stream);
#ifdef __cplusplus
}
#endif
#endif
