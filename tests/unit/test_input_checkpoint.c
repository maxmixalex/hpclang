#include <assert.h>
#include <stdint.h>
#include <string.h>

typedef struct
{
    void *cpu;
} hdw_t;
static unsigned port, column, row, pressed, writes, edges;
static uint32_t acknowledgement;
static int write_error;
static uint64_t clock_us;
static uint64_t timer_get_clock(void)
{
    return clock_us;
}
static void s3c2410_io_port_g_update(hdw_t *hdw, unsigned col, unsigned r, unsigned down)
{
    (void)hdw;
    port = 1;
    column = col;
    row = r;
    pressed = down;
    ++edges;
}
static void s3c2410_io_port_f_set_bit(hdw_t *hdw, unsigned bit, unsigned down)
{
    (void)hdw;
    port = 0;
    column = bit;
    pressed = down;
    ++edges;
}
static int cpu_memory_rw_debug(void *cpu, uint32_t address, uint8_t *bytes, unsigned size,
                               int write)
{
    assert(cpu && address == 0x1234 && size == 4 && write == 1);
    acknowledgement =
        bytes[0] | (uint32_t)bytes[1] << 8 | (uint32_t)bytes[2] << 16 | (uint32_t)bytes[3] << 24;
    ++writes;
    return write_error;
}
#include "input-checkpoint.h"

int main(void)
{
    hdw_t hdw = {.cpu = &hdw};
    uint64_t held = 0;
    struct input_sequence sequence = {0};
    for (unsigned scan = 0; scan < 60; ++scan) {
        assert(input_checkpoint(&hdw, 0x1234, 0x4b490100u | scan, &held, &sequence) == 1);
        assert(held == UINT64_C(1) << scan && pressed == 1);
        assert(acknowledgement == (0x4b4a0100u | scan));
        if (scan < 56)
            assert(port == 1 && column == scan / 7 && row == scan % 7 + 1);
        else {
            const unsigned dedicated[] = {0, 4, 5, 6};
            assert(port == 0 && column == dedicated[scan - 56]);
        }
        unsigned before = writes;
        assert(input_checkpoint(&hdw, 0x1234, 0x4b490100u | scan, &held, &sequence) == -1);
        assert(writes == before && edges == writes);
        assert(input_checkpoint(&hdw, 0x1234, 0x4b490000u | scan, &held, &sequence) == 1);
        assert(held == 0 && pressed == 0 && acknowledgement == (0x4b4a0000u | scan));
        before = writes;
        assert(input_checkpoint(&hdw, 0x1234, 0x4b490000u | scan, &held, &sequence) == -1);
        assert(writes == before && edges == writes);
    }
    unsigned before = writes;
    assert(input_checkpoint(&hdw, 0x1234, 0x50415353u, &held, &sequence) == 0);
    assert(input_checkpoint(&hdw, 0x1234, 0x4b49013cu, &held, &sequence) == -1);
    assert(input_checkpoint(&hdw, 0x1234, 0x4b490140u, &held, &sequence) == -1);
    assert(writes == before && edges == writes && held == 0);
    /* Distinct held keys coexist, including a dedicated modifier. */
    assert(input_checkpoint(&hdw, 0x1234, 0x4b490100u | 58, &held, &sequence) == 1);
    assert(input_checkpoint(&hdw, 0x1234, 0x4b490100u | 19, &held, &sequence) == 1);
    assert(held == ((UINT64_C(1) << 58) | (UINT64_C(1) << 19)));
    write_error = 1;
    assert(input_checkpoint(&hdw, 0x1234, 0x4b490000u | 19, &held, &sequence) == -1);
    write_error = 0;
    assert(input_checkpoint(&hdw, 0x1234, 0x4b490000u | 58, &held, &sequence) == 1);
    before = edges;
    assert(input_checkpoint(&hdw, 0x1234, 0x4b500123u, &held, &sequence) == 1);
    assert(acknowledgement == 0x4b510123u && !held && edges == before);
    assert(input_checkpoint(&hdw, 0x1234, 0x4b490124u, &held, &sequence) == -1);
    clock_us = 99999;
    assert(input_checkpoint(&hdw, 0x1234, acknowledgement, &held, &sequence) == 0);
    assert(!held && sequence.remaining == 2);
    clock_us = 100000;
    assert(input_checkpoint(&hdw, 0x1234, acknowledgement, &held, &sequence) == 0);
    assert(held == (UINT64_C(1) << 35) && sequence.remaining == 1 && edges == before + 1);
    clock_us = 200000;
    assert(input_checkpoint(&hdw, 0x1234, acknowledgement, &held, &sequence) == 0);
    assert(!held && !sequence.remaining && edges == before + 2);
    assert(input_checkpoint(&hdw, 0x1234, 0x4b500023u, &held, &sequence) == -1);
    assert(input_checkpoint(&hdw, 0x1234, 0x4b490123u, &held, &sequence) == 1);
    assert(input_checkpoint(&hdw, 0x1234, 0x4b500023u, &held, &sequence) == 1);
    clock_us = 300000;
    assert(input_checkpoint(&hdw, 0x1234, acknowledgement, &held, &sequence) == 0);
    assert(!held && !sequence.remaining);
    return 0;
}
