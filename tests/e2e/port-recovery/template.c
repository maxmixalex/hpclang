/* Load a separate real recovery template into the witness-owned CODE reserve.
 * Identity is compiled from the known build, never learned from user input. */
#include "template.h"
#include "rpl-object-store.h"
#include "rpl-recovery-identity.h"

volatile uint32_t template_result[3];

int hpclang_installation_load_template(const struct hpclang_rpl_reader *reader)
{
    uint32_t home;
    struct hpclang_rpl_directory_entry_view entry;
    if (hpclang_rpl_store_home_object(reader, &home) ||
        hpclang_rpl_directory_find(reader, home, (const uint8_t *)"HPCTEMPLATE", 11, &entry) ||
        entry.prologue != 0x2b40u)
        return 0;
    template_result[0] = entry.object_address;
    template_result[1] = (uint32_t)hpclang_rpl_recovery_identity_match(reader, entry.object_address,
                                                                       expected_identity);
    if (template_result[1] != 1)
        return 0;
    for (unsigned i = 0; i < preparation_bytes; ++i)
        expected_library[i] = 255;
    expected_library[0] = 0x53;
    unsigned extent = (preparation_bytes * 2u - 2u) | 0x100000u;
    for (unsigned i = 0; i < 3; ++i)
        expected_library[1 + i] = (uint8_t)(extent >> (8 * i));
    for (unsigned i = 0; i < template_span; ++i) {
        uint8_t nibble;
        if (!reader->read_nibble(reader->context, entry.object_address + i, &nibble))
            return 0;
        unsigned shift = 4 * (i & 1);
        expected_library[4 + i / 2] =
            (expected_library[4 + i / 2] & ~(15u << shift)) | (nibble << shift);
    }
    template_result[2] = preparation_bytes;
    return 1;
}
