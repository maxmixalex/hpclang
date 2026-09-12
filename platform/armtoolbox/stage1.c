#include <stdint.h>
#include <stddef.h>

#include <hpclang/abi.h>
#include <hpclang/bootstrap.h>

#define STAGE_WINDOW_BASE   0x09000000u
#define STAGE_CODE_OFFSET   0x00000400u
#define STAGE_STACK_PAGES   2u

#define ARMSYS_PAGE_COUNT   0x914u
#define ARMSYS_L2_PHYS      0x920u

#define HPZ1_MAGIC_0 'H'
#define HPZ1_MAGIC_1 'P'
#define HPZ1_MAGIC_2 'Z'
#define HPZ1_MAGIC_3 '1'
#define LZ4_MIN_MATCH 4u

struct hpclang_packed_hpc1 {
    uint8_t magic[4];
    struct hpclang_image_header hpc1;
    uint32_t packed_size;
    uint8_t data[];
};

void hpclang_cache_clean(const void *start, uint32_t size);

static uint32_t load32(const void *base, uint32_t offset)
{
    return *(const uint32_t *)((const uint8_t *)base + offset);
}

static int hpc1_header_ok(const struct hpclang_image_header *header)
{
    return header->magic[0] == 'H' &&
           header->magic[1] == 'P' &&
           header->magic[2] == 'C' &&
           header->magic[3] == '1';
}

static int hpz1_header_ok(const struct hpclang_packed_hpc1 *packed)
{
    return packed->magic[0] == HPZ1_MAGIC_0 &&
           packed->magic[1] == HPZ1_MAGIC_1 &&
           packed->magic[2] == HPZ1_MAGIC_2 &&
           packed->magic[3] == HPZ1_MAGIC_3 &&
           hpc1_header_ok(&packed->hpc1) &&
           packed->packed_size != 0u;
}

static int lz4_read_length(
    const uint8_t **src,
    const uint8_t *end,
    uint32_t base,
    uint32_t *length)
{
    uint32_t value = base;

    if (base == 15u) {
        for (;;) {
            if (*src >= end)
                return 0;
            uint32_t byte = *(*src)++;
            if (value > UINT32_MAX - byte)
                return 0;
            value += byte;
            if (byte != 255u)
                break;
        }
    }

    *length = value;
    return 1;
}

static int lz4_decode(
    const uint8_t *src,
    uint32_t src_size,
    volatile uint8_t *dst,
    uint32_t dst_size)
{
    const uint8_t *in = src;
    const uint8_t *in_end = src + src_size;
    volatile uint8_t *out = dst;
    volatile uint8_t *out_end = dst + dst_size;

    while (in < in_end) {
        uint32_t token = *in++;
        uint32_t literal_length;
        uint32_t match_length;

        if (!lz4_read_length(&in, in_end, token >> 4, &literal_length))
            return 0;
        if ((uint32_t)(in_end - in) < literal_length ||
            (uint32_t)(out_end - out) < literal_length)
            return 0;

        for (uint32_t i = 0; i < literal_length; ++i)
            *out++ = *in++;

        if (in == in_end)
            break;
        if ((uint32_t)(in_end - in) < 2u)
            return 0;

        uint32_t offset = (uint32_t)in[0] | ((uint32_t)in[1] << 8);
        in += 2;
        if (offset == 0u || offset > (uint32_t)(out - dst))
            return 0;

        if (!lz4_read_length(&in, in_end, token & 0x0fu, &match_length))
            return 0;
        if (match_length > UINT32_MAX - LZ4_MIN_MATCH)
            return 0;
        match_length += LZ4_MIN_MATCH;
        if ((uint32_t)(out_end - out) < match_length)
            return 0;

        volatile uint8_t *match = out - offset;
        for (uint32_t i = 0; i < match_length; ++i)
            *out++ = *match++;
    }

    return out == out_end;
}

/*
 * Fixed-address stage1 entered from the tiny position-independent ARMToolbox
 * trampoline.
 *
 * The stock launcher has already created a page pool and a coarse L2 table for
 * the temporary 0x09000000 window. We use that pool only as startup storage;
 * HPC1 itself sees the normal hpclang v1 fixed layout.
 *
 * hpc1_blob is normally a raw HPC1 image. Large focused applications may use
 * the launcher-private HPZ1 staging wrapper instead: the original HPC1 header
 * remains uncompressed and stage1 expands the LZ4 body directly into the final
 * code pages, so no second full-size scratch image is required. HPZ1 never
 * crosses the public HPC1/application ABI boundary.
 */
int hpclang_armtoolbox_stage1(
    void *saturn_state,
    const void *hpc1_blob,
    uint32_t stage1_size,
    uint32_t model)
{
    const struct hpclang_image_header *hpc1;
    const uint8_t *image_src = NULL;
    uint32_t packed_size = 0u;
    int packed = 0;

    const struct hpclang_image_header *raw =
        (const struct hpclang_image_header *)hpc1_blob;
    if (hpc1_header_ok(raw)) {
        hpc1 = raw;
        image_src = (const uint8_t *)(raw + 1);
    } else {
        const struct hpclang_packed_hpc1 *hpz1 =
            (const struct hpclang_packed_hpc1 *)hpc1_blob;
        if (!hpz1_header_ok(hpz1))
            return -1;
        hpc1 = &hpz1->hpc1;
        image_src = hpz1->data;
        packed_size = hpz1->packed_size;
        packed = 1;
    }

    uint32_t total_pages = load32(saturn_state, ARMSYS_PAGE_COUNT);
    uint32_t old_l2_phys = load32(saturn_state, ARMSYS_L2_PHYS);
    volatile uint32_t *old_l2 =
        (volatile uint32_t *)(uintptr_t)STAGE_WINDOW_BASE;

    uint32_t stage_pages =
        (STAGE_CODE_OFFSET + stage1_size + HPCLANG_PAGE_SIZE - 1u) /
        HPCLANG_PAGE_SIZE;
    uint32_t code_count =
        (hpc1->image_size + HPCLANG_PAGE_SIZE - 1u) / HPCLANG_PAGE_SIZE;

    uint32_t workspace_page = stage_pages;
    uint32_t code_first = workspace_page + 1u;
    uint32_t rw_first = code_first + code_count;

    if (code_count > HPCLANG_CODE_PAGE_CAPACITY)
        return -2;
    if (rw_first + STAGE_STACK_PAGES >= total_pages)
        return -3;

    uint32_t rw_count = total_pages - STAGE_STACK_PAGES - rw_first;
    uint32_t rw_needed =
        1u +
        (hpc1->rw_static_size + HPCLANG_PAGE_SIZE - 1u) / HPCLANG_PAGE_SIZE +
        HPCLANG_STACK_MIN / HPCLANG_PAGE_SIZE;

    if (rw_count < rw_needed || rw_count > HPCLANG_RW_PAGE_CAPACITY)
        return -4;

    uint32_t code_pages[HPCLANG_CODE_PAGE_CAPACITY];
    uint32_t rw_pages[HPCLANG_RW_PAGE_CAPACITY];

    for (uint32_t i = 0; i < code_count; ++i)
        code_pages[i] = old_l2[code_first + i] & 0xfffff000u;

    for (uint32_t i = 0; i < rw_count; ++i)
        rw_pages[i] = old_l2[rw_first + i] & 0xfffff000u;

    volatile uint8_t *code_dst = (volatile uint8_t *)(uintptr_t)(
        STAGE_WINDOW_BASE + code_first * HPCLANG_PAGE_SIZE);

    if (packed) {
        if (!lz4_decode(image_src, packed_size, code_dst, hpc1->image_size))
            return -5;
    } else {
        for (uint32_t i = 0; i < hpc1->image_size; ++i)
            code_dst[i] = image_src[i];
    }

    uint32_t padded_size = code_count * HPCLANG_PAGE_SIZE;
    for (uint32_t i = hpc1->image_size; i < padded_size; ++i)
        code_dst[i] = 0;

    hpclang_cache_clean(
        (const void *)(uintptr_t)(
            STAGE_WINDOW_BASE + code_first * HPCLANG_PAGE_SIZE),
        padded_size);

    struct hpclang_launch_context launch;

    launch.mmu.l1 = (volatile uint32_t *)(uintptr_t)0x07f00000u;
    launch.mmu.l2_workspace = (volatile uint32_t *)(uintptr_t)(
        STAGE_WINDOW_BASE + workspace_page * HPCLANG_PAGE_SIZE);
    launch.mmu.l2_workspace_phys =
        old_l2[workspace_page] & 0xfffff000u;
    launch.mmu.rw_pages = rw_pages;
    launch.mmu.rw_page_count = rw_count;
    launch.mmu.code_pages = code_pages;
    launch.mmu.code_page_count = code_count;

    launch.model = model;
    launch.entry_offset = hpc1->entry_offset;
    launch.saturn_state = (uintptr_t)saturn_state;
    launch.platform_cookie = old_l2_phys;

    return hpclang_bootstrap_run(&launch);
}
