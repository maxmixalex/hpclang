#ifndef HPCLANG_RPL_FLASH_RECORD_H
#define HPCLANG_RPL_FLASH_RECORD_H

#include "rpl-backup.h"

/* Encode the allocation envelope around an exact, CRC-validated backup.
 * Coordinates in the input reader are nibbles; the owned output is bytes.
 * Matches native HP49/50 STO records: status 53, relative extent, metadata 1,
 * packed object, then erased padding to a whole number of halfwords.
 * This only builds bytes; it neither reserves space nor commits a record.
 * The source stays stable throughout validation/copy. Outputs are unchanged
 * on failure. Free *record after use. */
int hpclang_rpl_flash_backup_record(const struct hpclang_rpl_reader *reader, uint32_t address,
                                    uint32_t span, uint8_t **record, uint32_t *bytes);

#endif
