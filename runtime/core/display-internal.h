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

#endif
