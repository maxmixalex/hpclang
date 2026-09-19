#include <stdint.h>
#include <stddef.h>

#include <hpclang/abi.h>
#include <hpclang/bootstrap.h>
#include <hpclang/target.h>

#define STAGE_WINDOW_BASE   0x09000000u
#define STAGE_CODE_OFFSET   0x00000400u
#define STAGE_STACK_PAGES   2u

#define ARMSYS_PAGE_COUNT   0x914u
#define ARMSYS_L2_PHYS      0x920u

#define RPL_RSKTOP           0x000806f3u
#define RPL_DSKTOP           0x000806f8u
#define RPL_MAIN_FIRST       0x00080000u
#define RPL_MAIN_END         0x00100000u
#define RPL_PHYS_PAGE_NIBBLES 0x00002000u

#define HP39_MAIN_FIRST_NIBBLE   0x0007c651u
#define HP39_MAIN_LAST_NIBBLE    (HP39_MAIN_FIRST_NIBBLE + 255u * 16u + 15u)
#define HP39_MINI_FIRST_NIBBLE   0x0007e695u
#define HP39_MINI_LAST_NIBBLE    (HP39_MINI_FIRST_NIBBLE + 255u * 11u + 10u)
#define HP4950_MINI_FIRST_NIBBLE 0x000812c8u
#define HP4950_MINI_LAST_NIBBLE  (HP4950_MINI_FIRST_NIBBLE + 7u + 255u * 6u + 5u)
#define HP4950_BIG_OBJECT_NIBBLE 0x00084d82u
/* get_bigfont() skips either 8 bytes or 9 + name_length bytes before the
   256-record, 16-nibble raster. Require the worst-case name length so every
   Read[] page the runtime may dereference is present before publishing the
   Saturn state. */
#define HP4950_BIG_MAX_NAME_BYTES 255u
#define HP4950_BIG_LAST_NIBBLE \
    (HP4950_BIG_OBJECT_NIBBLE + \
     2u * (9u + HP4950_BIG_MAX_NAME_BYTES) + 255u * 16u + 15u)

/*
 * The official HP39gs/40gs 2.22 update images both contain the exact reviewed
 * 256-record main and mini raster tables in physical NOR pages 0x7e and 0x7f.
 * Host-side scans match P/A/S/5/1 against the known HP39/40G atlas; the two
 * models differ only in the table's nibble offset within those pages. Keep the
 * physical pages launcher-private: runtime selects the model-specific table
 * bases and sees only the stable virtual window through platform_cookie.
 */
static const uint32_t hp39_font_nor_pages[] = {
    0x0007e000u,
    0x0007f000u,
};

_Static_assert(
    sizeof(hp39_font_nor_pages) / sizeof(hp39_font_nor_pages[0]) <=
        HPCLANG_CODE_WINDOW_PAGE_CAPACITY,
    "HP39 native font window must fit before HPCLANG_CODE_BASE");

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

static int saturn_read_range_present(
    const void *saturn_state,
    uint32_t first_nibble,
    uint32_t last_nibble)
{
    if (saturn_state == NULL || last_nibble < first_nibble)
        return 0;

    /* HPGCC ARMSYS begins with SAT_CPU.Read[257]. A real stock launch therefore
       carries non-zero mappings for every ROM page used by the native font
       objects. Direct E2E's synthetic ARMSYS intentionally contains only the
       allocator/page-pool fields at 0x914+, so these Read[] entries are zero. */
    const uint32_t *read_map = (const uint32_t *)saturn_state;
    uint32_t first_page = first_nibble >> 12;
    uint32_t last_page = last_nibble >> 12;
    for (uint32_t page = first_page; page <= last_page; ++page) {
        if (read_map[page] == 0u)
            return 0;
    }
    return 1;
}

static int stage_saturn_read_nibble(
    const uint32_t *read_map,
    uint32_t address,
    uint8_t *value)
{
    if (read_map == NULL || value == NULL || address >= RPL_MAIN_END)
        return 0;

    uint32_t base = read_map[address >> 12];
    if (base == 0u)
        return 0;

    const volatile uint8_t *p =
        (const volatile uint8_t *)(uintptr_t)(
            base + ((address & UINT32_C(0x0fff)) >> 1));
    uint8_t packed = *p;
    *value = (address & 1u) != 0u
        ? (uint8_t)(packed >> 4)
        : (uint8_t)(packed & 0x0fu);
    return 1;
}

static int stage_saturn_read_word5(
    const uint32_t *read_map,
    uint32_t address,
    uint32_t *value)
{
    if (value == NULL || address > RPL_MAIN_END - 5u)
        return 0;

    uint32_t out = 0u;
    for (uint32_t i = 0u; i < 5u; ++i) {
        uint8_t nibble = 0u;
        if (!stage_saturn_read_nibble(read_map, address + i, &nibble))
            return 0;
        out |= (uint32_t)nibble << (i * 4u);
    }
    *value = out;
    return 1;
}

/*
 * Convert one 4 KiB physical main-RAM page into the corresponding 0x2000
 * Saturn-nibble interval by using Stock SAT_CPU.Read[] as the authority.
 * Two consecutive 0x1000-nibble Saturn pages normally share one ARM page.
 */
static int stage_phys_page_saturn_start(
    const uint32_t *read_map,
    uint32_t phys_page,
    uint32_t *saturn_start)
{
    if (read_map == NULL || saturn_start == NULL)
        return 0;

    phys_page &= UINT32_C(0xfffff000);
    for (uint32_t page = RPL_MAIN_FIRST >> 12;
         page < RPL_MAIN_END >> 12;
         ++page) {
        uint32_t base = read_map[page];
        if (base < phys_page || base >= phys_page + HPCLANG_PAGE_SIZE)
            continue;

        uint32_t byte_offset = base - phys_page;
        uint32_t page_nibble = page << 12;
        uint32_t nibble_offset = byte_offset << 1;
        if (nibble_offset > page_nibble)
            continue;

        uint32_t candidate = page_nibble - nibble_offset;
        if (candidate < RPL_MAIN_FIRST ||
            candidate > RPL_MAIN_END - RPL_PHYS_PAGE_NIBBLES)
            continue;

        *saturn_start = candidate;
        return 1;
    }
    return 0;
}

static int stage_rpl_memory_bounds(
    const void *saturn_state,
    volatile const uint32_t *pool_l2,
    uint32_t pool_pages,
    struct hpclang_rpl_memory_bounds *bounds)
{
    if (saturn_state == NULL || pool_l2 == NULL || bounds == NULL)
        return 0;

    const uint32_t *read_map = (const uint32_t *)saturn_state;
    uint32_t rsktop = 0u;
    uint32_t dsktop = 0u;
    if (!stage_saturn_read_word5(read_map, RPL_RSKTOP, &rsktop) ||
        !stage_saturn_read_word5(read_map, RPL_DSKTOP, &dsktop) ||
        rsktop < RPL_MAIN_FIRST || dsktop > RPL_MAIN_END ||
        rsktop >= dsktop)
        return 0;

    /*
     * If no native page lies between the two RPL stacks, the opposite Stock
     * stack is the natural fence. Otherwise tighten each side to the nearest
     * hpclang-owned physical page, matching the safety intent of HPGCC 2/3's
     * MMU-aware free-space checks without importing their MMU table format.
     */
    uint32_t temp_limit = dsktop;
    uint32_t user_limit = rsktop;

    for (uint32_t i = 0u; i < pool_pages; ++i) {
        uint32_t sat_start = 0u;
        if (!stage_phys_page_saturn_start(
                read_map, pool_l2[i] & UINT32_C(0xfffff000), &sat_start))
            continue;

        uint32_t sat_end = sat_start + RPL_PHYS_PAGE_NIBBLES;

        /* A native page must not already overlap a live RPL stack boundary. */
        if ((sat_start < rsktop && sat_end > rsktop) ||
            (sat_start < dsktop && sat_end > dsktop))
            return 0;

        if (sat_start >= rsktop && sat_start < temp_limit)
            temp_limit = sat_start;
        if (sat_end <= dsktop && sat_end > user_limit)
            user_limit = sat_end;
    }

    /* Keep the classic 11-nibble TEMPOB/return-stack safety gap. */
    if (temp_limit <= rsktop + 11u || dsktop < user_limit + 5u)
        return 0;

    bounds->magic = HPCLANG_RPL_BOUNDS_MAGIC;
    bounds->temp_limit = temp_limit;
    bounds->user_limit = user_limit;
    return 1;
}

static uintptr_t live_saturn_state(void *saturn_state, uint32_t model)
{
    switch (model) {
    case HPCLANG_MODEL_HP39GPLUS:
    case HPCLANG_MODEL_HP39GS:
    case HPCLANG_MODEL_HP40GS:
        return saturn_read_range_present(
                   saturn_state,
                   HP39_MAIN_FIRST_NIBBLE,
                   HP39_MAIN_LAST_NIBBLE) &&
               saturn_read_range_present(
                   saturn_state,
                   HP39_MINI_FIRST_NIBBLE,
                   HP39_MINI_LAST_NIBBLE)
            ? (uintptr_t)saturn_state
            : 0u;

    case HPCLANG_MODEL_HP48GII:
    case HPCLANG_MODEL_HP49GPLUS:
    case HPCLANG_MODEL_HP50G:
        return saturn_read_range_present(
                   saturn_state,
                   HP4950_MINI_FIRST_NIBBLE,
                   HP4950_MINI_LAST_NIBBLE) &&
               saturn_read_range_present(
                   saturn_state,
                   HP4950_BIG_OBJECT_NIBBLE,
                   HP4950_BIG_LAST_NIBBLE)
            ? (uintptr_t)saturn_state
            : 0u;

    default:
        return 0u;
    }
}

/*
 * Fixed-address stage1 entered from the tiny position-independent ARMToolbox
 * trampoline.
 *
 * The stock launcher has already created a page pool and a coarse L2 table for
 * the temporary 0x09000000 window. We use that pool only as startup storage;
 * HPC1 itself sees the normal hpclang v1 fixed layout.
 */
int hpclang_armtoolbox_stage1(
    void *saturn_state,
    const struct hpclang_image_header *hpc1,
    uint32_t stage1_size,
    uint32_t model)
{
    if (!hpc1_header_ok(hpc1))
        return -1;

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

    const uint8_t *src = (const uint8_t *)(hpc1 + 1);
    uint32_t left = hpc1->image_size;

    for (uint32_t i = 0; i < code_count; ++i) {
        volatile uint8_t *dst = (volatile uint8_t *)(uintptr_t)(
            STAGE_WINDOW_BASE + (code_first + i) * HPCLANG_PAGE_SIZE);
        uint32_t n = left < HPCLANG_PAGE_SIZE ? left : HPCLANG_PAGE_SIZE;

        for (uint32_t j = 0; j < n; ++j)
            dst[j] = src[j];
        for (uint32_t j = n; j < HPCLANG_PAGE_SIZE; ++j)
            dst[j] = 0;

        src += n;
        left -= n;
    }

    hpclang_cache_clean(
        (const void *)(uintptr_t)(
            STAGE_WINDOW_BASE + code_first * HPCLANG_PAGE_SIZE),
        code_count * HPCLANG_PAGE_SIZE);

    struct hpclang_launch_context launch;
    struct hpclang_rpl_memory_bounds rpl_bounds = { 0u, 0u, 0u };

    launch.mmu.l1 = (volatile uint32_t *)(uintptr_t)0x07f00000u;
    launch.mmu.l2_workspace = (volatile uint32_t *)(uintptr_t)(
        STAGE_WINDOW_BASE + workspace_page * HPCLANG_PAGE_SIZE);
    launch.mmu.l2_workspace_phys =
        old_l2[workspace_page] & 0xfffff000u;
    launch.mmu.rw_pages = rw_pages;
    launch.mmu.rw_page_count = rw_count;
    launch.mmu.code_pages = code_pages;
    launch.mmu.code_page_count = code_count;
    launch.mmu.code_window_pages = NULL;
    launch.mmu.code_window_page_count = 0u;

    /*
     * Stock HP39gs/40gs 2.22 carries the reviewed native main/mini raster
     * tables in NOR pages 0x7e and 0x7f. Expose only those pages in the unused
     * prefix of hpclang's own L1[0x12] coarse table; runtime handles each
     * model's exact nibble offset. HP39g+ remains excluded until a trustworthy
     * real-hardware full-flash fixture establishes its layout.
     */
    if (model == HPCLANG_MODEL_HP39GS || model == HPCLANG_MODEL_HP40GS) {
        launch.mmu.code_window_pages = hp39_font_nor_pages;
        launch.mmu.code_window_page_count =
            (uint32_t)(sizeof(hp39_font_nor_pages) /
                       sizeof(hp39_font_nor_pages[0]));
    }

    launch.model = model;
    launch.entry_offset = hpc1->entry_offset;
    /* Only advertise a Saturn state when its Read[] map contains every ROM
       page the current native-font runtime may dereference. This keeps Direct
       E2E's synthetic allocator block from masquerading as a live stock map. */
    launch.saturn_state = live_saturn_state(saturn_state, model);
    if (launch.mmu.code_window_page_count != 0u) {
        launch.platform_cookie = (uintptr_t)HPCLANG_CODE_WINDOW_BASE;
    } else if (
        launch.saturn_state != 0u &&
        (model == HPCLANG_MODEL_HP48GII ||
         model == HPCLANG_MODEL_HP49GPLUS ||
         model == HPCLANG_MODEL_HP50G) &&
        stage_rpl_memory_bounds(
            saturn_state, old_l2, total_pages, &rpl_bounds)) {
        /*
         * rpl_bounds lives in this stage1 stack frame, whose ARMToolbox window
         * remains mapped for the complete hpclang_bootstrap_run() call.
         */
        launch.platform_cookie = (uintptr_t)&rpl_bounds;
    } else {
        /* No mutation fence: read-only Saturn services may still be available. */
        launch.platform_cookie = old_l2_phys;
    }

    return hpclang_bootstrap_run(&launch);
}
