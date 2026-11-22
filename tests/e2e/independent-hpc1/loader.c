#include <hpclang/bootstrap.h>
#include "stage-image.h"
void hpclang_cache_clean(const void *, unsigned);
static unsigned cpsr(void)
{
    unsigned v;
    __asm__ volatile("mrs %0, cpsr" : "=r"(v));
    return v;
}
#ifdef HPCLANG_NOTEBOOK
/* The Saturn program and the aligned ARM copy can begin on different nibbles. */
static unsigned char packed_byte(const unsigned char *source, unsigned nibble)
{
    unsigned index = nibble / 2;
    return nibble & 1 ? (source[index] >> 4) | (source[index + 1] << 4) : source[index];
}

static void store_packed_byte(unsigned char *data, unsigned nibble, unsigned char value)
{
    unsigned index = nibble / 2;
    if (nibble & 1) {
        data[index] = (data[index] & 15u) | (value << 4);
        data[index + 1] = (data[index + 1] & 0xf0u) | (value >> 4);
    } else {
        data[index] = value;
    }
}

#ifdef HPCLANG_NOTEBOOK_PORT0
static unsigned owner_record(const unsigned *map, unsigned entry, unsigned last);
static unsigned backup_word(const unsigned char *ram, unsigned at, unsigned width);
void exercise(unsigned *out, unsigned char *arena, unsigned saturn_state, unsigned *frame)
#else
void exercise(unsigned *out, unsigned char *arena, unsigned saturn_state)
#endif
#else
void exercise(unsigned *out, unsigned char *arena, const struct hpclang_image_header *image,
              unsigned bytes)
#endif
{
    enum
    {
        code_count = HPCLANG_PROBE_MODEL == 4 ? 16 : 1,
#ifdef HPCLANG_NOTEBOOK
        rw_count = NOTEBOOK_RW_PAGES,
#else
        rw_count = HPCLANG_PROBE_MODEL == 4 ? 12 : 4,
#endif
        pool_bytes = (1 + code_count + rw_count) * 4096
    };
    for (unsigned i = 0; i < 134; i++)
        out[i] = 0;
    out[0] = 0x32435048u;
    out[1] = 1;
#ifdef HPCLANG_NOTEBOOK
    struct hpclang_image_header header;
    unsigned pos;
    enum
    {
        trailer = (pool_bytes + 4096u) * 2u
    };
    for (pos = trailer; pos <= trailer + 7; pos++) {
        for (unsigned i = 0; i < sizeof(header); i++)
            ((unsigned char *)&header)[i] = packed_byte(arena, pos + 2 * i);
        if (hpclang_stage_image_valid(&header, NOTEBOOK_BYTES + sizeof(header)) &&
            header.magic[3] == '1' && header.entry_offset == NOTEBOOK_ENTRY)
            break;
    }
    if (pos > trailer + 7)
        return;
    const struct hpclang_image_header *image = &header;
#else
    if (!hpclang_stage_image_valid(image, bytes) || image->magic[3] != '1' ||
        image->image_size > 4096 || image->rw_static_size > 4096)
        return;
#endif
    unsigned start = ((unsigned)arena + 4095u) & ~4095u;
    const unsigned sram_end = HPCLANG_PROBE_MODEL == 4 ? 0x08040000u : 0x08080000u;
    if (start < 0x08000000u || start + pool_bytes > sram_end ||
        start + pool_bytes > (unsigned)arena + (pool_bytes + 4096u))
        return;
    volatile unsigned *l1 = (void *)0x07f00000u;
    if ((l1[0x80] & 3u) != 2u || (l1[0x80] & 0xfff00000u) != 0x08000000u) {
        out[1] = 2;
        return;
    }
#ifdef HPCLANG_NOTEBOOK
    unsigned char *code = (void *)(start + 4096u);
    unsigned distance = 0;
    for (unsigned i = 0; i < 4; i++)
        distance |= (unsigned)packed_byte(arena, pos + 2 * (sizeof(header) + i)) << (8 * i);
    if (distance > pos || distance < 2 * NOTEBOOK_BYTES)
        return;
    unsigned source = pos - distance;
    unsigned target = 2 * ((unsigned)code - (unsigned)arena);
    if (source < target) {
        for (unsigned i = NOTEBOOK_BYTES; i; --i)
            code[i - 1] = packed_byte(arena, source + 2 * (i - 1));
    } else if (source > target) {
        for (unsigned i = 0; i < NOTEBOOK_BYTES; i++)
            code[i] = packed_byte(arena, source + 2 * i);
    }
    distance = pos - target;
    for (unsigned i = 0; i < 4; i++)
        store_packed_byte(arena, pos + 2 * (sizeof(header) + i), distance >> (8 * i));
    for (unsigned i = NOTEBOOK_BYTES; i < pool_bytes - 4096u; i++)
        code[i] = 0;
#else
    unsigned *memory = (void *)start;
    for (unsigned i = 0; i < pool_bytes / 4; i++)
        memory[i] = 0;
    unsigned char *code = (void *)(start + 4096u);
    const unsigned char *from = (const void *)(image + 1);
    for (unsigned i = 0; i < image->image_size; i++)
        code[i] = from[i];
#endif
    unsigned code_pages[code_count], rw_pages[rw_count];
    for (unsigned i = 0; i < code_count; i++)
        code_pages[i] = start + 4096u * (1 + i);
    for (unsigned i = 0; i < rw_count; i++)
        rw_pages[i] = start + (1 + code_count + i) * 4096u;
    struct hpclang_launch_context launch;
    launch.mmu.l1 = l1;
    launch.mmu.l2_workspace = (void *)start;
    launch.mmu.l2_workspace_phys = start;
    launch.mmu.rw_pages = rw_pages;
    launch.mmu.rw_page_count = rw_count;
    launch.mmu.code_pages = code_pages;
    launch.mmu.code_page_count = code_count;
    launch.mmu.code_window_pages = 0;
    launch.mmu.code_window_page_count = 0;
    launch.model = HPCLANG_PROBE_MODEL;
    launch.entry_offset = image->entry_offset;
#ifdef HPCLANG_NOTEBOOK
    launch.saturn_state = saturn_state;
#else
    launch.saturn_state = 0;
#endif
    launch.platform_cookie = 0;
#ifdef HPCLANG_NOTEBOOK_PORT0
    const unsigned char *ram = (const void *)0x08008000u;
    unsigned record = owner_record((void *)saturn_state, (unsigned)out, start + pool_bytes - 1);
    if (!record)
        return;
    unsigned rstk = backup_word(ram, 0x806f3, 5);
    unsigned dstk = backup_word(ram, 0x806f8, 5);
    if (rstk < 0x80000u || dstk <= rstk || dstk >= record)
        return;
    unsigned *roots[] = {frame};
    struct hpclang_rpl_memory_bounds bounds = {
        .magic = HPCLANG_RPL_BOUNDS_PORT0_RESIDENT,
        .temp_limit = dstk,
        .user_limit = rstk,
        .native_roots = roots,
        .native_root_count = 1,
    };
    launch.platform_cookie = (unsigned)&bounds | HPCLANG_PLATFORM_COOKIE_RPL_BOUNDS_TAG;
#endif
    out[3] = l1[0x11];
    out[4] = l1[0x12];
    out[7] = *(volatile unsigned *)0x08000008u;
    out[9] = start;
    out[10] = start + pool_bytes;
    out[14] = cpsr();
    hpclang_cache_clean((const void *)start, pool_bytes);
    out[2] = (unsigned)hpclang_bootstrap_run(&launch);
    out[15] = cpsr();
    out[5] = l1[0x11];
    out[6] = l1[0x12];
    out[8] = *(volatile unsigned *)0x08000008u;
#ifdef HPCLANG_NOTEBOOK
    const volatile struct hpclang_boot_info *boot =
        (const void *)(start + (1 + code_count) * 4096u);
    out[11] = boot->model;
    out[12] = boot->rw_bytes;
    out[1] = out[2] || out[3] != out[5] || out[4] != out[6] || out[7] != out[8];
#else
    volatile unsigned *data = (void *)(start + (2 + code_count) * 4096u);
    out[11] = data[0];
    out[12] = data[1];
    out[13] = data[2];
    out[1] = out[2] == 42 && out[3] == out[5] && out[4] == out[6] && out[7] == out[8] &&
                     out[11] == HPCLANG_PROBE_MODEL && out[12] == rw_count * 4096u &&
                     out[13] == 0x31435048u && (out[14] & 255u) == (out[15] & 255u)
                 ? 0
                 : 3;
#endif
}

#ifdef HPCLANG_NOTEBOOK_PORT0
/* Stock 2.15 HP48gII backup records use nibble addresses. */
static unsigned backup_word(const unsigned char *ram, unsigned at, unsigned width)
{
    unsigned value = 0;
    for (unsigned i = 0; i < width; ++i, ++at)
        value |= ((ram[(at - 0x80000u) / 2] >> (4 * (at & 1))) & 15u) << (4 * i);
    return value;
}

static unsigned owner_record(const unsigned *map, unsigned entry, unsigned last)
{
    /* Stop before the physical SRAM mirror; require matching Stock read/write maps. */
    const unsigned char *ram = (const void *)0x08008000u;
    for (unsigned i = 128; i < 240; ++i)
        if (map[i] != (unsigned)ram + (i - 128) * 2048 || map[257 + i] != map[i])
            return 0;
    if (entry < (unsigned)ram || last < entry || last >= 0x08040000)
        return 0;
    unsigned pc = 0x80000 + 2 * (entry - (unsigned)ram);
    unsigned record = backup_word(ram, 0x80716, 5);
    unsigned end;
    for (;;) {
        if (record < 0x80000 || record > 0xf0000 - 10 || record > pc)
            return 0;
        unsigned type = backup_word(ram, record, 5);
        if (type != 0x2b62 && type != 0x2b40)
            return 0;
        end = record + 5 + backup_word(ram, record + 5, 5);
        if (end < record + 29 || end > 0xf0000)
            return 0;
        if (pc < end) {
            if (type != 0x2b62)
                return 0;
            break;
        }
        record = end;
    }
    unsigned name_length = backup_word(ram, record + 10, 2);
    unsigned program = record + 14 + 2 * name_length;
    if (end < program + 25 || backup_word(ram, program - 2, 2) != name_length ||
        backup_word(ram, program, 5) != 0x2d9d || backup_word(ram, program + 5, 5) != 0x2dcc ||
        backup_word(ram, end - 10, 5) != 0x2911)
        return 0;
    unsigned code_end = program + 10 + backup_word(ram, program + 10, 5);
    if (code_end > end - 10 || pc < program + 15 || pc >= code_end ||
        0x80000 + 2 * (last - (unsigned)ram) + 1 >= code_end)
        return 0;
    return record;
}

void finish_port0_backup(const unsigned *map, unsigned entry)
{
    unsigned record = owner_record(map, entry, entry);
    if (!record)
        return;
    const unsigned char *ram = (const void *)0x08008000u;
    unsigned end = record + 5 + backup_word(ram, record + 5, 5);
    unsigned crc = 0;
    for (unsigned at = record + 5; at < end - 4; ++at)
        crc = (crc >> 4) ^ (((crc ^ backup_word(ram, at, 1)) & 15) * 0x1081);
    for (unsigned at = end - 4; at < end; ++at) {
        unsigned char *byte = (unsigned char *)ram + (at - 0x80000u) / 2;
        unsigned shift = 4 * (at & 1);
        *byte = (*byte & ~(15u << shift)) | ((crc & 15u) << shift);
        crc >>= 4;
    }
}
#endif
