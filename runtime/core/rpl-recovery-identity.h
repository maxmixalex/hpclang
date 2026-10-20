#ifndef HPCLANG_RPL_RECOVERY_IDENTITY_H
#define HPCLANG_RPL_RECOVERY_IDENTITY_H

#include "rpl-object-span.h"

#define HPCLANG_RPL_RECOVERY_IDENTITY_BYTES 48u

/* Seal an exclusively owned packed library template in RAM. Coordinates are
 * object nibbles, without an allocation envelope. Require an untouched
 * HPCROOT1 + 40 zero bytes placeholder at the supplied trusted layout offset.
 * Fill its span/offset/digest and native CRC exactly as the host packager does.
 * Preserve allocation padding, including an unused high nibble. Return 1 on
 * success, 0 on invalid input, without changes on failure. No heap or Flash IO.
 * This checks the outer library/placeholder bounds, not command provenance or
 * arbitrary library structure. Only use trusted owned templates; sealing is
 * content identity, never installation ownership or publication permission. */
int hpclang_rpl_recovery_identity_seal(uint8_t *object, uint32_t bytes, uint32_t identity_nibble);

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
