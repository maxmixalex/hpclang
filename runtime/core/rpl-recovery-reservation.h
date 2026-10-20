#ifndef HPCLANG_RPL_RECOVERY_RESERVATION_H
#define HPCLANG_RPL_RECOVERY_RESERVATION_H

#include "rpl-mutation.h"

struct hpclang_rpl_recovery_reservation
{
    uint32_t object, capacity;
};

/* Create a new HOME string, then relocate HOME and verify the whole object.
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

#endif
