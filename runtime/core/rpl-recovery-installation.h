#ifndef HPCLANG_RPL_RECOVERY_INSTALLATION_H
#define HPCLANG_RPL_RECOVERY_INSTALLATION_H

#include "rpl-flash-write.h"
#include "rpl-recovery-identity.h"
#include "rpl-flash-replay.h"

struct hpclang_rpl_recovery_port_view
{
    const struct hpclang_rpl_reader *reader;
    uint32_t first, end;
};

/* Plan a fresh recovery library's Flash location only after excluding its ID
 * from both complete RAM-port chains and the strictly ordered native catalog.
 * The caller supplies actual Port 0/1 mappings, including readable terminators,
 * and keeps every view stable through use of the result. Empty ports still
 * require a valid terminated view; NULL does not mean absent.
 * No writes or heap allocation. Output changes only on success (0); errors use
 * flash_result. Malformed/unreadable later records are not hidden by a match.
 * This does not reserve space, discover mappings, choose an early startup ID,
 * register the library or grant durable installation/publication authority. */
int hpclang_rpl_recovery_installation_plan(const struct hpclang_rpl_flash_io *io,
                                           const uint8_t *record, uint32_t bytes,
                                           const struct hpclang_rpl_recovery_port_view ports[2],
                                           const struct hpclang_rpl_flash_catalog *catalog,
                                           uint32_t *slot);

/* The same namespace check, followed by placement of the library and its fresh
 * journal/authority pair outside target. All views remain stable across both
 * checks. Output changes only on success; storage and authority are not reserved.
 * Seal and revalidate the final descriptors before any owned publication. */
int hpclang_rpl_recovery_installation_plan_space(
    const struct hpclang_rpl_flash_io *io, const uint8_t *record, uint32_t bytes,
    const struct hpclang_rpl_recovery_port_view ports[2],
    const struct hpclang_rpl_flash_catalog *catalog, uint32_t target,
    struct hpclang_rpl_flash_recovery_space *space);

/* Prepare a fresh owned installation record in caller-owned staging RAM.
 * The immutable trusted layout names both HPCBIND1 descriptors; identity_nibble
 * excludes the allocation envelope. Validate the original sealed identity,
 * plan space with namespace exclusion, copy/update both bindings, reseal, and
 * repeat placement validation before publishing space. No native writes.
 * staging must be disjoint from the source and every input/output/view. On
 * failure space is unchanged, but staging is disposable and may be modified.
 * Keep mappings and exclusive ownership stable throughout. The resulting draft
 * is not a storage reservation, library registration or publication authority. */
int hpclang_rpl_recovery_installation_prepare(
    const struct hpclang_rpl_flash_io *io, const struct hpclang_rpl_flash_recovery_layout *layout,
    uint32_t identity_nibble, const struct hpclang_rpl_recovery_port_view ports[2],
    const struct hpclang_rpl_flash_catalog *catalog, uint32_t target, uint8_t *staging,
    uint32_t capacity, struct hpclang_rpl_flash_recovery_space *space);

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

/* Same stable-view identity/admission contract without a sector scratch buffer.
 * CRC-check the complete capsule, then read its image directly through the same
 * exclusive Flash view. Validate exact pair membership and all native banks.
 * Closing/retiring states require target equality; closed uses actual Flash.
 * No heap, writes, RAM release or new ownership/startup authority. */
int hpclang_rpl_recovery_installation_stream(
    const struct hpclang_rpl_flash_io *io, uint32_t library_number,
    const struct hpclang_rpl_flash_catalog *catalog, const uint8_t expected[48], uint32_t authority,
    uint32_t journal, struct hpclang_rpl_flash_installed_recovery *installed);

#endif
