#ifndef HPCLANG_RUNTIME_RPL_STOCK_MEMORY_H
#define HPCLANG_RUNTIME_RPL_STOCK_MEMORY_H

#include <stdint.h>
#include "rpl-mutation.h"

#define HPCLANG_RPL_MAIN_PAGES 128u
#define HPCLANG_RPL_PORT1_NIBBLES 0x40000u
#define HPCLANG_RPL_PORT2_BANKS 6u
#define HPCLANG_RPL_FLASH_BANK_NIBBLES 0x40000u

/* HP49g+/50g user Flash banks 8..13 occupy physical 0x140000..0x1fffff.
 * Capture verifies their enclosing identity section permits user reads. */
int hpclang_rpl_stock_port2_capture(struct hpclang_rpl_reader *reader,
                                    const volatile uint8_t *bank_bytes, uint32_t flash_section);
int hpclang_rpl_stock_port2_reader(uint32_t bank, struct hpclang_rpl_reader *reader);

/* HP49/50 ERAM is a packed RAM-port chain independent of the active Saturn
 * bank. Read-only capture does not claim ownership of its free tail. */
int hpclang_rpl_stock_port1_capture(struct hpclang_rpl_reader *reader,
                                    const volatile uint8_t *bytes, uint32_t ram_section);
int hpclang_rpl_stock_port1_reader(struct hpclang_rpl_reader *reader);
int hpclang_rpl_stock_port1_mutation_io(struct hpclang_rpl_mutation_io *io, uint32_t *limit);

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
