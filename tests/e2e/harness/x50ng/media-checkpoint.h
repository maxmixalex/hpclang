#ifndef HPCLANG_MEDIA_CHECKPOINT_H
#define HPCLANG_MEDIA_CHECKPOINT_H

#include "s3c2410/s3c2410_sdi.h"

/* SD(1): enter raw card A; SD(2): replace with B; SD(3): return A;
 * SD(4): restore the launch card. Both physical transitions
 * happen before the guest resumes, so a GPIO-presence-only driver misses them.
 * Use the emulator's card API: it changes both block media and the detect pin. */
static int media_checkpoint(hdw_t *hdw, uint32_t address, uint32_t value, unsigned *phase)
{
    if ((value & 0xffff0000u) != 0x53440000u)
        return 0;
    unsigned request = value & 65535u;
    const char *replacement = getenv("HPCLANG_E2E_REPLACEMENT_CARD");
    const char *storage = getenv("HPCLANG_E2E_STORAGE_CARD");
    const char *original = getenv("HPCLANG_X50NG_SD_DIR");
    if (!replacement || !*replacement || !original || !*original || !storage || !*storage ||
        request != *phase + 1 || request > 4 || !s3c2410_sdi_is_mounted(hdw))
        return -1;
    const char *path = request == 2 ? replacement : request == 4 ? original : storage;
    if (s3c2410_sdi_mount(hdw, (char *)path) || !s3c2410_sdi_is_mounted(hdw))
        return -1;
    *phase = request;
    uint32_t ack = 0x53450000u | request;
    uint8_t bytes[4] = {(uint8_t)ack, (uint8_t)(ack >> 8), (uint8_t)(ack >> 16),
                        (uint8_t)(ack >> 24)};
    return cpu_memory_rw_debug(hdw->cpu, address, bytes, sizeof bytes, 1) ? -1 : 1;
}

static int media_checkpoint_complete(unsigned phase)
{
    const char *replacement = getenv("HPCLANG_E2E_REPLACEMENT_CARD");
    return !replacement || !*replacement || phase == 4;
}

#endif
