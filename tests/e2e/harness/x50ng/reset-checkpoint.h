#ifndef HPCLANG_RESET_CHECKPOINT_H
#define HPCLANG_RESET_CHECKPOINT_H

/* Observe the owned ARM witness's SWI 0 / r0=1 before Stock can reclaim TEMP.
 * Inert without an explicit output prefix. No guest register/memory is changed.
 * The caller treats capture failure as an E2E failure, never as missing evidence. */
static int hpclang_reset_checkpoint(CPUARMState *env)
{
    const char *prefix = getenv("HPCLANG_X50NG_RESET_CHECKPOINT");
    uint32_t pc = env->regs[15];
    if (!prefix || !*prefix || env->thumb || env->regs[0] != 1 || pc < 0x08000004u ||
        pc > 0x08080000u)
        return 0;
    uint8_t instruction[4];
    if (cpu_memory_rw_debug(env, pc - 4u, instruction, 4, 0))
        return -1;
    if (instruction[0] || instruction[1] || instruction[2] || instruction[3] != 0xef)
        return 0;
    char ram_path[1024], metadata_path[1024];
    if (snprintf(ram_path, sizeof ram_path, "%s.sram", prefix) >= (int)sizeof ram_path ||
        snprintf(metadata_path, sizeof metadata_path, "%s.json", prefix) >=
            (int)sizeof metadata_path)
        return -1;
    const size_t bytes = 0x80000u;
    uint8_t *ram = malloc(bytes);
    if (!ram)
        return -1;
    if (cpu_memory_rw_debug(env, 0x08000000u, ram, bytes, 0)) {
        free(ram);
        return -1;
    }
    FILE *output = fopen(ram_path, "wx");
    if (!output) {
        free(ram);
        return -1;
    }
    int error = fwrite(ram, 1, bytes, output) != bytes;
    free(ram);
    if (fclose(output) || error)
        return -1;
    output = fopen(metadata_path, "wx");
    if (!output)
        return -1;
    error = fprintf(output, "{\"version\":1,\"pc\":%u,\"cpsr\":%u,\"r0\":1,\"bytes\":%u}\n", pc,
                    cpsr_read(env), (unsigned)bytes) < 0;
    return fclose(output) || error ? -1 : 1;
}
#endif
