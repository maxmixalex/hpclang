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
    return 0;
}
