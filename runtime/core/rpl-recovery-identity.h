#ifndef HPCLANG_RPL_RECOVERY_IDENTITY_H
#define HPCLANG_RPL_RECOVERY_IDENTITY_H

#include "rpl-object-span.h"

#define HPCLANG_RPL_RECOVERY_IDENTITY_BYTES 48u

/* Compare a library object with the HPCROOT1 descriptor held in the executing
 * admission image. Object coordinates exclude any native allocation envelope.
 * Check the encoded length, embedded descriptor, native CRC and canonical
 * SHA-256; only digest/CRC fields are zeroed when hashing. No heap or writes.
 * Return 1 for matching content, 0 for mismatch, -1 for invalid input/read error.
 * Caller keeps the descriptor, object and mapping stable, and separately proves
 * live allocation membership, unique catalog identity and trusted execution.
 * Content identity alone grants no ownership, startup order or erase/release
 * permission. This does not authenticate a replacement executing program. */
int hpclang_rpl_recovery_identity_match(const struct hpclang_rpl_reader *reader, uint32_t object,
                                        const uint8_t expected[48]);

#endif
