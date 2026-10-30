#ifndef HPCLANG_RPL_EXECUTION_MAP_H
#define HPCLANG_RPL_EXECUTION_MAP_H
#include <stdint.h>
/* Prove that every logical nibble in a nonempty execution range retains its
 * physical location across two separately qualified read maps. Reject invalid
 * pages, bounds and remapping, including equal-byte/different-physical aliases.
 * Discontinuous physical runs are supported. Zero succeeds; negative span
 * errors reject. No target reads or writes. Caller retains map/code guards and
 * exclusive lifetime. This is one continuation condition, not a proof of code
 * identity, register/return roots, instruction boundaries or execution safety. */
int hpclang_rpl_execution_mapping_preserved(const uint32_t before[256], const uint32_t after[256],
                                            uint32_t address, uint32_t span);
#endif
