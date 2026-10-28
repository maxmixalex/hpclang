#ifndef HPCLANG_STAGE_STACK_H
#define HPCLANG_STAGE_STACK_H

#include <stdint.h>

struct hpclang_stage_stack_io
{
    void *context;
    int (*read)(void *context, uint32_t address, unsigned width, uint32_t *value);
    int (*write)(void *context, uint32_t address, uint32_t value);
};

struct stage_launcher_bias
{
    uint32_t object;
    uint32_t active;
};

/* Invocation-local stack reservation, while native execution is suspended.
 * Hide returns 1 for a recognized launcher cell, 0 when absent/unreadable,
 * -1 on write failure. Restore returns nonzero on success. The caller must
 * retain the hidden object as a relocation root and reserve its stack cell.
 * Entry/base fields identify the descriptor at hide time; restore uses only
 * the retained object root, not those consumed ARM launch addresses. This is
 * not permission to dereference stale fields or classify unrelated payloads.
 * These operations do not change AVMEM or supply rollback for failed writes. */
int hpclang_stage_stack_hide(const struct hpclang_stage_stack_io *io, uintptr_t stage0_code,
                             struct stage_launcher_bias *bias);
int hpclang_stage_stack_restore(const struct hpclang_stage_stack_io *io,
                                const struct stage_launcher_bias *bias);

/* A proven suspended Saturn PC names the next instruction, not arbitrary data.
 * RTNSXM, RTN, RTNSC and RTNCC unconditionally consume the circular machine-stack top
 * as their next execution address. Return that cell, or NULL for unreadable /
 * unsupported instructions or invalid state. No writes, allocation or scan
 * of lower slots. The caller supplies eight stable cells and a next-push cursor;
 * this does not classify other execution roots or certify a complete call chain.
 * RTNSXM's XM flag change does not affect these unconditional transfers; the
 * immediate selector does not interpret conditional returns. */
uint32_t *hpclang_stage_immediate_return_root(const struct hpclang_stage_stack_io *io,
                                              uint32_t continuation, uint32_t *machine_stack,
                                              uint32_t cursor);

/* Follow unconditional returns, direct GOTO/GOLONG/GOVLNG jumps and direct
 * GOSUB/GOSUBL/GOSBVL calls, up to eight distinct original machine-stack cells
 * and 64 total instructions. A direct jump
 * does not consume a cell; each reported cell requires reaching a proven return.
 * Calls push into a private circular-stack copy. Generated return addresses and
 * overwritten original cells are not reported; the supplied stack is unchanged.
 * RTNSC/RTNCC establish carry for later GOC/GONC, including zero-displacement
 * RTNC/RTNNC. Initial carry is unknown: these instructions stop discovery until
 * a followed return establishes it. RTN/RTNSXM and followed calls/jumps preserve
 * carry. No other flag-setting or predicate instructions are interpreted.
 * Stop at the step limit or an unreadable, unsupported or invalid continuation;
 * previously proven cells remain valid. Instruction mappings must remain stable.
 * This identifies consumed cells, not relocation repairs for call/jump operands.
 * No inference about the unvisited suffix or complete execution-root coverage.
 * The caller provides eight output slots disjoint from the stable stack and
 * instruction storage. Only the returned number of output slots is written. */
unsigned hpclang_stage_return_roots(const struct hpclang_stage_stack_io *io, uint32_t continuation,
                                    uint32_t *machine_stack, uint32_t cursor, uint32_t **roots);

/* Same bounded discovery, with the caller-proven carry at the suspended PC.
 * -1 means unknown, 0 clear, 1 set. Other values reject without output writes.
 * The caller must keep that interpreter state and instruction mapping stable;
 * no inference from flags of a different/earlier continuation is permitted. */
unsigned hpclang_stage_return_roots_with_carry(const struct hpclang_stage_stack_io *io,
                                               uint32_t continuation, uint32_t *machine_stack,
                                               uint32_t cursor, int carry, uint32_t **roots);

/* Invert a physical 4 KiB page through the 256-entry Stock Read map.
 * Return 1 only for one complete, consecutive pair of 2 KiB mappings in
 * main RAM, 0 for no overlap, -1 for partial/ambiguous/invalid mappings.
 * On 0/-1 leave the output untouched. Only main-RAM aliases are classified;
 * banked mappings require their own ownership proof. */
int hpclang_stage_main_page(const uint32_t *read_map, uint32_t physical_page,
                            uint32_t *saturn_start);

#endif
