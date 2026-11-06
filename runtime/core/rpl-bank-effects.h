#ifndef HPCLANG_RPL_BANK_EFFECTS_H
#define HPCLANG_RPL_BANK_EFFECTS_H
#include "rpl-stock-memory.h"
#include "rpl-port.h"

struct hpclang_rpl_bank_effects
{
    struct hpclang_rpl_stock_bank_map map;
    uint32_t sentinel;    /* Native byte offset 0x400. */
    uint32_t writes[256]; /* Native byte offsets 0x404..0x803. */
    uint32_t cache[4];    /* Saved read/read/write/write words at 0xc4c..0xc58. */
};

/* Predict the reviewed HP49g+/50g native CONFIG handler from a guarded native
 * [0,0xc60) snapshot and the current C.A value. Models every flag-dispatch arm:
 * IO, size/base phases for RAM/ERAM/port, bank-control routing, owner priority,
 * read/write maps, sentinels and conditional two-page overlay caches. Other
 * bytes survive, including CPU registers. c may contain the full native word;
 * only its low 20 bits participate. No interpreter/PC step or native writes.
 * Caller qualifies ROM identity, snapshot lifetime and all input dependencies;
 * opaque physical words are predicted, not certified as accessible memory.
 * Mapping spans outside 256 pages and active overlays outside pages 1..254
 * are rejected before output changes. Zero-size maps skip overlay work, as
 * the ROM does, but still refresh sentinels. Output may equal input exactly;
 * otherwise the two buffers must be disjoint. No allocation or large scratch
 * frame. This is a prediction primitive, not execution/mutation authority. */
int hpclang_rpl_config_predict(const uint8_t state[0xc60], uint32_t c, uint8_t output[0xc60]);

struct hpclang_rpl_bank_cpu_effects
{
    /* Native-state byte slice [0x90c,0x99c): A..R4, D0/D1/P, flags,
     * saved PC, eight return cells and circular cursor. */
    uint8_t state[0x90];
    uint32_t continuation;
};
/* Predict the reviewed P=0/P=1 bank program's CPU effects after its final RTN.
 * Caller separately qualifies program/interpreter identity, incoming P,
 * configuration value, exclusive state and absence of pending interrupts.
 * C's low five nibbles and D0 receive configuration; P and carry become zero.
 * RTN clears its consumed slot and decrements the circular cursor. All other
 * bytes in the captured slice survive. continuation is the consumed address,
 * not a qualification of its mapping, ownership or executable contents.
 * Keep these input bytes/configuration in dependency guards. Does not model
 * memory/map/CONFIG effects, IRQs, or authorize execution or movement.
 * Zero publishes output; failure preserves it. Input may be output.state. */
int hpclang_rpl_bank_cpu_predict(const uint8_t state[0x90], uint32_t configuration,
                                 struct hpclang_rpl_bank_cpu_effects *output);
/* Capture the CPU slice through native-state-relative nibble coordinates and
 * predict the same normal return. The reviewed dispatch loop tests native
 * word 0xc24 before executing instructions; reject any nonzero value instead
 * of silently applying the no-interrupt model. expected_p must match both the
 * captured P and the separately prepared bank program. All 296 valid nibble
 * reads (pending word plus CPU slice) reach dependency on success. The caller
 * retains those guards and the configuration/program dependencies under an
 * exclusive lifetime. This is not a complete interrupt model. Errors preserve
 * output and invalidate partial observations; positive callback errors become
 * MALFORMED. No native writes or allocation. */
int hpclang_rpl_bank_cpu_capture_predict(const struct hpclang_rpl_reader *reader,
                                         uint32_t configuration, unsigned expected_p,
                                         struct hpclang_rpl_bank_cpu_effects *output,
                                         hpclang_rpl_reference_dependency_fn dependency,
                                         void *context);

/* Capture read-map state plus sentinel, all write-map words and overlay caches.
 * Coordinates are native-state-relative NIBBLES, matching bank_map_capture.
 * Caller keeps the interpreter/mappings exclusive and stable throughout capture,
 * prediction and use; preserve the native mapping ID in dependency guards.
 * All 4680 observed nibbles are reported. Callback errors invalidate accumulated
 * dependencies; negative errors propagate, positive errors become MALFORMED.
 * Output changes only on success. Capture does not semantically validate values
 * or establish ROM identity, geometry, routing or complete execution roots. */
int hpclang_rpl_bank_effects_capture(const struct hpclang_rpl_reader *reader,
                                     struct hpclang_rpl_bank_effects *state,
                                     hpclang_rpl_reference_dependency_fn dependency, void *context);

/* Predict the reviewed populated-bank handler's mapping/cache state without
 * executing it. Inputs are separately qualified, stable snapshots; retain all
 * source dependencies for later guards. The existing map validator qualifies
 * read pages, geometry and overlay bounds. Write/cache words are opaque values
 * copied or replaced, never dereferenced or certified as writable addresses.
 * Equal selectors preserve all state. Otherwise publish Read[256] before
 * conditional overlay repair, preserving caches when their read page matches.
 * Zero succeeds; negative span errors preserve output. Output may equal input.
 * No allocation, native memory access or writes. This is not a complete CPU,
 * routing/interrupt model, execution-edge proof or mutation authorization. */
int hpclang_rpl_bank_effects_select(const struct hpclang_rpl_bank_effects *state,
                                    const struct hpclang_rpl_rom_bank banks[16], uint32_t lower,
                                    uint32_t upper, struct hpclang_rpl_bank_effects *selected);
#endif
