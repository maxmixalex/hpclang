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

static void seal_tests(unsigned v)
{
    const uint8_t *descriptor = vectors[v].descriptor;
    uint32_t at = descriptor[12] | (uint32_t)descriptor[13] << 8 | (uint32_t)descriptor[14] << 16 |
                  (uint32_t)descriptor[15] << 24;
    memcpy(data, vectors[v].data, sizeof data);
    if (span & 1)
        data[span / 2] |= 0xf0;
    uint8_t expected[sizeof data], untouched[sizeof data];
    memcpy(expected, data, sizeof data);
    for (unsigned i = 16; i < 96; ++i)
        put(at + i, 0);
    /* Existing native CRC bytes must not enter the canonical hash. */
    for (unsigned i = span - 4; i < span; ++i)
        put(i, 15);
    memcpy(untouched, data, sizeof data);
    assert(hpclang_rpl_recovery_identity_seal(data, sizeof data, at) == 1);
    assert(!memcmp(data, expected, sizeof data));
    assert(!hpclang_rpl_recovery_identity_seal(data, sizeof data, at));
    assert(!memcmp(data, expected, sizeof data));
    for (unsigned fault = 0; fault < 8; ++fault) {
        memcpy(data, untouched, sizeof data);
        uint32_t offset = at, capacity = sizeof data;
        if (fault == 0)
            put(0, 1);
        if (fault == 1)
            put(at, 0);
        if (fault == 2)
            put(at + 95, 1);
        if (fault == 3)
            offset = UINT32_MAX;
        if (fault == 4)
            offset = 9;
        if (fault == 5)
            capacity = (span + 1) / 2 - 1;
        if (fault == 6)
            capacity = 4;
        if (fault == 7)
            offset = span - 99;
        memcpy(expected, data, sizeof data);
        assert(!hpclang_rpl_recovery_identity_seal(data, capacity, offset));
        assert(!memcmp(data, expected, sizeof data));
    }
    assert(!hpclang_rpl_recovery_identity_seal(NULL, sizeof data, at));
}

int main(void)
{
    const struct hpclang_rpl_reader reader = {read_nibble, data};
    for (unsigned v = 0; v < sizeof vectors / sizeof vectors[0]; ++v) {
        span = vectors[v].span;
        seal_tests(v);
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
