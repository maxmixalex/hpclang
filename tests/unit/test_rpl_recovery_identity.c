#include "rpl-recovery-identity.h"
#include <assert.h>
#include <string.h>
#include "recovery-identity-vectors.h"

static uint8_t data[512];
static unsigned span, reads, fail_read, invalid_read;
static int read_nibble(void *context, uint32_t at, uint8_t *value)
{
    assert(context == data && at >= 123 && at - 123 < span);
    if (++reads == fail_read)
        return 0;
    at -= 123;
    *value = reads == invalid_read ? 16 : (data[at / 2] >> (4 * (at & 1))) & 15;
    return 1;
}
static void put(unsigned at, uint8_t value)
{
    unsigned shift = 4 * (at & 1);
    data[at / 2] = (data[at / 2] & ~(15u << shift)) | value << shift;
}
static void repair_crc(void)
{
    unsigned crc = 0;
    for (unsigned i = 5; i < span - 4; ++i) {
        unsigned value = (data[i / 2] >> (4 * (i & 1))) & 15;
        for (unsigned bit = 0; bit < 4; ++bit) {
            unsigned carry = (crc ^ (value >> bit)) & 1;
            crc >>= 1;
            if (carry)
                crc ^= 0x8408;
        }
    }
    for (unsigned i = 0; i < 4; ++i)
        put(span - 4 + i, (crc >> (4 * i)) & 15);
}
int main(void)
{
    const struct hpclang_rpl_reader reader = {read_nibble, data};
    for (unsigned v = 0; v < sizeof vectors / sizeof vectors[0]; ++v) {
        span = vectors[v].span;
        memcpy(data, vectors[v].data, sizeof data);
        reads = fail_read = invalid_read = 0;
        assert(hpclang_rpl_recovery_identity_match(&reader, 123, vectors[v].descriptor) == 1);
        assert(reads == span);
        /* Native allocation padding is not part of content identity. */
        if (span & 1) {
            data[span / 2] ^= 0xf0;
            assert(hpclang_rpl_recovery_identity_match(&reader, 123, vectors[v].descriptor) == 1);
        }
        for (unsigned i = 1; i <= span; ++i) {
            reads = 0;
            fail_read = i;
            assert(hpclang_rpl_recovery_identity_match(&reader, 123, vectors[v].descriptor) == -1);
        }
        fail_read = 0;
        reads = 0;
        invalid_read = span / 2;
        assert(hpclang_rpl_recovery_identity_match(&reader, 123, vectors[v].descriptor) == -1);
        invalid_read = 0;
        for (unsigned i = 0; i < span - 4; ++i) {
            memcpy(data, vectors[v].data, sizeof data);
            data[i / 2] ^= 1u << (4 * (i & 1));
            repair_crc(); /* A newly valid native CRC cannot hide changed content. */
            assert(hpclang_rpl_recovery_identity_match(&reader, 123, vectors[v].descriptor) == 0);
        }
        uint8_t descriptor[48];
        memcpy(descriptor, vectors[v].descriptor, sizeof descriptor);
        descriptor[0] ^= 1;
        assert(hpclang_rpl_recovery_identity_match(&reader, 123, descriptor) == -1);
        assert(hpclang_rpl_recovery_identity_match(&reader, UINT32_MAX, vectors[v].descriptor) ==
               -1);
        assert(hpclang_rpl_recovery_identity_match(NULL, 123, vectors[v].descriptor) == -1);
    }
    return 0;
}
