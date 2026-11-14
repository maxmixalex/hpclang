#ifndef HPCLANG_NATIVE_HEAP_OBSERVER_H
#define HPCLANG_NATIVE_HEAP_OBSERVER_H

#include <stdint.h>
#include <string.h>

/* Host-only observer core. Call at a fingerprint-verified allocator return,
 * after its list updates, not during individual metadata writes. Integration
 * must delimit the observed lifecycle and account for missed/rejected events.
 * This measures native free capacity, not total calculator memory ownership. */
struct native_heap_capacity
{
    uint32_t usable_bytes, largest_usable_bytes, metadata_bytes, blocks;
};

struct native_heap_observation
{
    uint64_t accepted, rejected, minimum_sample;
    uint32_t minimum_total_usable;
    struct native_heap_capacity initial[2], latest[2], at_minimum[2];
};

typedef int (*native_heap_read32_t)(void *, uint32_t, uint32_t *);

static int native_heap_word(native_heap_read32_t read32, void *context, uint32_t address,
                            uint32_t begin, uint32_t end, uint32_t *value)
{
    if ((address & 3u) || address < begin || address > end - 4u)
        return -1;
    return read32(context, address, value);
}

static int native_heap_capacity_read(native_heap_read32_t read32, void *context, uint32_t begin,
                                     uint32_t end, uint32_t roots,
                                     struct native_heap_capacity output[2])
{
    if (!read32 || !output || (begin & 3u) || (end & 3u) || begin >= end || end - begin < 32u ||
        end - begin > 0x80000u || roots < begin || roots > end - 32u || (roots & 3u))
        return -1;
    /* One bit per SRAM word proves non-overlap, including between the heaps.
     * The fixed 16 KiB workspace is host memory, never guest application RAM. */
    uint32_t occupied[4096] = {0};
    for (uint32_t address = roots; address < roots + 32u; address += 4u) {
        uint32_t index = (address - begin) / 4u;
        occupied[index / 32u] |= UINT32_C(1) << (index % 32u);
    }
    struct native_heap_capacity value[2] = {{0}, {0}};
    for (unsigned heap = 0; heap < 2; ++heap) {
        uint32_t head, previous = 0;
        if (native_heap_word(read32, context, roots + heap * 16u, begin, end, &head))
            return -1;
        while (head) {
            if (head < begin || head > end - 20u || (head & 3u))
                return -1;
            uint32_t size, flags, next, back, footer;
            if (native_heap_word(read32, context, head, begin, end, &size) || (size & 3u) ||
                size > end - head - 20u ||
                native_heap_word(read32, context, head + 4u, begin, end, &flags) ||
                native_heap_word(read32, context, head + 8u, begin, end, &next) ||
                native_heap_word(read32, context, head + 12u, begin, end, &back) ||
                native_heap_word(read32, context, head + 16u + size, begin, end, &footer) ||
                (flags & 0xf000u) || (flags & 0x300u) != (heap + 1u) * 0x100u || back != previous ||
                footer != size)
                return -1;
            for (uint32_t address = head; address < head + size + 20u; address += 4u) {
                uint32_t index = (address - begin) / 4u;
                uint32_t bit = UINT32_C(1) << (index % 32u);
                if (occupied[index / 32u] & bit)
                    return -1;
                occupied[index / 32u] |= bit;
            }
            value[heap].usable_bytes += size;
            value[heap].metadata_bytes += 20u;
            ++value[heap].blocks;
            if (size > value[heap].largest_usable_bytes)
                value[heap].largest_usable_bytes = size;
            previous = head;
            head = next;
        }
    }
    memcpy(output, value, sizeof value);
    return 0;
}

static int native_heap_observe(struct native_heap_observation *state, native_heap_read32_t read32,
                               void *context, uint32_t begin, uint32_t end, uint32_t roots)
{
    if (!state || state->accepted == UINT64_MAX || state->rejected == UINT64_MAX)
        return -1;
    struct native_heap_capacity value[2];
    if (native_heap_capacity_read(read32, context, begin, end, roots, value)) {
        ++state->rejected;
        return -1;
    }
    uint32_t total = value[0].usable_bytes + value[1].usable_bytes;
    if (!state->accepted)
        memcpy(state->initial, value, sizeof value);
    /* Preserve both heaps from the SAME event. Summing independent per-heap
     * minima would invent a low-water state that may never have existed. */
    if (!state->accepted || total < state->minimum_total_usable) {
        state->minimum_total_usable = total;
        state->minimum_sample = state->accepted + 1u;
        memcpy(state->at_minimum, value, sizeof value);
    }
    memcpy(state->latest, value, sizeof value);
    ++state->accepted;
    return 0;
}

#endif
