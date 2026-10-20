#ifndef HPCLANG_RPL_RECOVERY_INSTALLATION_H
#define HPCLANG_RPL_RECOVERY_INSTALLATION_H

#include "rpl-flash-write.h"
#include "rpl-recovery-identity.h"

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

#endif
