#ifndef HPCLANG_RPL_RECOVERY_INSTALLATION_H
#define HPCLANG_RPL_RECOVERY_INSTALLATION_H

#include "rpl-flash-write.h"
#include "rpl-recovery-identity.h"
#include "rpl-flash-replay.h"

/* Resolve a unique live library/catalog binding and compare its complete
 * content with the descriptor in the trusted executing admission image.
 * Read-only; no heap; output changes only on success (0). Other results use
 * hpclang_rpl_flash_result. Mapping, descriptor and catalog remain stable.
 * This supplies installed content identity, not durable installation provenance,
 * one-use transaction ownership, startup ordering or RAM release permission. */
int hpclang_rpl_recovery_installation_match(const struct hpclang_rpl_flash_io *io,
                                            uint32_t library_number,
                                            const struct hpclang_rpl_flash_catalog *catalog,
                                            const uint8_t expected[48], uint32_t *slot);

/* Admission for a pair read from the SAME trusted executing image as expected.
 * Validate the bound transaction before using its image for read-only library/
 * catalog checks. The entire recovery allocation must remain outside target.
 * Closed transactions check actual Flash, never a historical image. Scratch
 * and return phases follow preflight_bound; output changes only on success.
 * Scratch must not overlap the expected identity, output or IO/catalog state.
 * No write, RAM release or ownership/startup authorization is granted. */
int hpclang_rpl_recovery_installation_preflight(
    const struct hpclang_rpl_flash_io *io, uint32_t library_number,
    const struct hpclang_rpl_flash_catalog *catalog, const uint8_t expected[48], uint32_t authority,
    uint32_t journal, uint8_t *scratch, uint32_t capacity,
    struct hpclang_rpl_flash_installed_recovery *installed);

#endif
