#include "stage-stack.h"
#include <stddef.h>
#define RPL_MAIN_FIRST 0x80000u
#define RPL_MAIN_END 0x100000u
#define RPL_DSKTOP 0x806f8u
#define RPL_DOCSTR 0x2a2cu
static int read_word(const struct hpclang_stage_stack_io *io, uint32_t at, uint32_t *value)
{
    return io->read(io->context, at, 5, value);
}
static int read_byte(const struct hpclang_stage_stack_io *io, uint32_t at, uint8_t *value)
{
    uint32_t word;
    if (!io->read(io->context, at, 2, &word) || word > 255u)
        return 0;
    *value = (uint8_t)word;
    return 1;
}
int hpclang_stage_stack_hide(const struct hpclang_stage_stack_io *io, uintptr_t stage0_code,
                             struct stage_launcher_bias *bias)
{
    static const uint8_t launcher_magic[] = {0x4cu, 0xb3u, 0x80u, 0x41u, 0x3eu, 0x43u, 0x50u};

    if (io == NULL || io->read == NULL || io->write == NULL || bias == NULL)
        return 0;

    bias->object = 0u;
    bias->active = 0u;

    uint32_t dsktop = 0u;
    uint32_t object = 0u;
    uint32_t prologue = 0u;
    uint32_t length = 0u;

    if (!read_word(io, RPL_DSKTOP, &dsktop) || dsktop < RPL_MAIN_FIRST ||
        dsktop > RPL_MAIN_END - 5u || !read_word(io, dsktop, &object) ||
        object > RPL_MAIN_END - 24u || !read_word(io, object, &prologue) ||
        !read_word(io, object + 5u, &length) || length < 19u)
        return 0;

    if (prologue == RPL_DOCSTR) {
        for (uint32_t i = 0u; i < (uint32_t)(sizeof(launcher_magic) / sizeof(launcher_magic[0]));
             ++i) {
            uint8_t byte = 0u;
            if (!read_byte(io, object + 10u + i * 2u, &byte) || byte != launcher_magic[i])
                return 0;
        }
    } else if (prologue == 0x2a4eu && length == 45u && object >= RPL_MAIN_FIRST &&
               object <= RPL_MAIN_END - 50u && stage0_code != 0u && (stage0_code & 3u) == 0u) {
        /* ARMToolbox can replace the string with a five-word HEXSTR launch
         * descriptor. Match both its entry and code-base words to the actual
         * executing stage0, rather than hiding an arbitrary user HEXSTR. The
         * native PrRUN continuation consumes this cell after ARM returns. */
        uint32_t entry = 0u, code_base = 0u;
        for (uint32_t i = 0u; i < 4u; ++i) {
            uint8_t entry_byte, base_byte;
            if (!read_byte(io, object + 18u + 2u * i, &entry_byte) ||
                !read_byte(io, object + 34u + 2u * i, &base_byte))
                return 0;
            entry |= (uint32_t)entry_byte << (8u * i);
            code_base |= (uint32_t)base_byte << (8u * i);
        }
        if (entry != stage0_code || code_base != stage0_code)
            return 0;
    } else {
        return 0;
    }

    /*
     * HPGCC 1.1's sat_stack_init() pseudo-drops this launcher cell without
     * changing AVMEM. Preserve that exact reservation: it is needed again when
     * the launcher object is restored after HPC1 returns.
     */
    if (!io->write(io->context, RPL_DSKTOP, dsktop + 5u))
        return -1;

    bias->object = object;
    bias->active = 1u;
    return 1;
}

int hpclang_stage_stack_restore(const struct hpclang_stage_stack_io *io,
                                const struct stage_launcher_bias *bias)
{
    if (bias == NULL || bias->active == 0u)
        return 1;
    if (io == NULL || io->read == NULL || io->write == NULL)
        return 0;

    uint32_t dsktop = 0u;
    if (!read_word(io, RPL_DSKTOP, &dsktop) || dsktop < RPL_MAIN_FIRST + 5u ||
        dsktop > RPL_MAIN_END)
        return 0;

    uint32_t slot = dsktop - 5u;
    if (!io->write(io->context, slot, bias->object))
        return 0;
    return io->write(io->context, RPL_DSKTOP, slot);
}
