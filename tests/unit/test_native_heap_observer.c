#include <assert.h>
#include <stdint.h>
#include <string.h>
#include "native-heap-observer.h"

#define BASE UINT32_C(0x08000000)
static uint32_t ram[256];
static uint32_t fail_address;

static int read32(void *context, uint32_t address, uint32_t *value)
{
    assert(context == ram);
    assert(address >= BASE && address <= BASE + sizeof ram - 4 && !(address & 3));
    if (address == fail_address)
        return -1;
    *value = ram[(address - BASE) / 4u];
    return 0;
}

static void block(unsigned offset, unsigned size, unsigned heap)
{
    ram[heap * 4u] = BASE + offset;
    ram[offset / 4u] = size;
    ram[offset / 4u + 1u] = (heap + 1u) * 0x100u;
    ram[(offset + 16u + size) / 4u] = size;
}

static void sample(unsigned first, unsigned second)
{
    memset(ram, 0, sizeof ram);
    block(64, first, 0);
    block(512, second, 1);
}

int main(void)
{
    struct native_heap_observation observation = {0};
    sample(200, 200);
    assert(native_heap_observe(&observation, read32, ram, BASE, BASE + sizeof ram, BASE) == 0);
    sample(20, 100);
    assert(native_heap_observe(&observation, read32, ram, BASE, BASE + sizeof ram, BASE) == 0);
    sample(100, 20);
    assert(native_heap_observe(&observation, read32, ram, BASE, BASE + sizeof ram, BASE) == 0);
    sample(200, 200);
    assert(native_heap_observe(&observation, read32, ram, BASE, BASE + sizeof ram, BASE) == 0);
    assert(observation.accepted == 4 && observation.rejected == 0);
    assert(observation.minimum_total_usable == 120 && observation.minimum_sample == 2);
    assert(observation.at_minimum[0].usable_bytes == 20);
    assert(observation.at_minimum[1].usable_bytes == 100);
    assert(observation.initial[0].usable_bytes == observation.latest[0].usable_bytes);
    assert(observation.latest[0].blocks == 1 && observation.latest[0].metadata_bytes == 20);
    assert(observation.latest[0].largest_usable_bytes == 200);

    struct native_heap_observation saved = observation;
    const unsigned bad_offsets[] = {0, 64, 68, 72, 76, 280, 16};
    const uint32_t bad_values[] = {BASE + 65, UINT32_MAX - 3, 0x2100, BASE + 64, BASE + 512,
                                   0,         BASE + 64};
    for (unsigned i = 0; i < sizeof bad_offsets / sizeof *bad_offsets; ++i) {
        sample(200, 200);
        ram[bad_offsets[i] / 4u] = bad_values[i];
        observation = saved;
        assert(native_heap_observe(&observation, read32, ram, BASE, BASE + sizeof ram, BASE) == -1);
        assert(observation.accepted == saved.accepted && observation.rejected == 1);
        assert(!memcmp(observation.latest, saved.latest, sizeof saved.latest));
        assert(observation.minimum_total_usable == saved.minimum_total_usable);
    }
    sample(200, 200);
    block(128, 20, 1); /* A plausible second-heap header inside the first span. */
    assert(native_heap_observe(&observation, read32, ram, BASE, BASE + sizeof ram, BASE) == -1);
    sample(200, 200);
    fail_address = BASE + 68;
    assert(native_heap_observe(&observation, read32, ram, BASE, BASE + sizeof ram, BASE) == -1);
    fail_address = 0;
    struct native_heap_capacity output[2], unchanged[2];
    memset(output, 0x55, sizeof output);
    memcpy(unchanged, output, sizeof output);
    assert(native_heap_capacity_read(read32, ram, BASE, BASE + sizeof ram, BASE + 1020, output) ==
           -1);
    assert(!memcmp(output, unchanged, sizeof output));
    memset(ram, 0, sizeof ram);
    assert(native_heap_observe(&observation, read32, ram, BASE, BASE + sizeof ram, BASE) == 0);
    assert(observation.minimum_total_usable == 0);
    return 0;
}
