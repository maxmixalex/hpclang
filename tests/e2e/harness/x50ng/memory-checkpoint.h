#ifndef HPCLANG_MEMORY_CHECKPOINT_H
#define HPCLANG_MEMORY_CHECKPOINT_H

/* Test-only request 0x4d1ooooo points at a word-aligned descriptor in the
 * application's RW arena: {id, count, words[count]}. ooooo is the byte offset
 * from the boot page. Acknowledgement 0x4d2ooooo follows successful JSON close.
 * Bounded integer arrays let suites define their own versioned measurements. */
static int memory_checkpoint(hdw_t *hdw, uint32_t mailbox, uint32_t request)
{
    if ((request & 0xfff00000u) != 0x4d100000u)
        return 0;
    uint32_t offset = request & 0xfffffu;
    uint8_t bytes[264], size[4];
    if ((offset & 3u) || offset < 4096u || cpu_memory_rw_debug(hdw->cpu, 0x01100004u, size, 4, 0))
        return -1;
    uint32_t extent = (uint32_t)size[0] | ((uint32_t)size[1] << 8) | ((uint32_t)size[2] << 16) |
                      ((uint32_t)size[3] << 24);
    if (extent > 0x100000u || extent < 8u || offset > extent - 8u ||
        cpu_memory_rw_debug(hdw->cpu, 0x01100000u + offset, bytes, 8, 0))
        return -1;
    uint32_t id = (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8) | ((uint32_t)bytes[2] << 16) |
                  ((uint32_t)bytes[3] << 24);
    uint32_t count = (uint32_t)bytes[4] | ((uint32_t)bytes[5] << 8) | ((uint32_t)bytes[6] << 16) |
                     ((uint32_t)bytes[7] << 24);
    if (id > 65535u || !count || count > 64u || count * 4u > extent - offset - 8u ||
        cpu_memory_rw_debug(hdw->cpu, 0x01100000u + offset + 8u, bytes, count * 4u, 0))
        return -1;
    const char *prefix = getenv("HPCLANG_E2E_FRAME_PREFIX");
    char path[1024];
    if (!prefix || snprintf(path, sizeof path, "%s-memory-%u.json", prefix, id) >= (int)sizeof path)
        return -1;
    FILE *file = fopen(path, "wx");
    if (!file)
        return -1;
    fprintf(file, "{\"version\":1,\"id\":%u,\"words\":[", id);
    for (uint32_t i = 0; i < count; ++i) {
        const uint8_t *p = bytes + i * 4u;
        uint32_t value = (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
                         ((uint32_t)p[3] << 24);
        fprintf(file, "%s%u", i ? "," : "", value);
    }
    fputs("]}\n", file);
    int error = ferror(file);
    if (fclose(file) || error)
        return -1;
    uint32_t ack = 0x4d200000u | offset;
    for (unsigned i = 0; i < 4; ++i)
        bytes[i] = (uint8_t)(ack >> (8 * i));
    return cpu_memory_rw_debug(hdw->cpu, mailbox, bytes, 4, 1) ? -1 : 1;
}
#endif
