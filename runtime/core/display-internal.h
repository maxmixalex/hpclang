#ifndef HPCLANG_DISPLAY_INTERNAL_H
#define HPCLANG_DISPLAY_INTERNAL_H

#include "dma-buffer.h"
#include "lcd-engine.h"
#include "irq.h"

/* Hardware boundary kept separate from the ownership state machine. */
extern const struct hpclang_lcd_bus hpclang_display_bus;
extern const struct hpclang_dma_allocator hpclang_display_allocator;
void hpclang_cache_clean(const void *start, uint32_t bytes);
struct hpclang_surface *hpclang_display_back(void);
void hpclang_display_reclock_saved(uint32_t control);
/* Adapter-owned single-page scanout. Public begin retains two-page semantics.
 * publish flushes CPU writes without waiting, changing pages or touching LCD
 * registers; the single-page adapter calls it after each drawing operation. */
int hpclang_display_begin_pages(uint32_t levels, unsigned page_count);
/* Replace an owned mode atomically; failure keeps its pages and LCD state. */
int hpclang_display_replace_pages(uint32_t levels, unsigned page_count);
int hpclang_display_publish(void);

#endif
