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

/* Prepare the same owned draft with all four extents. claim_nibble is the
 * immutable trusted layout's admission_creation_nibble, including the native
 * allocation envelope. It must name the admission ELF's owned field; it is not
 * discovered by scanning. Rebind that field before resealing, then revalidate
 * all four placements. Outputs must be disjoint and change only on success;
 * staging is disposable on failure. The prepare ownership/exclusion contract
 * above applies. This publishes no creation record or installation authority. */
int hpclang_rpl_recovery_installation_prepare_creation(
    const struct hpclang_rpl_flash_io *io, const struct hpclang_rpl_flash_recovery_layout *layout,
    uint32_t identity_nibble, uint32_t claim_nibble,
    const struct hpclang_rpl_recovery_port_view ports[2],
    const struct hpclang_rpl_flash_catalog *catalog, uint32_t target, uint8_t *staging,
    uint32_t capacity, struct hpclang_rpl_flash_recovery_space *space, uint32_t *claim_slot);

/* Explicit mutable-buffer preparation with identical placement/sealing checks.
 * owned must equal layout->record exactly; partial overlap or a separate buffer
 * is rejected. Caller exclusively owns writable storage of at least capacity
 * bytes and retains the trusted layout separately. No heap or Flash writes.
 * On failure the owned template may have been modified and must be discarded;
 * space/claim_slot change only on success. Existing copying APIs still reject
 * all aliasing. This neither publishes records nor proves restart ownership. */
int hpclang_rpl_recovery_installation_prepare_creation_in_place(
    const struct hpclang_rpl_flash_io *io, const struct hpclang_rpl_flash_recovery_layout *layout,
    uint32_t identity_nibble, uint32_t claim_nibble,
    const struct hpclang_rpl_recovery_port_view ports[2],
    const struct hpclang_rpl_flash_catalog *catalog, uint32_t target, uint8_t *owned,
    uint32_t capacity, struct hpclang_rpl_flash_recovery_space *space, uint32_t *claim_slot);

/* Prepare fresh transaction coordinates for an already registered inert root.
 * The original source template and its identity/field offsets are trusted and
 * stable. Reconstruct its fresh four-extent placement with only that candidate
 * root virtually erased, then compare the entire resealed record and unique
 * catalog binding against ACTUAL Flash. Other extents must still be erased;
 * occupied slots, changed placement and existing transactions are not adopted.
 * No writes, resource creation, release or restart-ownership authority. The
 * exclusive mapping/catalog contract applies throughout. Source and staging
 * cannot overlap; staging is disposable after failure. Coordinate outputs change
 * only on success. This does not register a library or establish early startup. */
int hpclang_rpl_recovery_installation_prepare_registered_creation(
    const struct hpclang_rpl_flash_io *io, const struct hpclang_rpl_flash_recovery_layout *layout,
    uint32_t identity_nibble, uint32_t claim_nibble,
    const struct hpclang_rpl_flash_catalog *catalog, uint32_t target, uint8_t *staging,
    uint32_t capacity, struct hpclang_rpl_flash_recovery_space *space, uint32_t *claim_slot);

/* Same registered-root checks, reusing exactly layout->record as owned storage.
 * Partial overlap and distinct buffers are rejected. The draft is disposable
 * after failure; coordinate outputs still change only on complete success. */
int hpclang_rpl_recovery_installation_prepare_registered_creation_in_place(
    const struct hpclang_rpl_flash_io *io, const struct hpclang_rpl_flash_recovery_layout *layout,
    uint32_t identity_nibble, uint32_t claim_nibble,
    const struct hpclang_rpl_flash_catalog *catalog, uint32_t target, uint8_t *owned,
    uint32_t capacity, struct hpclang_rpl_flash_recovery_space *space, uint32_t *claim_slot);

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

/* Classify a creation claim at the coordinate supplied by the SAME sealed
 * executing root. expected_binding is its exact HPCMEM2 installation/transaction
 * binding, constructed from trusted inputs, not copied from the claim being
 * checked. Revalidate the installed transaction and CRC, compare the complete
 * claim, then require its exact deleted allocation boundary in the validated
 * restored view (or actual Flash for a closed transaction). No signature scan,
 * heap or writes. Return flash results; phase changes only on success (0).
 * INTENT/CREATED/CLOSING/CLOSED are classifications, not release permission.
 * Stable exclusive views, producer provenance, startup ordering and one-use
 * lifetime remain caller obligations. This does not admit unpublished journals.
 */
int hpclang_rpl_recovery_installation_creation(const struct hpclang_rpl_flash_io *io,
                                               uint32_t library_number,
                                               const struct hpclang_rpl_flash_catalog *catalog,
                                               const uint8_t expected_binding[80],
                                               uint32_t claim_slot, const uint8_t *name,
                                               uint32_t name_length,
                                               enum hpclang_rpl_flash_creation_phase *phase);

/* Read-only retirement prerequisite for the SAME trusted executing root/binding.
 * Require a fully closed authority and retired journal, then validate identity,
 * catalog and exact claim allocation against actual Flash (never replayed data).
 * Accept CREATED/CLOSING/CLOSED claims; output changes only on success (0).
 * No RAM inspection, writes, release, ownership reconstruction or reclamation
 * permission. Callers must separately prove resource absence or safe release,
 * durable provenance and execution safety before cleanup. Retain the exclusive
 * stable-view and trusted-input contract of creation() above. */
int hpclang_rpl_recovery_installation_creation_retired(
    const struct hpclang_rpl_flash_io *io, uint32_t library_number,
    const struct hpclang_rpl_flash_catalog *catalog, const uint8_t expected_binding[80],
    uint32_t claim_slot, const uint8_t *name, uint32_t name_length,
    enum hpclang_rpl_flash_creation_phase *phase);

/* Close a retired installation's claim after validating absence in Stock HOME.
 * The binding/name must come from the SAME independently trusted installed root,
 * with durable producer provenance and no allocation reuse; never reconstruct
 * that authority from a surviving claim or similarly named RAM object. Keep
 * Flash/catalog/RAM views exclusive and stable throughout. Recheck retirement
 * and native boundaries, reject any existing same-name object or malformed RAM,
 * then clear only the claim phase. No RAM release, erase, root removal or space
 * reuse. CREATED/CLOSING/CLOSED support retry; DIRTY forbids assuming closure.
 * This supplies post-recovery claim closure, not installation trust discovery,
 * early startup ordering or general orphan-resource cleanup. */
int hpclang_rpl_recovery_installation_close_retired_creation(
    const struct hpclang_rpl_flash_io *io, uint32_t library_number,
    const struct hpclang_rpl_flash_catalog *catalog, const uint8_t expected_binding[80],
    uint32_t claim_slot, const uint8_t *name, uint32_t name_length,
    const struct hpclang_rpl_reader *ram);

/* Streaming staged-transaction validation. Require exact authority 7F and READY
 * journal, then validate both intact current and proposed views, including root
 * identity/catalog and native pair boundaries. No heap or writes. Return 0 and
 * update installed only on success. This grants no activation/ownership. */
int hpclang_rpl_recovery_installation_staged(
    const struct hpclang_rpl_flash_io *io, uint32_t library_number,
    const struct hpclang_rpl_flash_catalog *catalog, const uint8_t expected[48], uint32_t authority,
    uint32_t journal, struct hpclang_rpl_flash_installed_recovery *installed);

/* Read-only pre-activation gate under the same trusted binding/claim contract.
 * Require exact staged authority 7F, a READY journal and a CREATED claim. Validate
 * root/catalog and all three allocation boundaries against BOTH intact current
 * Flash and the proposed target image. Staging cannot justify current damage.
 * No scratch sector, heap, writes or release permission. Caller must separately
 * retain original installation ownership and revalidate the fresh RAM receipt;
 * this proves consistent state, not provenance or safe startup ordering. */
int hpclang_rpl_recovery_installation_creation_staged(
    const struct hpclang_rpl_flash_io *io, uint32_t library_number,
    const struct hpclang_rpl_flash_catalog *catalog, const uint8_t expected_binding[80],
    uint32_t claim_slot, const uint8_t *name, uint32_t name_length);

#endif
