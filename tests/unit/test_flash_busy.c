#include "flash-busy.h"
#include <assert.h>

int main(void)
{
    uint8_t memory[8192], original[8192];
    struct hpclang_flash_busy busy = {0};
    memset(memory, 255, sizeof(memory));
    memcpy(original, memory, sizeof(memory));
    assert(hpclang_flash_busy_start(&busy, sizeof(memory), 6, 2, 0x1234, 8));
    assert(!hpclang_flash_busy_start(&busy, sizeof(memory), 8, 2, 0, 8));
    uint16_t prior = 0;
    for (unsigned i = 0; i < 8; ++i) {
        uint16_t status = hpclang_flash_busy_read(&busy, memory, 6);
        assert((status ^ prior) & 0x40u);
        assert((status & 0x80u) == ((~0x1234u) & 0x80u));
        assert(!memcmp(memory, original, sizeof(memory)));
        prior = status;
    }
    assert(hpclang_flash_busy_read(&busy, memory, 6) == 0x1234 && !busy.bytes);
    original[6] = 0x34;
    original[7] = 0x12;
    assert(!memcmp(memory, original, sizeof(memory)));
    assert(hpclang_flash_busy_start(&busy, sizeof(memory), 6, 2, 0xFFFF, 1));
    (void)hpclang_flash_busy_read(&busy, memory, 6);
    assert(hpclang_flash_busy_read(&busy, memory, 6) == 0x1234); /* no 0-to-1 program */

    memset(memory, 0, sizeof(memory));
    assert(hpclang_flash_busy_start(&busy, sizeof(memory), 4096, 4096, 0xFFFF, 3));
    for (unsigned i = 0; i < 3; ++i)
        assert(!(hpclang_flash_busy_read(&busy, memory, 4096) & 0x80u));
    assert(memory[4096] == 0 && memory[8191] == 0);
    assert(hpclang_flash_busy_read(&busy, memory, 4096) == 0xFFFF);
    for (unsigned i = 0; i < sizeof(memory); ++i)
        assert(memory[i] == (i < 4096 ? 0 : 255));

    assert(hpclang_flash_busy_start(&busy, sizeof(memory), 0, 4096, 0xFFFF, 2));
    (void)hpclang_flash_busy_read(&busy, memory, 0);
    hpclang_flash_busy_cancel(&busy);
    assert(hpclang_flash_busy_read(&busy, memory, 0) == 0);
    assert(!hpclang_flash_busy_start(&busy, sizeof(memory), 0, 2, 0, 0));
    assert(!hpclang_flash_busy_start(&busy, sizeof(memory), 1, 2, 0, 1));
    assert(!hpclang_flash_busy_start(&busy, sizeof(memory), sizeof(memory), 2, 0, 1));
    assert(!hpclang_flash_busy_start(&busy, sizeof(memory), 0, UINT32_MAX, 0, 1));
    assert(!hpclang_flash_busy_start(&busy, sizeof(memory), 8190, 4, 0, 1));

    /* All masks for a program pulse, with adjacent bytes as independent guards.
     * Bits outside the requested 1-to-0 transitions must never change. */
    for (unsigned mask = 0; mask <= UINT16_MAX; ++mask) {
        uint8_t word[] = {0x5a, 0xa5, 0xed, 0xb7, 0x69, 0x96};
        assert(hpclang_flash_busy_start(&busy, sizeof(word), 2, 2, 0x1234, 8));
        (void)hpclang_flash_busy_read(&busy, word, 2);
        assert(hpclang_flash_busy_power_loss(&busy, word, (uint16_t)mask));
        uint16_t actual = (uint16_t)(word[2] | (uint16_t)word[3] << 8u);
        assert(actual == (uint16_t)(0xb7ed & ~(mask & ~0x1234u)));
        assert(word[0] == 0x5a && word[1] == 0xa5 && word[4] == 0x69 && word[5] == 0x96);
        assert(!busy.bytes && !busy.reads);
        assert(!hpclang_flash_busy_power_loss(&busy, word, UINT16_MAX));
    }
    for (unsigned bit = 0; bit < 16; ++bit) {
        memset(memory, 0, sizeof memory);
        uint16_t mask = (uint16_t)(1u << bit);
        assert(hpclang_flash_busy_start(&busy, sizeof memory, 4096, 4096, UINT16_MAX, 8));
        assert(hpclang_flash_busy_power_loss(&busy, memory, mask));
        for (unsigned i = 0; i < sizeof memory; i += 2) {
            uint16_t word = (uint16_t)(memory[i] | (uint16_t)memory[i + 1] << 8u);
            assert(word == (i < 4096 ? 0 : mask));
        }
    }
    return 0;
}
