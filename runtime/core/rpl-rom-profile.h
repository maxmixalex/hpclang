#ifndef HPCLANG_RPL_ROM_PROFILE_H
#define HPCLANG_RPL_ROM_PROFILE_H

#include <stdint.h>
#include "rpl-object-span.h"

/* Physical byte reader: fill exactly length bytes and return zero, or a negative
 * span error. Positive errors are MALFORMED. Caller qualifies read permissions,
 * physical identity and exclusive lifetime; no MMIO/Flash commands are issued. */
typedef int (*hpclang_rpl_rom_read_fn)(void *context, uint32_t offset, uint8_t *bytes,
                                       uint32_t length);
/* SHA-256 with reads of at most 64 bytes, no allocation, output unchanged on error. */
int hpclang_rpl_rom_digest(hpclang_rpl_rom_read_fn read, void *context, uint32_t bytes,
                           uint8_t digest[32]);

struct hpclang_rpl_rom_profile
{
    uint32_t model, flash_bytes, immutable_bytes, geometry_operands, geometry_bases;
};

/* Match the complete immutable prefix of the two reviewed HP49g+/HP50g images.
 * User Flash [0x140000,0x200000) is excluded. This is content identification,
 * not a signature/trust chain, native-state proof or mutation authorization.
 * Unknown model/content and reader failures leave *profile unchanged. On zero,
 * return a static profile valid while the matched Flash/mapping remains stable.
 * Caller still qualifies CPU-state identity, routing, interrupts and roots.
 * Never infer these fixed layout locations merely from a plausible directory. */
int hpclang_rpl_rom_profile_identify(hpclang_rpl_rom_read_fn read, void *context, uint32_t model,
                                     const struct hpclang_rpl_rom_profile **profile);
#endif
