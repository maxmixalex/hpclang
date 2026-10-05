#ifndef HPCLANG_HP39_APLET_FRAME_H
#define HPCLANG_HP39_APLET_FRAME_H

#include "rpl-object-span.h"

/* Locate the data cell only under the verified HPGCC39 _Action protocol.
 * incoming_span is zero before pinning, otherwise the original EDITLINE minus
 * DSKTOP. launcher_hidden comes from the bootstrap's explicit launcher-bias
 * record, never from
 * guessing the application's data type. The returned slot
 * is invocation-local and must be
 * resolved after GC. */
int hpclang_hp39_aplet_data_slot(const struct hpclang_rpl_reader *reader, uint32_t incoming_span,
                                 int launcher_hidden, uint32_t *slot);

#endif
