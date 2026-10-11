#ifndef HPCLANG_INPUT_CHECKPOINT_H
#define HPCLANG_INPUT_CHECKPOINT_H

/* Test-only physical input mailbox: KI(scan | down<<8), then KJ(same).
 * Inject the electrical input used by the emulator's UI, never an SDK event
 * or a character. The guest must poll/debounce it before requesting another
 * transition. Keep an independent ledger so duplicate/unbalanced input fails. */
static int input_checkpoint(hdw_t *hdw, uint32_t address, uint32_t value, uint64_t *held)
{
    if ((value & 0xffff0000u) != 0x4b490000u)
        return 0;
    unsigned request = value & 65535u;
    unsigned scan = request & 63u;
    unsigned down = (request >> 8) & 1u;
    if ((request & ~0x13fu) || scan >= 60u)
        return -1;
    uint64_t bit = UINT64_C(1) << scan;
    if (((*held & bit) != 0) == (down != 0))
        return -1;
    if (scan < 56u)
        s3c2410_io_port_g_update(hdw, scan / 7u, scan % 7u + 1u, down);
    else
        s3c2410_io_port_f_set_bit(hdw, scan == 56u ? 0u : scan - 53u, down);
    *held ^= bit;
    uint32_t ack = 0x4b4a0000u | request;
    uint8_t bytes[4] = {(uint8_t)ack, (uint8_t)(ack >> 8), (uint8_t)(ack >> 16),
                        (uint8_t)(ack >> 24)};
    if (cpu_memory_rw_debug(hdw->cpu, address, bytes, sizeof bytes, 1))
        return -1;
    return 1;
}
#endif
