#ifndef HPCLANG_INPUT_CHECKPOINT_H
#define HPCLANG_INPUT_CHECKPOINT_H

struct input_sequence
{
    uint64_t deadline;
    unsigned request;
    unsigned remaining;
};

static int input_switch(hdw_t *hdw, unsigned request, uint64_t *held)
{
    unsigned scan = request & 63u;
    unsigned down = (request >> 8) & 1u;
    uint64_t bit = UINT64_C(1) << scan;
    if (((*held & bit) != 0) == (down != 0))
        return -1;
    if (scan < 56u)
        s3c2410_io_port_g_update(hdw, scan / 7u, scan % 7u + 1u, down);
    else
        s3c2410_io_port_f_set_bit(hdw, scan == 56u ? 0u : scan - 53u, down);
    *held ^= bit;
    return 0;
}

/* Test-only physical input mailbox: KI(scan | down<<8), then KJ(same).
 * Inject the electrical input used by the emulator's UI, never an SDK event
 * or a character. The guest must poll/debounce it before requesting another
 * transition. Keep an independent ledger so duplicate/unbalanced input fails. */
static int input_checkpoint(hdw_t *hdw, uint32_t address, uint32_t value, uint64_t *held,
                            struct input_sequence *sequence)
{
    uint64_t now = timer_get_clock();
    if (sequence->remaining && now >= sequence->deadline) {
        if (input_switch(hdw, sequence->request, held))
            return -1;
        --sequence->remaining;
        sequence->request ^= 0x100u;
        sequence->deadline = now + 100000u;
    }
    unsigned command = value & 0xffff0000u;
    if (command != 0x4b490000u && command != 0x4b500000u)
        return 0;
    unsigned request = value & 65535u;
    unsigned scan = request & 63u;
    unsigned down = (request >> 8) & 1u;
    if ((request & ~0x13fu) || scan >= 60u || sequence->remaining)
        return -1;
    /* KP schedules a press/release pulse, or releases an already-held key.
     * It lets unmodified blocking guest APIs run while real GPIO transitions
     * occur later. KQ acknowledges scheduling, not completion. */
    if (command == 0x4b500000u) {
        if (((*held >> scan) & 1u) == down)
            return -1;
        sequence->request = request;
        sequence->remaining = down ? 2 : 1;
        sequence->deadline = now + 100000u;
    } else if (input_switch(hdw, request, held))
        return -1;
    uint32_t ack = (command == 0x4b500000u ? 0x4b510000u : 0x4b4a0000u) | request;
    uint8_t bytes[4] = {(uint8_t)ack, (uint8_t)(ack >> 8), (uint8_t)(ack >> 16),
                        (uint8_t)(ack >> 24)};
    if (cpu_memory_rw_debug(hdw->cpu, address, bytes, sizeof bytes, 1))
        return -1;
    return 1;
}
#endif
