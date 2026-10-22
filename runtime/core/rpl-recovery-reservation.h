#ifndef HPCLANG_RPL_RECOVERY_RESERVATION_H
#define HPCLANG_RPL_RECOVERY_RESERVATION_H

#include "rpl-mutation.h"
#include "rpl-flash-replay.h"
#include "rpl-recovery-identity.h"

#define HPCLANG_RPL_RECOVERY_RESERVATION_HEADER_BYTES 80u

/* Serialize the already validated installation, one-use transaction and RAM
 * capacity into a string payload header. identity and installed must describe
 * the same trusted, stable view; journal_crc is its status-independent capsule
 * CRC. This records binding, not proof of creation or authorization to release.
 * Fresh creation, durable publication ordering and exclusion of intervening
 * user/native writers remain caller obligations. Never persist a RAM address.
 * Returns 1 on success, 0 on invalid arguments; output changes only on success.
 */
int hpclang_rpl_recovery_reservation_header(
    const uint8_t identity[HPCLANG_RPL_RECOVERY_IDENTITY_BYTES],
    const struct hpclang_rpl_flash_installed_recovery *installed, uint32_t journal_crc,
    uint32_t capacity, uint8_t header[HPCLANG_RPL_RECOVERY_RESERVATION_HEADER_BYTES]);

/* Compare a native string's exact capacity and complete header against the
 * current trusted binding. The remaining payload is disposable workspace.
 * Returns 1 for a match, 0 for a different type/capacity/header, -1 for invalid
 * input or a read failure. No writes or RAM-release permission. A caller must
 * independently prove creation/publication/lifetime and startup ordering.
 */
int hpclang_rpl_recovery_reservation_match(
    const struct hpclang_rpl_reader *reader, uint32_t object,
    const uint8_t header[HPCLANG_RPL_RECOVERY_RESERVATION_HEADER_BYTES]);

struct hpclang_rpl_recovery_reservation
{
    uint32_t object, capacity;
};

/* Require Stock's oldest, inline null-named hidden directory in HOME, create a
 * new HOME string, then relocate HOME and recheck that prerequisite and object.
 * A missing hidden directory returns UNSUPPORTED without allocation or writes;
 * this API does not invoke Stock initialization or repair a malformed HOME.
 * Existing names, including identical contents, are never adopted. The caller
 * owns the native view, roots and immutable snapshot throughout. Output changes
 * only on success. Its address is valid only while that ownership continues;
 * it is not a persistent identity or permission to release RAM after restart.
 *
 * Returns mutation results. Any failure after a native write attempt returns
 * INCOMPLETE: retain execution/IRQ ownership, do not publish Flash or resume
 * Stock. This is not rollback. Preflight failures make no native writes.
 * The verified resource must still be bound to a durable installation and
 * transaction before destructive target writes.
 */
int hpclang_rpl_recovery_reservation_create(const struct hpclang_rpl_mutation_io *io,
                                            const uint8_t *name, uint32_t name_length,
                                            const struct hpclang_object *snapshot,
                                            struct hpclang_rpl_recovery_reservation *created);

/* Discard an untouched reservation created by this same exclusive invocation,
 * after cancelling any associated unpublished Flash transaction. The original
 * snapshot, name and creation receipt must remain stable; no native return,
 * other writer, relocation or reuse may intervene. Resolve HOME again and
 * require the exact receipt address, capacity and full original string before
 * removal. Matching bytes alone are not a receipt and never authorize adoption.
 * This is cancellation cleanup, not active-recovery workspace release or
 * reconstruction of ownership after reset. It does not inspect Flash phases.
 *
 * Success consumes the receipt (both fields zero). Pre-write errors leave it
 * and native memory unchanged. Any error after a write attempt is INCOMPLETE:
 * retain the invocation, never resume Stock or retry with the stale receipt.
 * Existing native references are preserved by normal directory removal.
 */
int hpclang_rpl_recovery_reservation_discard_created(
    const struct hpclang_rpl_mutation_io *io, const uint8_t *name, uint32_t name_length,
    const struct hpclang_object *snapshot, struct hpclang_rpl_recovery_reservation *created);

#endif
