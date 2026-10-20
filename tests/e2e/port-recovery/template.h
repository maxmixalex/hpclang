#ifndef HPCLANG_RECOVERY_TEMPLATE_WITNESS_H
#define HPCLANG_RECOVERY_TEMPLATE_WITNESS_H

#include "rpl-object-span.h"

/* Generated from the real recovery/admission programs at build time. Keep the
 * declarations available during analysis without requiring generated headers. */
#pragma GCC visibility push(hidden)
extern uint8_t expected_library[], prepared_library[];
extern const uint8_t expected_identity[48];
extern const uint32_t preparation_bytes, template_span;
extern const uint32_t template_recovery_binding, template_admission_binding, template_identity;
#pragma GCC visibility pop
#define EXPECTED_LIBRARY_BYTES preparation_bytes
#define EXPECTED_RECOVERY_BINDING template_recovery_binding
#define EXPECTED_ADMISSION_BINDING template_admission_binding
#define EXPECTED_IDENTITY_NIBBLE template_identity
#define EXPECTED_TARGET 0x140000u

int hpclang_installation_load_template(const struct hpclang_rpl_reader *reader);

#endif
