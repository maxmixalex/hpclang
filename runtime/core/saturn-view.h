#ifndef HPCLANG_RUNTIME_SATURN_VIEW_H
#define HPCLANG_RUNTIME_SATURN_VIEW_H

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /*
     * Runtime-private Saturn nibble address.
     *
     * This is deliberately not an ARM pointer and is never part of the public
     * hpclang application ABI. M7.3 object services use it internally so Saturn
     * address arithmetic cannot be confused with native pointer arithmetic.
     */
    struct hpclang_saturn_address
    {
        uint32_t nibble;
    };

    static inline struct hpclang_saturn_address hpclang_saturn_address_make(uint32_t nibble)
    {
        struct hpclang_saturn_address address = {nibble};
        return address;
    }

    static inline struct hpclang_saturn_address
    hpclang_saturn_address_add(struct hpclang_saturn_address address, uint32_t nibble_offset)
    {
        address.nibble += nibble_offset;
        return address;
    }

    /* Return 1 on success, 0 when the live Stock mapping is unavailable. */
    int hpclang_saturn_read_nibble(struct hpclang_saturn_address address, uint8_t *value);

    /*
     * Read one byte encoded as two consecutive Saturn nibbles, low nibble first.
     * The helper deliberately performs two mapped reads so page crossings are
     * handled correctly.
     */
    int hpclang_saturn_read_byte(struct hpclang_saturn_address address, uint8_t *value);

#ifdef __cplusplus
}
#endif

#endif
