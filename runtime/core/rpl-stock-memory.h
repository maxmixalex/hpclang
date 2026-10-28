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

/* Resolve a nonempty Saturn span through a caller-owned, stable 256-entry
 * HP49g+/50g Read[] map. Return its absolute physical NIBBLE address only when
 * every touched 2-KiB page is aligned, belongs to the 2-MiB Flash or 512-KiB
 * SRAM region, and the entire span is physically contiguous. Aliases therefore
 * compare equal, while distinct banks remain distinct. Discontinuous spans
 * must be split by the caller, not approximated by their first page.
 * No target memory is read. The caller must independently establish the map's
 * provenance, ARM identity mappings and exclusive lifetime; this proves neither
 * pointer type nor complete roots, and never authorizes reads or mutation.
 * Zero is success; negative span errors leave physical unchanged. */
int hpclang_rpl_stock_resolve_span(const uint32_t read_map[256], uint32_t address, uint32_t span,
                                   uint32_t *physical);

/* Enumerate maximal physically contiguous runs for a logical span, in logical
 * order. Repeated physical aliases remain separate logical runs. Validate all
 * touched pages before invoking the first callback; each run reports absolute
 * physical NIBBLE coordinates. Same map provenance/lifetime and region bounds
 * as resolve_span, but discontinuous pages are split rather than rejected.
 * Nonzero callback results propagate and invalidate partial output. No target
 * memory reads, allocation, pointer classification or mutation authorization. */
typedef int (*hpclang_rpl_stock_span_visit_fn)(void *context, uint32_t logical, uint32_t physical,
                                               uint32_t span);
int hpclang_rpl_stock_visit_spans(const uint32_t read_map[256], uint32_t address, uint32_t span,
                                  hpclang_rpl_stock_span_visit_fn visit, void *context);

/* Compare complete nonempty ranges under two independently proven, stable
 * mappings. Return 1 for any shared physical nibble, 0 for disjoint ranges,
 * or a negative span error. Validate both complete ranges even if their first
 * pages overlap. Logical address equality alone is neither necessary nor
 * sufficient. No allocation or target-memory access; worst case compares
 * 256 by 256 physical runs. This proves only aliasing, not a live reference,
 * bank-switch provenance, write permission or relocation safety. */
int hpclang_rpl_stock_spans_overlap(const uint32_t left_map[256], uint32_t left_address,
                                    uint32_t left_span, const uint32_t right_map[256],
                                    uint32_t right_address, uint32_t right_span);

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
