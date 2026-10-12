#ifndef HPCLANG_RUNTIME_RPL_PORT_MUTATION_H
#define HPCLANG_RUNTIME_RPL_PORT_MUTATION_H

#include "rpl-mutation.h"

/* Prepend one validated packed backup without moving existing port objects.
 * Duplicate names are rejected. The input is owned ARM memory and must remain
 * stable. Uses the same preflight/commit I/O contract as USEROB mutation. */
int hpclang_rpl_port0_create_backup(const struct hpclang_rpl_mutation_io *io, const uint8_t *packed,
                                    uint32_t span);

#endif
