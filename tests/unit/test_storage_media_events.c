#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stddef.h>
#include "storage-media-events.h"
#include "storage-mmio.h"
#include "irq.h"

static uint32_t pin, trigger, mask, route, source, pending;
static unsigned writes, locked;

uint32_t hpclang_irq_save_disable(void)
{
    assert(!locked);
    locked = 1;
    return 0x123;
}
void hpclang_irq_restore(uint32_t state)
{
    assert(locked && state == 0x123);
    locked = 0;
}
uint32_t hpclang_storage_mmio_read32(uintptr_t address)
{
    assert(locked);
    switch (address) {
    case 0x07100000:
        return source;
    case 0x07100004:
        return route;
    case 0x07100008:
        return mask;
    case 0x07100010:
        return pending;
    case 0x07a00050:
        return pin;
    case 0x07a00088:
        return trigger;
    default:
        assert(0);
        return 0;
    }
}
void hpclang_storage_mmio_write32(uintptr_t address, uint32_t value)
{
    assert(locked);
    ++writes;
    switch (address) {
    case 0x07100000:
        assert(value == 8);
        source &= ~value;
        break;
    case 0x07100004:
        route = value;
        break;
    case 0x07100008:
        mask = value;
        break;
    case 0x07100010:
        assert(value == 8);
        pending &= ~value;
        break;
    case 0x07a00050:
        pin = value;
        break;
    case 0x07a00088:
        trigger = value;
        break;
    default:
        assert(0);
    }
}

int main(void)
{
    struct hpclang_storage_media_events events = {0};
    assert(hpclang_storage_media_events_acquire(NULL) == -1 && errno == EINVAL);
    assert(hpclang_storage_media_events_changed(&events) == -1 && errno == EPERM);
    for (unsigned mode = 0; mode < 4; ++mode) {
        pin = 0x5a00 | (mode << 6);
        trigger = 0x76543210;
        mask = 0x81204000;
        route = 0x1008;
        source = pending = 0x10008;
        writes = 0;
        if (mode == 1 || mode == 3) {
            assert(hpclang_storage_media_events_acquire(&events) == -1 && errno == ENOTSUP);
            assert(!writes && !events.owned && source == 0x10008);
            continue;
        }
        assert(hpclang_storage_media_events_acquire(&events) == 0 && events.owned);
        assert(pin == 0x5a80 && trigger == 0x76542210);
        assert(mask == 0x81204008 && route == 0x1000);
        assert(source == 0x10000 && pending == 0x10000);
        unsigned before = writes;
        assert(hpclang_storage_media_events_acquire(&events) == -1 && errno == EBUSY);
        assert(writes == before);
        assert(hpclang_storage_media_events_changed(&events) == 0);
        source |= 8; /* Removal edge; GPIO is already high again at the next poll. */
        assert(hpclang_storage_media_events_changed(&events) == 1);
        assert(source == 0x10000 && pending == 0x10000);
        assert(hpclang_storage_media_events_changed(&events) == 0);
        /* Restore only leased fields, retaining changes to unrelated hardware. */
        pin ^= 1;
        trigger ^= 1;
        mask ^= 1;
        route ^= 1;
        hpclang_storage_media_events_release(&events);
        assert(!events.owned && pin == (0x5a01 | (mode << 6)));
        assert(trigger == 0x76543211 && mask == 0x81204001 && route == 0x1009);
        assert(source == 0x10000 && pending == 0x10000);
        before = writes;
        hpclang_storage_media_events_release(&events);
        assert(writes == before && !locked);
    }
    return 0;
}
