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

static void mode_delay(const struct hpclang_sst_bus *bus)
{
    /* Two ticks guarantee a full microsecond even at a timer tick boundary;
     * S71223 requires 150 ns for software ID access and exit. */
    uint32_t start = bus->time_us(bus->context);
    while ((uint32_t)(bus->time_us(bus->context) - start) < 2u) {
    }
}

static uint16_t query(const struct hpclang_sst_bus *bus, uint32_t word)
{
    return bus->read16(bus->context, word * 2u);
}

int hpclang_sst_identify(const struct hpclang_sst_bus *bus, struct hpclang_sst_device *device)
{
    if (!device)
        return HPCLANG_SST_INVALID;
    *device = (struct hpclang_sst_device){0};
    if (!bus || !bus->read16 || !bus->write16 || !bus->time_us)
        return HPCLANG_SST_INVALID;
    unlock(bus, 0x90u);
    mode_delay(bus);
    uint16_t manufacturer = query(bus, 0);
    uint16_t product = query(bus, 1);
    bus->write16(bus->context, 0, 0xf0u);
    mode_delay(bus);
    uint32_t exponent;
    if (manufacturer != 0xbfu)
        return HPCLANG_SST_UNSUPPORTED;
    switch (product) {
    case 0x234au:
    case 0x234bu:
        exponent = 21;
        break;
    case 0x235au:
    case 0x235bu:
        exponent = 22;
        break;
    default:
        return HPCLANG_SST_UNSUPPORTED;
    }
    unlock(bus, 0x98u);
    mode_delay(bus);
    /* SST S71223 tables 7, 9 and 10: these are two alternative erase
     * granularities over the WHOLE chip, not two consecutive regions. */
    int matches = query(bus, 0x10) == 'Q' && query(bus, 0x11) == 'R' && query(bus, 0x12) == 'Y' &&
                  query(bus, 0x13) == 1 && query(bus, 0x14) == 7 && query(bus, 0x27) == exponent &&
                  query(bus, 0x28) == 1 && query(bus, 0x29) == 0 && query(bus, 0x2c) == 2 &&
                  query(bus, 0x2d) == 0xff && query(bus, 0x2e) == ((1u << (exponent - 20)) - 1u) &&
                  query(bus, 0x2f) == 0x10 && query(bus, 0x30) == 0 &&
                  query(bus, 0x31) == ((1u << (exponent - 16)) - 1u) && query(bus, 0x32) == 0 &&
                  query(bus, 0x33) == 0 && query(bus, 0x34) == 1;
    bus->write16(bus->context, 0, 0xf0u);
    mode_delay(bus);
    if (!matches)
        return HPCLANG_SST_UNSUPPORTED;
    *device = (struct hpclang_sst_device){1u << exponent, 4096, 65536, manufacturer, product};
    return HPCLANG_SST_OK;
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
