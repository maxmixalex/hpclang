#define _GNU_SOURCE
#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stddef.h>
#include <sys/mman.h>
#include <hpclang/device.h>
#include <hpclang/time.h>
#include "clock-internal.h"
#include "datetime-internal.h"
#include "irq.h"

void hpclang_time_init(void);
static uint32_t flags = 0x10;
static unsigned bridges;
static int read_failure, write_failure;
static struct hpclang_datetime civil = {2026, 10, 24, 12, 0, 0};

uint32_t hpclang_irq_save_disable(void)
{
    uint32_t old = flags;
    flags |= 0xc0;
    ++bridges;
    return old;
}
void hpclang_irq_restore(uint32_t old)
{
    assert((flags & 0xc0) == 0xc0);
    flags = old;
}
int hpclang_rtc_read(struct hpclang_datetime *value)
{
    assert((flags & 0xc0) == 0xc0);
    *value = civil;
    return !read_failure;
}
int hpclang_rtc_write(const struct hpclang_datetime *value)
{
    assert((flags & 0xc0) == 0xc0);
    if (write_failure) {
        errno = EBUSY;
        return -1;
    }
    civil = *value;
    return 0;
}
const struct hpclang_device_info *hpclang_device_info(void)
{
    static const struct hpclang_device_info device = {.family = HPCLANG_FAMILY_HP4950};
    return &device;
}
#define REG32(address) (*(volatile uint32_t *)(uintptr_t)(address))
int main(void)
{
    const uintptr_t pages[] = {0x07200000, 0x07600000};
    for (unsigned i = 0; i < 2; ++i) {
        void *p = mmap((void *)pages[i], 4096, PROT_READ | PROT_WRITE,
                       MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        assert(p == (void *)pages[i]);
    }
    REG32(0x07200010) = 0x10;
    REG32(0x07600000) = 255;
    REG32(0x07600004) = 3;
    REG32(0x07600008) = 9;
    REG32(0x0760000c) = 65535;
    REG32(0x07600014) = 60000;
    hpclang_time_init();
    assert(flags == 0x10);
    uint64_t first, next;
    assert(!hpclang_time_read_us(&first));
    REG32(0x07600014) = 59999;
    assert(!hpclang_time_read_us(&next) && next > first && flags == 0x10);
    uint32_t old = hpclang_irq_save_disable();
    unsigned count = bridges;
    REG32(0x07600014) = 59998;
    assert(!hpclang_time_read_us_masked(&first) && first > next);
    assert(bridges == count && (flags & 0xc0) == 0xc0);
    REG32(0x07600014) = 59997;
    first = hpclang_time_monotonic_us_masked();
    assert(first > next && bridges == count && (flags & 0xc0) == 0xc0);
    read_failure = 1;
    errno = EBUSY;
    assert(hpclang_time_monotonic_us_masked() == first);
    assert(bridges == count && errno == EBUSY && (flags & 0xc0) == 0xc0);
    hpclang_irq_restore(old);

    read_failure = 1;
    next = 123;
    assert(hpclang_time_read_us(&next) == -1 && errno == EIO && next == 123);
    assert(hpclang_time_monotonic_us() == first && flags == 0x10);
    read_failure = 0;
    struct hpclang_datetime changed = civil;
    changed.year = 2025;
    assert(!hpclang_datetime_set(&changed) && flags == 0x10);
    assert(!hpclang_time_read_us(&next) && next >= first);
    write_failure = 1;
    assert(hpclang_datetime_set(&civil) == -1 && errno == EBUSY && flags == 0x10);
    assert(hpclang_datetime_set(NULL) == -1 && errno == EINVAL && flags == 0x10);
    old = hpclang_irq_save_disable();
    assert(!hpclang_time_read_us(&next) && (flags & 0xc0) == 0xc0);
    struct hpclang_clock_epoch epoch;
    assert(!hpclang_time_clock_prepare(&epoch));
    hpclang_time_clock_commit(&epoch);
    assert(!hpclang_time_read_us_masked(&next) && next >= first);
    hpclang_irq_restore(old);
    assert(flags == 0x10);
    for (unsigned i = 0; i < 2; ++i)
        assert(!munmap((void *)pages[i], 4096));
    return 0;
}
