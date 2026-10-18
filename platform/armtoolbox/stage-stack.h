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
 * RTN, RTNSC and RTNCC unconditionally consume the circular machine-stack top
 * as their next execution address. Return that cell, or NULL for unreadable /
 * unsupported instructions or invalid state. No writes, allocation or scan
 * of lower slots. The caller supplies eight stable cells and a next-push cursor;
 * this does not classify other execution roots or certify a complete call chain. */
uint32_t *hpclang_stage_immediate_return_root(const struct hpclang_stage_stack_io *io,
                                              uint32_t continuation, uint32_t *machine_stack,
                                              uint32_t cursor);

/* Follow a prefix of consecutive unconditional returns, up to eight distinct
 * original machine-stack cells. Each consumed target must itself name a proven
 * return instruction before the next cell is included. Stop at an unreadable,
 * unsupported or invalid continuation; previously proven cells remain valid.
 * No inference about the unvisited suffix or complete execution-root coverage.
 * The caller provides eight output slots disjoint from the stable stack and
 * instruction storage. Only the returned number of output slots is written. */
unsigned hpclang_stage_return_roots(const struct hpclang_stage_stack_io *io, uint32_t continuation,
                                    uint32_t *machine_stack, uint32_t cursor, uint32_t **roots);

#endif
