#ifndef HPCLANG_RUNTIME_RPL_BACKUP_H
#define HPCLANG_RUNTIME_RPL_BACKUP_H

#include "rpl-object-span.h"

/* Invocation-local native coordinates; never an application-owned snapshot. */
struct hpclang_rpl_backup_view
{
    uint32_t span;
    uint32_t name_address;
    uint32_t name_length;
    uint32_t value_address;
    uint32_t value_span;
    uint32_t checksum;
};

/* Validate the backup envelope, contained object extent and native CRC.
 * This does not establish port availability or write
 * permission. Output is unchanged on failure; returns hpclang_rpl_span_result.
 */
int hpclang_rpl_backup_view(const struct hpclang_rpl_reader *reader, uint32_t address,
                            uint32_t limit, struct hpclang_rpl_backup_view *view);

enum
{
    HPCLANG_RPL_BACKUP_NOMEM = -6
};

/* Build an owned packed DOBAK from one bounded source object. The caller frees
 * *packed. Output arguments are unchanged on failure. The source must remain
 * stable throughout this call; no native memory is written. Result codes are
 * hpclang_rpl_span_result or HPCLANG_RPL_BACKUP_NOMEM. */
int hpclang_rpl_backup_encode(const struct hpclang_rpl_reader *reader, uint32_t address,
                              uint32_t value_span, const uint8_t *name, uint32_t name_length,
                              uint8_t **packed, uint32_t *span);

#endif
