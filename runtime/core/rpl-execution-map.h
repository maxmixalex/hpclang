#ifndef HPCLANG_RPL_EXECUTION_MAP_H
#define HPCLANG_RPL_EXECUTION_MAP_H
#include <stdint.h>
#include "rpl-reference.h"
/* Prove that every logical nibble in a nonempty execution range retains its
 * physical location across two separately qualified read maps. Reject invalid
 * pages, bounds and remapping, including equal-byte/different-physical aliases.
 * Discontinuous physical runs are supported. Zero succeeds; negative span
 * errors reject. No target reads or writes. Caller retains map/code guards and
 * exclusive lifetime. This is one continuation condition, not a proof of code
 * identity, register/return roots, instruction boundaries or execution safety. */
int hpclang_rpl_execution_mapping_preserved(const uint32_t before[256], const uint32_t after[256],
                                            uint32_t address, uint32_t span);
/* Frame a complete instruction at a separately proven code boundary, requiring
 * unchanged physical mapping before each source read. Observe every valid code
 * nibble consumed by the decoder. The reader uses the before mapping; caller
 * retains both maps' dependencies, code provenance and exclusive lifetime.
 * limit is the bounded code extent remaining, not an end address. Errors leave
 * length unchanged and invalidate partial observations. This checks instruction
 * fetch continuity, not executable ownership, instruction semantics, subsequent
 * branches, interrupt handling or permission to mutate native storage. */
int hpclang_rpl_execution_instruction_preserved(const struct hpclang_rpl_reader *reader,
                                                const uint32_t before[256],
                                                const uint32_t after[256], uint32_t address,
                                                uint32_t limit, uint32_t *length,
                                                hpclang_rpl_reference_dependency_fn dependency,
                                                void *context);
#endif
