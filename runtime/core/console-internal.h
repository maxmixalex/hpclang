#ifndef HPCLANG_CONSOLE_INTERNAL_H
#define HPCLANG_CONSOLE_INTERNAL_H
#include <stdint.h>
struct hpclang_console;
struct hpclang_surface;
/* Adapter glyphs are drawn into a cleared cell-sized monochrome surface. The
 * callback and borrowed context outlive the console and must not reenter it.
 * Cell clipping, text storage and controls remain owned by the console. */
typedef void (*hpclang_console_glyph_fn)(struct hpclang_surface *cell, const void *context,
                                         uint32_t codepoint);
struct hpclang_console *hpclang_console_create_cells(unsigned columns, unsigned rows,
                                                     unsigned width, unsigned height,
                                                     hpclang_console_glyph_fn draw,
                                                     const void *context);
/* Historical calculator-byte text: 0x20..0xff are glyph IDs, including C1
 * bytes; lower controls use the console policy. Finish pending UTF-8 first. */
int hpclang_console_write_byte(struct hpclang_console *console, unsigned char byte);
#endif
