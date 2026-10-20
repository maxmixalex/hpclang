#ifndef HPCLANG_RECOVERY_BINDING_H
#define HPCLANG_RECOVERY_BINDING_H

#include <stdint.h>
#include "rpl-flash-record.h"

/* Internal packaging ABI; these coordinates do not establish ownership. */
struct hpclang_recovery_binding
{
    uint32_t magic[2], journal_slot, authority_slot;
};
_Static_assert(sizeof(struct hpclang_recovery_binding) == 16, "Recovery packager binding ABI");
#define HPCLANG_RECOVERY_BINDING_INITIALIZER                                                       \
    {{0x42435048u, 0x31444e49u},                                                                   \
     HPCLANG_RECOVERY_BOUND_JOURNAL_SLOT,                                                          \
     HPCLANG_RECOVERY_BOUND_JOURNAL_SLOT + HPCLANG_RPL_FLASH_SECTOR_RECORD_BYTES}

struct hpclang_recovery_reservation_binding
{
    uint32_t magic[2], capacity, name_length;
    uint8_t name[128];
};
_Static_assert(sizeof(struct hpclang_recovery_reservation_binding) == 144,
               "Recovery reservation packager ABI");
#define HPCLANG_RECOVERY_RESERVATION_INITIALIZER {{0x52435048u, 0x31565345u}, 0, 0, {0}}

#endif
