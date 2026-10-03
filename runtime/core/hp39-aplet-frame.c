#include <errno.h>
#include <stddef.h>
#include "hp39-aplet-frame.h"

static int word(const struct hpclang_rpl_reader *reader, uint32_t address, uint32_t count,
                uint32_t *value)
{
    if (reader == NULL || reader->read_nibble == NULL || address >= 0x100000u ||
        count > 0x100000u - address)
        return 0;
    uint32_t result = 0u;
    for (uint32_t i = 0u; i < count; ++i) {
        uint8_t nibble = 0u;
        if (!reader->read_nibble(reader->context, address + i, &nibble) || nibble > 15u)
            return 0;
        result |= (uint32_t)nibble << (4u * i);
    }
    *value = result;
    return 1;
}

int hpclang_hp39_aplet_data_slot(const struct hpclang_rpl_reader *reader, uint32_t incoming_span,
                                 uint32_t *slot)
{
    if (slot == NULL) {
        errno = EINVAL;
        return -1;
    }
    uint32_t top, end, returns, continuation;
    if (!word(reader, 0x806f8u, 5u, &top) || !word(reader, 0x806fdu, 5u, &end) ||
        !word(reader, 0x806f3u, 5u, &returns)) {
        errno = EIO;
        return -1;
    }
    if (top < 0x80000u || end > 0x100000u || end < top || (end - top) % 5u != 0u ||
        returns < 0x80005u || returns > top || incoming_span > end - top ||
        incoming_span % 5u != 0u) {
        errno = EILSEQ;
        return -1;
    }
    if (incoming_span != 0u)
        top = end - incoming_span;
    if (end - top < 20u || !word(reader, returns - 5u, 5u, &continuation) || continuation < 36u ||
        continuation > 0x100000u - 15u) {
        errno = ENOSYS;
        return -1;
    }

    /* HPGCC39 _Action, confirmed in both source and the packaged template:
     * :: TopicVar2@ TopicVar1@ LastBut3 CARCOMP xPrRUN TopicVar1! DROP ;
     * xPrRUN is an inline 6-nibble ROMPTR (library 275, command 0).
     * Check instructions, not a fixed RAM address, title or assumed stack type. */
    static const uint32_t protocol[] = {
        0x02d9du, 0x12cfdu,  0x12cfcu, 0x2892du, 0x05842u,
        0x02e92u, 0x000113u, 0x12ca1u, 0x03244u, 0x0312bu,
    };
    uint32_t address = continuation - 36u;
    for (uint32_t i = 0u; i < sizeof(protocol) / sizeof(protocol[0]); ++i) {
        uint32_t actual;
        uint32_t width = i == 6u ? 6u : 5u;
        if (!word(reader, address, width, &actual) || actual != protocol[i]) {
            errno = ENOSYS;
            return -1;
        }
        address += width;
    }
    uint32_t object, type, view;
    if (!word(reader, top, 5u, &object) || !word(reader, object, 5u, &type) || type != 0x02a2cu ||
        !word(reader, top + 10u, 5u, &object) || !word(reader, object, 5u, &type) ||
        type != 0x02911u || !word(reader, object + 5u, 5u, &view) || view > 7u) {
        errno = ENOSYS;
        return -1;
    }
    *slot = top + 5u;
    return 0;
}
