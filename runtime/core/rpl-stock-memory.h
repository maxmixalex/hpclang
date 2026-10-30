#ifndef HPCLANG_RUNTIME_RPL_STOCK_MEMORY_H
#define HPCLANG_RUNTIME_RPL_STOCK_MEMORY_H

#include <stdint.h>
#include "rpl-mutation.h"
#include "rpl-reference.h"

#define HPCLANG_RPL_MAIN_PAGES 128u
#define HPCLANG_RPL_PORT1_NIBBLES 0x40000u
#define HPCLANG_RPL_PORT2_BANKS 6u
#define HPCLANG_RPL_FLASH_BANK_NIBBLES 0x40000u

struct hpclang_rpl_rom_bank;
struct hpclang_rpl_stock_rom_bank
{
    const volatile uint8_t *bytes;
    uint32_t size;
};

/* Incoming device routing for the reviewed CONFIG/DAT0/UNCONFIG sequence.
 * Bases retain native units: IO in 64-nibble blocks, RAM devices in 4096-nibble
 * pages. Capture and guard these fields with the selector, code and map state. */
struct hpclang_rpl_stock_bank_route
{
    uint32_t flags, io, ram, eram, port;
};

/* Capture these five 32-bit fields from a caller-qualified CPU-state reader.
 * Reader/dependency coordinates are NIBBLE OFFSETS from the interpreter state,
 * not Saturn virtual addresses. Use a distinct mapping ID in reference guards.
 * Every field nibble is observed, including inactive device fields; any failure
 * leaves route unchanged and invalidates partial dependencies. Positive callback
 * errors become MALFORMED. Caller holds the mapping/state stable and validates
 * retained guards before using it; capture itself supplies neither exclusion
 * nor firmware identity. No hardware access, allocation or mutation. */
int hpclang_rpl_stock_bank_route_capture(const struct hpclang_rpl_reader *state,
                                         struct hpclang_rpl_stock_bank_route *route,
                                         hpclang_rpl_reference_dependency_fn dependency,
                                         void *context);

/* Check only device routing under independently identified handler semantics.
 * Require configured IO/main RAM and no pending configuration or bank device;
 * optional ERAM/port devices may remain configured. The five-nibble selector
 * read and bank-control write must avoid IO; UNCONFIG must not select any RAM
 * device before the bank device. configuration is the current selector value.
 * Zero means these routing preconditions hold, not complete switch safety.
 * Does not read state, identify firmware, check pending interrupts/return roots,
 * validate geometry/owners or authorize mutation. Nonzero is a span error. */
int hpclang_rpl_stock_bank_route_check(const struct hpclang_rpl_stock_bank_route *route,
                                       uint32_t selector, uint32_t configuration);

struct hpclang_rpl_stock_bank_window
{
    uint32_t pages[64];
    uint8_t owners[64];
};

/* Capture Read[0x40..0x7f] and its owner bytes in the SAME CPU-state coordinate
 * system and with the same lifetime/dependency contract as route_capture.
 * Observe all 640 nibbles, including pages currently hidden by an owner. Publish
 * only on complete success. Values are captured, not authenticated or validated;
 * window_select checks usable page geometry, while owner meaning remains a
 * caller obligation. This is not a snapshot of lower pages or write mappings. */
int hpclang_rpl_stock_bank_window_capture(const struct hpclang_rpl_reader *state,
                                          struct hpclang_rpl_stock_bank_window *window,
                                          hpclang_rpl_reference_dependency_fn dependency,
                                          void *context);

/* Model only the reviewed Flash-selection update of Read[0x40..0x7f].
 * A zero owner selects the corresponding bank page; any nonzero owner keeps
 * the previous page. Inputs must be stable, independently qualified snapshots
 * of the same interpreter state. Caller proves handler identity, P=0 selection
 * semantics, geometry provenance and every retained owner's meaning. Flags or
 * a bank index cannot replace those obligations. This does not model ERAM
 * entry/exit, lower-window changes, write maps or interpreter side effects.
 * The 128-KiB window may exceed bank->bytes (notably the 96-KiB first bank);
 * retain that separate bound before reading any selected-bank payload.
 * No native reads/writes or bank switches. Return zero or a negative span result.
 * Invalid/unpopulated geometry and invalid retained pages leave output unchanged;
 * output may alias previous. A predicted map does not authorize mutation. */
int hpclang_rpl_stock_bank_window_select(const uint32_t previous[64], const uint8_t owners[64],
                                         const struct hpclang_rpl_rom_bank *bank,
                                         uint32_t selected[64]);

struct hpclang_rpl_stock_bank_map
{
    uint32_t pages[256];
    uint8_t owners[256];
    uint32_t lower, upper, overlay_page, overlay_physical;
};

/* Capture all 256 read pages/owners, prior bank selectors and overlay fields.
 * Same state-relative nibble coordinates and exclusive-lifetime contract as
 * route_capture. All 2592 observed nibbles must retain the native-state mapping
 * ID when accumulated into commit guards. No partial output on failure; values
 * require separate semantic validation. This does not capture write maps,
 * geometry, interpreter code or all state needed for execution qualification. */
int hpclang_rpl_stock_bank_map_capture(const struct hpclang_rpl_reader *state,
                                       struct hpclang_rpl_stock_bank_map *map,
                                       hpclang_rpl_reference_dependency_fn dependency,
                                       void *context);

/* Predict the complete Read[0..255] result of the reviewed bank-control handler.
 * Equal bank selectors preserve the previous map. Otherwise update all four
 * quadrants respecting page owners, then restore the optional two-page overlay.
 * banks points to sixteen entries; it and all state fields require independently
 * qualified, guarded snapshots.
 * Reject absent banks, invalid physical pages and unsupported overlay bounds.
 * No writes; output is unchanged on failure and may alias state->pages.
 * This models read maps only: sentinel/write-map/cache updates, configuration
 * routing, interpreter identity and complete execution effects remain external. */
int hpclang_rpl_stock_bank_map_select(const struct hpclang_rpl_stock_bank_map *state,
                                      const struct hpclang_rpl_rom_bank *banks, uint32_t lower,
                                      uint32_t upper, uint32_t selected[256]);

/* Read a separately proven ROM bank in native 0x40000-based coordinates,
 * bounded by its actual declared byte size, not the entire bank-switch window.
 * Capture checks Flash extent and every touched ARM identity section for user
 * read permission. bank_bytes must correspond to bank->physical; the caller
 * proves geometry provenance (rom_bank_layout) and retains memory/reader storage
 * and exclusive mapping lifetime. No switch, allocation, native call or write.
 * Failure preserves both outputs. Shape/permission checks do not identify ROMs.
 * The Stock entry uses the current launcher identity mapping on HP49g+/HP50g. */
int hpclang_rpl_stock_rom_bank_capture(struct hpclang_rpl_stock_rom_bank *memory,
                                       struct hpclang_rpl_reader *reader,
                                       const volatile uint8_t *bank_bytes,
                                       const struct hpclang_rpl_rom_bank *bank,
                                       const uint32_t flash_sections[2]);
int hpclang_rpl_stock_rom_bank_reader(struct hpclang_rpl_stock_rom_bank *memory,
                                      struct hpclang_rpl_reader *reader,
                                      const struct hpclang_rpl_rom_bank *bank);

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
