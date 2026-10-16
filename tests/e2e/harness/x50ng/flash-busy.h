#ifndef HPCLANG_E2E_FLASH_BUSY_H
#define HPCLANG_E2E_FLASH_BUSY_H

#include <stdint.h>
#include <string.h>

/* Deterministic status-read latency, not a model of physical pulse duration.
 * DQ6 toggles and DQ7 complements programmed data (zero during erase), per
 * SST S71223, Write Operation Status Detection. Array changes are delayed until
 * completion. A power-loss fault selects completed bit transitions explicitly;
 * its mask describes a test outcome, not a prediction of physical cell behavior.
 * https://ww1.microchip.com/downloads/en/DeviceDoc/S71223_.pdf
 */
struct hpclang_flash_busy
{
    uint32_t address, bytes, reads;
    uint16_t value, toggle;
};

static inline int hpclang_flash_busy_start(struct hpclang_flash_busy *busy, uint32_t capacity,
                                           uint32_t address, uint32_t bytes, uint16_t value,
                                           uint32_t reads)
{
    if (busy->bytes || !reads || !bytes || ((address | bytes) & 1u) || address >= capacity ||
        bytes > capacity - address)
        return 0;
    *busy = (struct hpclang_flash_busy){address, bytes, reads, value, 0};
    return 1;
}

static inline uint16_t hpclang_flash_busy_read(struct hpclang_flash_busy *busy, uint8_t *array,
                                               uint32_t address)
{
    if (busy->bytes) {
        if (busy->reads) {
            --busy->reads;
            busy->toggle ^= 0x40u;
            return (uint16_t)(busy->toggle | ((~busy->value) & 0x80u));
        }
        if (busy->bytes == 2u) {
            array[busy->address] &= (uint8_t)busy->value;
            array[busy->address + 1u] &= (uint8_t)(busy->value >> 8u);
        } else {
            memset(array + busy->address, 255, busy->bytes);
        }
        busy->bytes = 0;
    }
    return (uint16_t)(array[address] | (uint16_t)array[address + 1u] << 8u);
}

static inline void hpclang_flash_busy_cancel(struct hpclang_flash_busy *busy)
{
    *busy = (struct hpclang_flash_busy){0};
}

/* Apply one selected torn-operation outcome before discarding controller state.
 * The same bit mask is repeated for each erase word. This deliberately does not
 * claim to enumerate all possible spatial patterns or real silicon outcomes. */
static inline int hpclang_flash_busy_power_loss(struct hpclang_flash_busy *busy, uint8_t *array,
                                                uint16_t completed_bits)
{
    if (!busy->bytes)
        return 0;
    for (uint32_t offset = 0; offset < busy->bytes; offset += 2u) {
        uint32_t at = busy->address + offset;
        if (busy->bytes == 2u) {
            uint16_t partial = (uint16_t)(busy->value | (uint16_t)~completed_bits);
            array[at] &= (uint8_t)partial;
            array[at + 1u] &= (uint8_t)(partial >> 8u);
        } else {
            array[at] |= (uint8_t)completed_bits;
            array[at + 1u] |= (uint8_t)(completed_bits >> 8u);
        }
    }
    hpclang_flash_busy_cancel(busy);
    return 1;
}

#endif
