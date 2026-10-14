#include "sst-flash.h"

/* SST x16 command protocol: word addresses 5555/2AAA become byte offsets.
 * References: SST S72006 application note and S71223 command/status tables.
 * https://ww1.microchip.com/downloads/cn/AppNotes/cn550978.pdf
 * https://ww1.microchip.com/downloads/en/DeviceDoc/S71223_.pdf
 */
static int valid(const struct hpclang_sst_bus *bus, const struct hpclang_sst_region *region,
                 uint32_t address, uint32_t timeout)
{
    return bus && bus->read16 && bus->write16 && bus->time_us && region && timeout &&
           timeout <= INT32_MAX && !(address & 1u) && !(region->first & 1u) &&
           !(region->end & 1u) && region->first < region->end && address >= region->first &&
           address < region->end;
}

static void unlock(const struct hpclang_sst_bus *bus, uint16_t command)
{
    bus->write16(bus->context, 0xaaaau, 0xaau);
    bus->write16(bus->context, 0x5554u, 0x55u);
    bus->write16(bus->context, 0xaaaau, command);
}

static int wait_ready(const struct hpclang_sst_bus *bus, uint32_t address, uint32_t timeout)
{
    uint32_t start = bus->time_us(bus->context);
    for (;;) {
        uint16_t first = bus->read16(bus->context, address);
        uint16_t second = bus->read16(bus->context, address);
        if (!((first ^ second) & 0x40u))
            return HPCLANG_SST_OK;
        if ((uint32_t)(bus->time_us(bus->context) - start) >= timeout)
            return HPCLANG_SST_BUSY;
    }
}

int hpclang_sst_program_word(const struct hpclang_sst_bus *bus,
                             const struct hpclang_sst_region *region, uint32_t address,
                             uint16_t value, uint32_t timeout_us)
{
    if (!valid(bus, region, address, timeout_us))
        return HPCLANG_SST_INVALID;
    uint16_t old = bus->read16(bus->context, address);
    if (old == value)
        return HPCLANG_SST_OK;
    if ((old & value) != value)
        return HPCLANG_SST_NEEDS_ERASE;
    unlock(bus, 0xa0u);
    bus->write16(bus->context, address, value);
    int result = wait_ready(bus, address, timeout_us);
    if (result != HPCLANG_SST_OK)
        return result;
    return bus->read16(bus->context, address) == value ? HPCLANG_SST_OK : HPCLANG_SST_VERIFY;
}

int hpclang_sst_erase_sector(const struct hpclang_sst_bus *bus,
                             const struct hpclang_sst_region *region, uint32_t address,
                             uint32_t timeout_us)
{
    if (!valid(bus, region, address, timeout_us) || region->sector_bytes < 2u ||
        (region->sector_bytes & (region->sector_bytes - 1u)) ||
        (address & (region->sector_bytes - 1u)) || region->sector_bytes > region->end - address)
        return HPCLANG_SST_INVALID;
    unlock(bus, 0x80u);
    bus->write16(bus->context, 0xaaaau, 0xaau);
    bus->write16(bus->context, 0x5554u, 0x55u);
    bus->write16(bus->context, address, 0x30u);
    int result = wait_ready(bus, address, timeout_us);
    if (result != HPCLANG_SST_OK)
        return result;
    for (uint32_t offset = 0; offset < region->sector_bytes; offset += 2u)
        if (bus->read16(bus->context, address + offset) != UINT16_MAX)
            return HPCLANG_SST_VERIFY;
    return HPCLANG_SST_OK;
}
