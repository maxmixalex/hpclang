#ifndef HPCLANG_RUNTIME_RPL_STOCK_MEMORY_H
#define HPCLANG_RUNTIME_RPL_STOCK_MEMORY_H

#include <stdint.h>
#include "rpl-mutation.h"

#define HPCLANG_RPL_MAIN_PAGES 128u

/* Invocation-local copy of the Stock main-RAM mapping. No Stock calls or
 * interrupt re-enabling may occur between preparation and mutation return. */
struct hpclang_rpl_stock_memory
{
    uintptr_t pages[HPCLANG_RPL_MAIN_PAGES];
};

int hpclang_rpl_stock_memory_capture(struct hpclang_rpl_stock_memory *memory,
                                     const uint32_t *read_map, uint32_t ram_section);
int hpclang_rpl_stock_memory_read(void *context, uint32_t address, uint8_t *value);
int hpclang_rpl_stock_memory_write(void *context, uint32_t address, uint8_t value);
int hpclang_rpl_stock_mutation_io(struct hpclang_rpl_mutation_io *io,
                                  struct hpclang_rpl_stock_memory *memory);
#endif
