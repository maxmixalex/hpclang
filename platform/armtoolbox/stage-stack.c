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

uint32_t *hpclang_stage_immediate_return_root(const struct hpclang_stage_stack_io *io,
                                              uint32_t continuation, uint32_t *machine_stack,
                                              uint32_t cursor)
{
    if (!io || !io->read || !machine_stack || cursor >= 8u || continuation > RPL_MAIN_END - 2u)
        return NULL;
    uint8_t instruction;
    if (!read_byte(io, continuation, &instruction) ||
        (instruction != 0x00u && instruction != 0x10u && instruction != 0x20u &&
         instruction != 0x30u))
        return NULL;
    uint32_t *root = &machine_stack[(cursor + 7u) & 7u];
    return *root < RPL_MAIN_END ? root : NULL;
}

unsigned hpclang_stage_return_roots_with_carry(const struct hpclang_stage_stack_io *io,
                                               uint32_t continuation, uint32_t *machine_stack,
                                               uint32_t cursor, int carry, uint32_t **roots)
{
    if (!roots || !io || !io->read || !machine_stack || cursor >= 8u || carry < -1 || carry > 1)
        return 0;
    unsigned count = 0;
    uint32_t stack[8];
    for (unsigned i = 0; i < 8u; ++i)
        stack[i] = machine_stack[i];
    unsigned original = 255u;
    /* Track call-generated continuations separately: a push overwrites that
     * cell's original value, which must never be reported as a consumed root. */
    for (unsigned steps = 0; count < 8u && steps < 64u; ++steps) {
        uint8_t opcode;
        if (continuation > RPL_MAIN_END - 2u || !read_byte(io, continuation, &opcode))
            break;
        uint32_t *root = hpclang_stage_immediate_return_root(io, continuation, stack, cursor);
        if (root) {
            if (opcode == 0x20u)
                carry = 1;
            else if (opcode == 0x30u)
                carry = 0;
        } else if ((opcode & 15u) == 4u || (opcode & 15u) == 5u) {
            uint32_t displacement;
            if (carry < 0 || continuation > RPL_MAIN_END - 3u ||
                !io->read(io->context, continuation + 1u, 2, &displacement) || displacement > 255u)
                break;
            if (carry != ((opcode & 15u) == 4u)) {
                continuation = (continuation + 3u) & 0xfffffu;
                continue;
            }
            if (displacement) {
                int32_t offset =
                    displacement < 128u ? (int32_t)displacement : (int32_t)displacement - 256;
                continuation = (uint32_t)((int32_t)continuation + 1 + offset) & 0xfffffu;
                continue;
            }
            /* RTNC/RTNNC consume the top only on the established taken path. */
            root = &stack[(cursor + 7u) & 7u];
            if (*root >= RPL_MAIN_END)
                break;
        }
        if (root) {
            cursor = (cursor + 7u) & 7u;
            if (original & (1u << cursor)) {
                roots[count++] = machine_stack + cursor;
                original &= ~(1u << cursor);
            }
            continuation = *root;
            continue;
        }
        unsigned prefix, width, call;
        if ((opcode & 15u) == 6u || (opcode & 15u) == 7u) {
            prefix = 1;
            width = 3;
            call = (opcode & 15u) == 7u;
        } else if (opcode == 0xc8u || opcode == 0xd8u || opcode == 0xe8u || opcode == 0xf8u) {
            prefix = 2;
            width = opcode == 0xc8u || opcode == 0xe8u ? 4 : 5;
            call = opcode == 0xe8u || opcode == 0xf8u;
        } else {
            break;
        }
        uint32_t encoded;
        if (continuation > RPL_MAIN_END - prefix - width ||
            !io->read(io->context, continuation + prefix, width, &encoded) ||
            encoded >> (4u * width))
            break;
        uint32_t next = (continuation + prefix + width) & 0xfffffu;
        if (call) {
            stack[cursor] = next;
            original &= ~(1u << cursor);
            cursor = (cursor + 1u) & 7u;
        }
        if (width == 5u) {
            continuation = encoded;
        } else {
            uint32_t range = 1u << (4u * width);
            int32_t offset = (int32_t)encoded;
            if (encoded >= range / 2u)
                offset -= (int32_t)range;
            uint32_t base = call ? next : continuation + prefix;
            continuation = (uint32_t)((int32_t)base + offset) & 0xfffffu;
        }
    }
    return count;
}

unsigned hpclang_stage_return_roots(const struct hpclang_stage_stack_io *io, uint32_t continuation,
                                    uint32_t *machine_stack, uint32_t cursor, uint32_t **roots)
{
    return hpclang_stage_return_roots_with_carry(io, continuation, machine_stack, cursor, -1,
                                                 roots);
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
    } else if (prologue == 0x2a4eu && (length == 21u || length == 45u) &&
               object >= RPL_MAIN_FIRST && object <= RPL_MAIN_END - 5u - length &&
               stage0_code != 0u && (stage0_code & 3u) == 0u) {
        /* ARMToolbox can replace the string with a two- or five-word HEXSTR
         * descriptor. Match its entry, and the separate code base when present,
         * to the executing stage0 rather than hiding an arbitrary user HEXSTR. The
         * native PrRUN continuation consumes this cell after ARM returns. */
        uint32_t entry = 0u, code_base = 0u;
        for (uint32_t i = 0u; i < 4u; ++i) {
            uint8_t entry_byte, base_byte;
            uint32_t entry_offset = length == 21u ? 10u : 18u;
            uint32_t base_offset = length == 21u ? 10u : 34u;
            if (!read_byte(io, object + entry_offset + 2u * i, &entry_byte) ||
                !read_byte(io, object + base_offset + 2u * i, &base_byte))
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

int hpclang_stage_main_page(const uint32_t *read_map, uint32_t physical_page,
                            uint32_t *saturn_start)
{
    if (!read_map || !saturn_start || (physical_page & 0xfffu))
        return -1;
    uint32_t first = 0u;
    unsigned overlaps = 0u;
    for (uint32_t page = 128u; page < 256u; ++page) {
        uint32_t base = read_map[page];
        if (base == 0u)
            continue;
        if (!((base >= physical_page && base - physical_page < 4096u) ||
              (base < physical_page && physical_page - base < 2048u)))
            continue;
        ++overlaps;
        if (base == physical_page) {
            if (first != 0u || page == 255u || read_map[page + 1u] != physical_page + 2048u)
                return -1;
            first = page;
        } else if (base != physical_page + 2048u || page == 128u ||
                   read_map[page - 1u] != physical_page) {
            return -1;
        }
    }
    if (!overlaps)
        return 0;
    if (overlaps != 2u || !first)
        return -1;
    *saturn_start = first << 12;
    return 1;
}
