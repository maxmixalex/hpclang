#ifndef HPCLANG_FRAME_CHECKPOINT_H
#define HPCLANG_FRAME_CHECKPOINT_H

/* Test-only mailbox: FR(id) requests an immutable LCD checkpoint, FK(id)
 * acknowledges durable evidence. No guest drawing or control flow is changed. */
static int frame_checkpoint(hdw_t *hdw, uint32_t address, uint32_t value, int height)
{
    if ((value & 0xffff0000u) != 0x46520000u)
        return 0;
    const char *prefix = getenv("HPCLANG_E2E_FRAME_PREFIX");
    if (!prefix)
        return -1;
    char path[1024];
    if (snprintf(path, sizeof path, "%s-%u.pgm", prefix, value & 65535u) >= (int)sizeof path)
        return -1;
    FILE *file = fopen(path, "wb");
    if (!file)
        return -1;
    /* One extra column contains the physical status segments. */
    fprintf(file, "P5\n132 %d\n255\n", height);
    for (int y = 0; y < height; ++y)
        for (int x = 0; x < 132; ++x) {
            int level = s3c2410_get_pixel_color((s3c2410_lcd_t *)hdw->s3c2410_lcd, x, y);
            if (level < 0 || level > 15) {
                fclose(file);
                return -1;
            }
            fputc(255 - 17 * level, file);
        }
    int error = ferror(file);
    if (fclose(file) || error)
        return -1;
    snprintf(path, sizeof path, "%s-%u.lcd", prefix, value & 65535u);
    file = fopen(path, "w");
    if (!file)
        return -1;
    const unsigned offsets[] = {0, 1, 2, 4, 5, 6, 7, 10, 19};
    for (unsigned i = 0; i < sizeof offsets / sizeof offsets[0]; ++i) {
        uint8_t bytes[4];
        if (cpu_memory_rw_debug(hdw->cpu, 0x07300000u + 4u * offsets[i], bytes, 4, 0)) {
            fclose(file);
            return -1;
        }
        uint32_t reg = (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8) | ((uint32_t)bytes[2] << 16) |
                       ((uint32_t)bytes[3] << 24);
        fprintf(file, "%u %08x\n", offsets[i], reg);
    }
    error = ferror(file);
    if (fclose(file) || error)
        return -1;
    uint32_t ack = 0x464b0000u | (value & 65535u);
    uint8_t bytes[4] = {(uint8_t)ack, (uint8_t)(ack >> 8), (uint8_t)(ack >> 16),
                        (uint8_t)(ack >> 24)};
    if (cpu_memory_rw_debug(hdw->cpu, address, bytes, 4, 1))
        return -1;
    return 1;
}
#endif
