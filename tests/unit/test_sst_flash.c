#include <assert.h>
#include <stdint.h>
#include <string.h>
#include "sst-flash.h"

#define FIRST UINT32_C(0x140000)
#define SECTOR 4096u
struct model
{
    uint16_t memory[SECTOR]; /* two sectors */
    uint32_t step, count, clock, pending_address, pending_reads;
    uint16_t pending_value;
    int erase, active, stuck, faulty;
    unsigned toggle;
};

static struct model chip;
static const struct hpclang_sst_region region = {FIRST, FIRST + 2 * SECTOR, SECTOR};

static uint16_t read16(void *context, uint32_t address)
{
    struct model *m = context;
    assert(!(address & 1u) && address >= FIRST && address < region.end);
    if (m->active) {
        if (m->stuck || m->pending_reads) {
            if (!m->stuck)
                --m->pending_reads;
            m->toggle ^= 0x40u;
            return (uint16_t)(m->toggle | ((~m->pending_value) & 0x80u));
        }
        unsigned index = (m->pending_address - FIRST) / 2;
        if (m->erase) {
            memset(m->memory + index, 0xff, SECTOR);
            if (m->faulty)
                m->memory[index + SECTOR / 2 - 1] = 0;
        } else if (!m->faulty) {
            m->memory[index] &= m->pending_value;
        }
        m->active = 0;
    }
    return m->memory[(address - FIRST) / 2];
}

static void write16(void *context, uint32_t address, uint16_t value)
{
    struct model *m = context;
    assert(!m->active);
    ++m->count;
    switch (m->step++) {
    case 0:
    case 3:
        if (m->step == 4 && !m->erase)
            goto start;
        assert(address == 0xaaaau && value == 0xaau);
        return;
    case 1:
    case 4:
        assert(address == 0x5554u && value == 0x55u);
        return;
    case 2:
        assert(address == 0xaaaau && (value == 0xa0u || value == 0x80u));
        m->erase = value == 0x80u;
        return;
    case 5:
        assert(m->erase && value == 0x30u && !(address % SECTOR));
        break;
    default:
        assert(0);
    }
start:
    assert(address >= FIRST && address < region.end);
    m->pending_address = address;
    m->pending_value = m->erase ? UINT16_MAX : value;
    m->pending_reads = 8;
    m->active = 1;
    m->step = 0;
}

static uint32_t time_us(void *context)
{
    return ((struct model *)context)->clock++;
}

static const struct hpclang_sst_bus bus = {&chip, read16, write16, time_us};

static void reset(uint8_t fill)
{
    memset(&chip, 0, sizeof chip);
    memset(chip.memory, fill, sizeof chip.memory);
}

int main(void)
{
    reset(0xff);
    chip.clock = UINT32_MAX - 2u;
    assert(hpclang_sst_program_word(&bus, &region, FIRST + 2, 0x1234, 20) == HPCLANG_SST_OK);
    assert(chip.count == 4 && !chip.active);
    assert(chip.memory[0] == 0xffff && chip.memory[1] == 0x1234 && chip.memory[2] == 0xffff);
    assert(hpclang_sst_program_word(&bus, &region, FIRST + 2, 0x1234, 20) == HPCLANG_SST_OK);
    assert(chip.count == 4);
    assert(hpclang_sst_program_word(&bus, &region, FIRST + 2, 0xffff, 20) ==
           HPCLANG_SST_NEEDS_ERASE);
    assert(chip.count == 4 && chip.memory[1] == 0x1234);

    reset(0);
    assert(hpclang_sst_erase_sector(&bus, &region, FIRST + SECTOR, 20) == HPCLANG_SST_OK);
    assert(chip.count == 6 && !chip.active);
    for (unsigned i = 0; i < SECTOR; ++i)
        assert(chip.memory[i] == (i < SECTOR / 2 ? 0 : UINT16_MAX));

    reset(0xff);
    const uint32_t bad[] = {FIRST - 2, FIRST + 1, region.end, UINT32_MAX};
    for (unsigned i = 0; i < sizeof bad / sizeof bad[0]; ++i) {
        assert(hpclang_sst_program_word(&bus, &region, bad[i], 0, 20) == HPCLANG_SST_INVALID);
        assert(hpclang_sst_erase_sector(&bus, &region, bad[i], 20) == HPCLANG_SST_INVALID);
    }
    assert(hpclang_sst_erase_sector(&bus, &region, FIRST + 2, 20) == HPCLANG_SST_INVALID);
    struct hpclang_sst_region short_region = {FIRST, FIRST + SECTOR - 2, SECTOR};
    assert(hpclang_sst_erase_sector(&bus, &short_region, FIRST, 20) == HPCLANG_SST_INVALID);
    assert(hpclang_sst_program_word(&bus, &region, FIRST, 0, 0) == HPCLANG_SST_INVALID);
    assert(hpclang_sst_program_word(&bus, &region, FIRST, 0, UINT32_MAX) == HPCLANG_SST_INVALID);
    assert(chip.count == 0);
    assert(hpclang_sst_program_word(NULL, &region, FIRST, 0, 20) == HPCLANG_SST_INVALID);
    assert(hpclang_sst_erase_sector(&bus, NULL, FIRST, 20) == HPCLANG_SST_INVALID);
    assert(hpclang_sst_program_word(&bus, &region, region.end - 2, 0, 20) == HPCLANG_SST_OK);
    assert(chip.memory[SECTOR - 1] == 0);

    reset(0xff);
    chip.stuck = 1;
    chip.clock = UINT32_MAX - 1u;
    assert(hpclang_sst_program_word(&bus, &region, FIRST, 0, 3) == HPCLANG_SST_BUSY);
    /* No reset/abort writes after timeout: SST may still be programming. */
    assert(chip.count == 4 && chip.active && chip.memory[0] == UINT16_MAX);
    reset(0);
    chip.stuck = 1;
    assert(hpclang_sst_erase_sector(&bus, &region, FIRST, 3) == HPCLANG_SST_BUSY);
    assert(chip.count == 6 && chip.active);

    reset(0xff);
    chip.faulty = 1;
    assert(hpclang_sst_program_word(&bus, &region, FIRST, 0, 20) == HPCLANG_SST_VERIFY);
    assert(!chip.active);
    reset(0);
    chip.faulty = 1;
    assert(hpclang_sst_erase_sector(&bus, &region, FIRST, 20) == HPCLANG_SST_VERIFY);
    assert(!chip.active);
    return 0;
}
