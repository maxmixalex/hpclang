#include <stdint.h>
#include <stddef.h>

#include <hpclang/abi.h>
#include <hpclang/bootstrap.h>
#include <hpclang/target.h>

#include "hp39-portal-private.h"

#define STAGE_WINDOW_BASE   0x09000000u
#define STAGE_CODE_OFFSET   0x00000400u
#define STAGE_STACK_PAGES   2u

#define ARMSYS_PAGE_COUNT   0x914u
#define ARMSYS_L2_PHYS      0x920u
#define ARMSYS_C_LO         0x91cu
#define COARSE_L1_FLAGS     0x0011u

#define RPL_RSKTOP           0x000806f3u
#define RPL_DSKTOP           0x000806f8u
#define RPL_DOCSTR           0x00002a2cu
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
void hpclang_mmu_sync(
    volatile uint32_t *l2_workspace,
    volatile uint32_t *l1);
uint32_t hpclang_irq_save_disable(void);
void hpclang_irq_restore(uint32_t cpsr);

struct hpclang_hp39_portal_context hpclang_hp39_portal_context
    __attribute__((section(".data.hp39.portal"), aligned(8))) = {
        .public_context = {
            .magic = HPCLANG_HP39_PLATFORM_MAGIC,
            .font_window_base = 0u,
            .portal_entry = 0u,
        },
    };

/*
 * Keep the evaluator probe on its own 64-byte cache/page sub-boundary. The ARM
 * stage writes it once, cleans it, and then executes it only through Stock's
 * Saturn Read[] alias. 64-byte alignment also proves this 64-byte buffer cannot
 * cross a 4 KiB page.
 *
 * The synthetic program deliberately has no application-visible RPL effect:
 *
 *   machine: GETPTR; A=colon; D0=return_stream; PC=(A)
 *   colon:   DOCOL GetApDirList SEMI
 *   return:  -> DOCODE
 *   DOCODE:  restore C.W; ARMSYS
 *
 * GetApDirList is a real read-only Stock internal Aplet-catalog primitive. ROM
 * evidence shows it returns the ApDirList list (or NULL{} when absent) through
 * a context-restoring wrapper. The result remains on the private RPL data stack
 * until the ARM resume validates exactly one pushed DOLIST cell, records its
 * pointer and rolls DSKTOP back to the pre-call value. DOCOL/SEMI still balance
 * their B/D threaded-return changes before the DOCODE machine resume.
 */
static uint8_t hp39_saturn_portal_thunk[64]
    __attribute__((section(".data.hp39.thunk"), aligned(64)));

enum {
    HP39P_MACHINE_NIBBLE = 0u,
    HP39P_COLON_NIBBLE = 28u,
    HP39P_RETURN_NIBBLE = 43u,
    HP39P_DOCODE_NIBBLE = 48u,
    HP39P_C_TEMPLATE_NIBBLE = 74u,
    HP39P_TOTAL_NIBBLES = 90u,
    HP39P_DOCODE_MACHINE_NIBBLES = 16u,
    HP39P_DOCODE_SIZE_NIBBLES = 5u + HP39P_DOCODE_MACHINE_NIBBLES
};

_Static_assert(
    HP39P_TOTAL_NIBBLES <= sizeof(hp39_saturn_portal_thunk) * 2u,
    "HP39 Saturn evaluator probe must fit its private buffer");

struct hp39_saturn_builder {
    uint8_t *bytes;
    uint32_t capacity_nibbles;
    uint32_t nibbles;
};

static int hp39_saturn_put_nibble(
    struct hp39_saturn_builder *builder,
    uint8_t nibble)
{
    if (builder == NULL || nibble > 0x0fu ||
        builder->nibbles >= builder->capacity_nibbles)
        return 0;

    uint32_t offset = builder->nibbles++;
    if ((offset & 1u) != 0u) {
        builder->bytes[offset >> 1] |= (uint8_t)(nibble << 4);
    } else {
        builder->bytes[offset >> 1] = nibble;
    }
    return 1;
}

static int hp39_saturn_put_word5(
    struct hp39_saturn_builder *builder,
    uint32_t value)
{
    if (value >= UINT32_C(0x00100000))
        return 0;
    for (uint32_t i = 0u; i < 5u; ++i) {
        if (!hp39_saturn_put_nibble(
                builder,
                (uint8_t)((value >> (i * 4u)) & 0x0fu)))
            return 0;
    }
    return 1;
}

static int hp39_saturn_put_u32(
    struct hp39_saturn_builder *builder,
    uint32_t value)
{
    for (uint32_t i = 0u; i < 8u; ++i) {
        if (!hp39_saturn_put_nibble(
                builder,
                (uint8_t)((value >> (i * 4u)) & 0x0fu)))
            return 0;
    }
    return 1;
}

static int hp39_saturn_put_sequence(
    struct hp39_saturn_builder *builder,
    const uint8_t *nibbles,
    uint32_t count)
{
    if (nibbles == NULL && count != 0u)
        return 0;
    for (uint32_t i = 0u; i < count; ++i) {
        if (!hp39_saturn_put_nibble(builder, nibbles[i]))
            return 0;
    }
    return 1;
}

static uint8_t hp39_saturn_buffer_nibble(
    const uint8_t *bytes,
    uint32_t nibble)
{
    uint8_t packed = bytes[nibble >> 1];
    return (nibble & 1u) != 0u
        ? (uint8_t)(packed >> 4)
        : (uint8_t)(packed & 0x0fu);
}

static int stage_saturn_read_nibble(
    const uint32_t *read_map,
    uint32_t address,
    uint8_t *value);
static int stage_phys_page_saturn_start(
    const uint32_t *read_map,
    uint32_t phys_page,
    uint32_t *saturn_start);

static int hp39_portal_prepare_thunk(
    void *saturn_state,
    struct hpclang_hp39_portal_context *portal)
{
    /*
     * These machine encodings are fixed by the HP39 Saturn ISA and are also
     * present in the original HPGCC39 LIB275 build:
     *   8F C03F0  GOSBVL $jGETPTR (#0F30C)
     *   1B xxxxx  D0=(5)
     *   132       AD0EX
     *   808C      PC=(A)
     *   1567      C=DAT0 W
     *   80BFE     ARMSYS
     */
    static const uint8_t getptr[] = {
        0x8u, 0xfu, 0xcu, 0x0u, 0x3u, 0xfu, 0x0u
    };
    static const uint8_t d0_imm[] = { 0x1u, 0xbu };
    static const uint8_t ad0ex[] = { 0x1u, 0x3u, 0x2u };
    static const uint8_t pc_indirect[] = { 0x8u, 0x0u, 0x8u, 0xcu };
    static const uint8_t c_dat0_w[] = { 0x1u, 0x5u, 0x6u, 0x7u };
    static const uint8_t armsys[] = { 0x8u, 0x0u, 0xbu, 0xfu, 0xeu };

    if (saturn_state == NULL || portal == NULL)
        return 0;

    uintptr_t thunk_address = (uintptr_t)hp39_saturn_portal_thunk;
    uintptr_t stage_limit =
        (uintptr_t)STAGE_WINDOW_BASE +
        (uintptr_t)HPCLANG_L2_ENTRIES * HPCLANG_PAGE_SIZE;
    if (thunk_address < (uintptr_t)STAGE_WINDOW_BASE ||
        thunk_address > stage_limit - sizeof(hp39_saturn_portal_thunk))
        return 0;

    uintptr_t delta = thunk_address - (uintptr_t)STAGE_WINDOW_BASE;
    uint32_t page_index = (uint32_t)(delta / HPCLANG_PAGE_SIZE);
    uint32_t byte_offset = (uint32_t)(delta % HPCLANG_PAGE_SIZE);
    if (page_index >= HPCLANG_L2_ENTRIES ||
        byte_offset > HPCLANG_PAGE_SIZE - sizeof(hp39_saturn_portal_thunk))
        return 0;

    const volatile uint32_t *stage_l2 =
        (const volatile uint32_t *)(uintptr_t)STAGE_WINDOW_BASE;
    uint32_t saturn_page = 0u;
    if (!stage_phys_page_saturn_start(
            (const uint32_t *)saturn_state,
            stage_l2[page_index] & UINT32_C(0xfffff000),
            &saturn_page))
        return 0;

    uint32_t saturn_base = saturn_page + byte_offset * 2u;
    if (saturn_base > RPL_MAIN_END - HP39P_TOTAL_NIBBLES)
        return 0;

    uint32_t colon = saturn_base + HP39P_COLON_NIBBLE;
    uint32_t return_stream = saturn_base + HP39P_RETURN_NIBBLE;
    uint32_t docode = saturn_base + HP39P_DOCODE_NIBBLE;
    uint32_t c_template = saturn_base + HP39P_C_TEMPLATE_NIBBLE;

    struct hp39_saturn_builder builder = {
        .bytes = hp39_saturn_portal_thunk,
        .capacity_nibbles =
            (uint32_t)sizeof(hp39_saturn_portal_thunk) * 2u,
        .nibbles = 0u,
    };

    /* Machine entry: restore interpreter state and enter synthetic DOCOL. */
    if (!hp39_saturn_put_sequence(
            &builder, getptr, (uint32_t)sizeof(getptr)) ||
        !hp39_saturn_put_sequence(
            &builder, d0_imm, (uint32_t)sizeof(d0_imm)) ||
        !hp39_saturn_put_word5(&builder, colon) ||
        !hp39_saturn_put_sequence(
            &builder, ad0ex, (uint32_t)sizeof(ad0ex)) ||
        !hp39_saturn_put_sequence(
            &builder, d0_imm, (uint32_t)sizeof(d0_imm)) ||
        !hp39_saturn_put_word5(&builder, return_stream) ||
        !hp39_saturn_put_sequence(
            &builder, pc_indirect, (uint32_t)sizeof(pc_indirect)) ||
        builder.nibbles != HP39P_COLON_NIBBLE)
        return 0;

    /*
     * Synthetic colon object:
     *   [DOCOL][GetApDirList][SEMI]
     *
     * GetApDirList is supported entry #3F5AB. Its resolved Stock body reads
     * the internal ApDirList ID through a temporary context binding and always
     * returns one object (the list or NULL{}). The ARM resume owns the matching
     * one-cell rollback after validating the returned DOLIST.
     */
    if (!hp39_saturn_put_word5(&builder, UINT32_C(0x00002d9d)) ||
        !hp39_saturn_put_word5(&builder, UINT32_C(0x0003f5ab)) ||
        !hp39_saturn_put_word5(&builder, UINT32_C(0x0000312b)) ||
        builder.nibbles != HP39P_RETURN_NIBBLE)
        return 0;

    /* SEMI returns here and executes the synthetic DOCODE object. */
    if (!hp39_saturn_put_word5(&builder, docode) ||
        builder.nibbles != HP39P_DOCODE_NIBBLE)
        return 0;

    /*
     * DOCODE framing. Its five-nibble size field counts from the size cell
     * through the end of the native body, hence 5 + 16 = 21 nibbles.
     */
    if (!hp39_saturn_put_word5(&builder, UINT32_C(0x00002dcc)) ||
        !hp39_saturn_put_word5(&builder, HP39P_DOCODE_SIZE_NIBBLES) ||
        !hp39_saturn_put_sequence(
            &builder, d0_imm, (uint32_t)sizeof(d0_imm)) ||
        !hp39_saturn_put_word5(&builder, c_template) ||
        !hp39_saturn_put_sequence(
            &builder, c_dat0_w, (uint32_t)sizeof(c_dat0_w)) ||
        !hp39_saturn_put_sequence(
            &builder, armsys, (uint32_t)sizeof(armsys)) ||
        builder.nibbles != HP39P_C_TEMPLATE_NIBBLE)
        return 0;

    /*
     * ARMSYS consumes C.LO as the ARM entry and C.HI as the Stock ARM RAM
     * context. Keep the launcher's original high half, but target the portal
     * resume stub in the low half. ARMSYS_L2_PHYS and C.HI intentionally share
     * offset 0x920 in the historical HPGCC39 ARMSYS block.
     */
    uint32_t c_hi =
        *(const volatile uint32_t *)(
            (const uint8_t *)saturn_state + ARMSYS_L2_PHYS);
    if (!hp39_saturn_put_u32(
            &builder,
            (uint32_t)(uintptr_t)&hpclang_hp39_portal_resume) ||
        !hp39_saturn_put_u32(&builder, c_hi) ||
        builder.nibbles != HP39P_TOTAL_NIBBLES)
        return 0;

    hpclang_cache_clean(
        hp39_saturn_portal_thunk,
        (uint32_t)sizeof(hp39_saturn_portal_thunk));

    /*
     * Prove the complete generated stream through the Saturn alias before the
     * portal is published. This catches wrong physical-page inversion, odd
     * nibble packing and stale aliasing without executing guessed memory.
     */
    for (uint32_t i = 0u; i < HP39P_TOTAL_NIBBLES; ++i) {
        uint8_t nibble = 0u;
        if (!stage_saturn_read_nibble(
                (const uint32_t *)saturn_state,
                saturn_base + i,
                &nibble) ||
            nibble != hp39_saturn_buffer_nibble(
                hp39_saturn_portal_thunk, i))
            return 0;
    }

    portal->saturn_entry = saturn_base + HP39P_MACHINE_NIBBLE;
    return 1;
}

static uint32_t load32(const void *base, uint32_t offset)
{
    return *(const uint32_t *)((const uint8_t *)base + offset);
}

static void store32(void *base, uint32_t offset, uint32_t value)
{
    *(volatile uint32_t *)((uint8_t *)base + offset) = value;
}

static void hp39_portal_configure(
    const struct hpclang_launch_context *launch,
    void *saturn_state,
    uint32_t model,
    uint32_t saturn_pc,
    uintptr_t emulator_lr,
    uint32_t stock_cpsr,
    uintptr_t stock_frame)
{
    struct hpclang_hp39_portal_context *portal =
        &hpclang_hp39_portal_context;

    portal->public_context.magic = HPCLANG_HP39_PLATFORM_MAGIC;
    portal->public_context.font_window_base =
        (uintptr_t)HPCLANG_CODE_WINDOW_BASE;
    portal->public_context.portal_entry = 0u;
    portal->saturn_state = (uintptr_t)saturn_state;
    portal->emulator_lr = emulator_lr;
    portal->saturn_entry = 0u;
    portal->armsys_target = 0u;
    portal->stock_cpsr = stock_cpsr;
    portal->l1 = (uintptr_t)launch->mmu.l1;
    portal->l2_workspace = (uintptr_t)launch->mmu.l2_workspace;
    portal->stock_data_l1 =
        launch->mmu.l1[HPCLANG_L1_DATA_INDEX];
    portal->stock_code_l1 =
        launch->mmu.l1[HPCLANG_L1_CODE_INDEX];
    portal->hpc_data_l1 =
        launch->mmu.l2_workspace_phys | COARSE_L1_FLAGS;
    portal->hpc_code_l1 =
        (launch->mmu.l2_workspace_phys + HPCLANG_L2_BYTES) |
        COARSE_L1_FLAGS;
    portal->stock_frame = stock_frame;
    portal->stock_sp = stock_frame + 14u * sizeof(uint32_t);
    portal->active = 0u;
    portal->trips = 0u;
    portal->last_cpsr = 0u;
    portal->pre_dsktop = 0u;
    portal->result_object = 0u;
    portal->result_prologue = 0u;

    if (stock_frame != 0u) {
        const volatile uint32_t *saved =
            (const volatile uint32_t *)stock_frame;
        for (uint32_t i = 0u; i < 14u; ++i)
            portal->stock_regs[i] = saved[i];
        for (uint32_t i = 0u; i < 8u; ++i)
            portal->emulator_regs[i] = saved[i + 4u];
    }

    if ((model == HPCLANG_MODEL_HP39GS ||
         model == HPCLANG_MODEL_HP40GS) &&
        launch->saturn_state != 0u &&
        saturn_pc >= 5u &&
        emulator_lr != 0u &&
        stock_frame != 0u) {
        portal->armsys_target =
            load32(saturn_state, ARMSYS_C_LO);
        if (hp39_portal_prepare_thunk(saturn_state, portal)) {
            portal->public_context.portal_entry =
                (uintptr_t)&hpclang_hp39_portal_enter;
        }
    }
}

int hpclang_hp39_portal_prepare_stock(void)
{
    struct hpclang_hp39_portal_context *portal =
        &hpclang_hp39_portal_context;
    volatile uint32_t *l1 =
        (volatile uint32_t *)portal->l1;
    const uint32_t *read_map =
        (const uint32_t *)portal->saturn_state;

    uint32_t dsktop = 0u;
    if (!stage_saturn_read_word5(read_map, RPL_DSKTOP, &dsktop) ||
        dsktop < 5u || dsktop > RPL_MAIN_END) {
        portal->pre_dsktop = 0u;
        return -3;
    }

    portal->pre_dsktop = dsktop;
    portal->result_object = 0u;
    portal->result_prologue = 0u;

    store32(
        (void *)portal->saturn_state,
        ARMSYS_C_LO,
        (uint32_t)(uintptr_t)&hpclang_hp39_portal_resume);

    l1[HPCLANG_L1_DATA_INDEX] = portal->stock_data_l1;
    l1[HPCLANG_L1_CODE_INDEX] = portal->stock_code_l1;
    hpclang_mmu_sync(
        (volatile uint32_t *)portal->l2_workspace,
        l1);

    hpclang_irq_restore(portal->stock_cpsr);
    return 0;
}

int hpclang_hp39_portal_restore_hpc(void)
{
    struct hpclang_hp39_portal_context *portal =
        &hpclang_hp39_portal_context;
    volatile uint32_t *l1 =
        (volatile uint32_t *)portal->l1;

    portal->last_cpsr = hpclang_irq_save_disable();

    int result = 0;
    const uint32_t *read_map =
        (const uint32_t *)portal->saturn_state;
    uint32_t post_dsktop = 0u;
    uint32_t object = 0u;
    uint32_t prologue = 0u;

    if (portal->pre_dsktop < 5u ||
        !stage_saturn_read_word5(read_map, RPL_DSKTOP, &post_dsktop)) {
        result = -4;
    } else {
        if (post_dsktop + 5u != portal->pre_dsktop ||
            !stage_saturn_read_word5(read_map, post_dsktop, &object) ||
            object > RPL_MAIN_END - 5u ||
            !stage_saturn_read_word5(read_map, object, &prologue) ||
            prologue != UINT32_C(0x00002a74)) {
            result = -5;
        } else {
            portal->result_object = object;
            portal->result_prologue = prologue;
        }

        /*
         * GetApDirList is a read-only one-result primitive. Roll the private
         * RPL data stack back even when result-shape validation fails, so the
         * probe never leaks its temporary cell into Stock execution.
         */
        if (!stage_saturn_write_word5(
                read_map, RPL_DSKTOP, portal->pre_dsktop))
            result = -6;
    }

    store32(
        (void *)portal->saturn_state,
        ARMSYS_C_LO,
        portal->armsys_target);

    /*
     * The emulator may have reused the original stage0 save slots while Stock
     * mappings/SP were active. Rebuild them from the immutable first-entry
     * copy so final stage0 teardown still restores the true launcher caller.
     */
    volatile uint32_t *saved =
        (volatile uint32_t *)portal->stock_frame;
    for (uint32_t i = 0u; i < 14u; ++i)
        saved[i] = portal->stock_regs[i];

    l1[HPCLANG_L1_DATA_INDEX] = portal->hpc_data_l1;
    l1[HPCLANG_L1_CODE_INDEX] = portal->hpc_code_l1;
    hpclang_mmu_sync(
        (volatile uint32_t *)portal->l2_workspace,
        l1);

    ++portal->trips;
    portal->active = 0u;
    return result;
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

static int stage_saturn_write_nibble(
    const uint32_t *read_map,
    uint32_t address,
    uint8_t value)
{
    if (read_map == NULL || value > 0x0fu || address >= RPL_MAIN_END)
        return 0;

    uint32_t base = read_map[address >> 12];
    if (base == 0u)
        return 0;

    volatile uint8_t *p =
        (volatile uint8_t *)(uintptr_t)(
            base + ((address & UINT32_C(0x0fff)) >> 1));
    uint8_t packed = *p;
    if ((address & 1u) != 0u)
        packed = (uint8_t)((packed & 0x0fu) | (uint8_t)(value << 4));
    else
        packed = (uint8_t)((packed & 0xf0u) | value);
    *p = packed;
    return 1;
}

static int stage_saturn_write_word5(
    const uint32_t *read_map,
    uint32_t address,
    uint32_t value)
{
    if (address > RPL_MAIN_END - 5u)
        return 0;

    /* Verify the complete target is mapped before the first write. */
    for (uint32_t i = 0u; i < 5u; ++i) {
        uint8_t ignored = 0u;
        if (!stage_saturn_read_nibble(read_map, address + i, &ignored))
            return 0;
    }

    for (uint32_t i = 0u; i < 5u; ++i) {
        if (!stage_saturn_write_nibble(
                read_map,
                address + i,
                (uint8_t)((value >> (i * 4u)) & 0x0fu)))
            return 0;
    }
    return 1;
}

static int stage_saturn_read_byte(
    const uint32_t *read_map,
    uint32_t address,
    uint8_t *value)
{
    if (value == NULL)
        return 0;

    uint8_t low = 0u;
    uint8_t high = 0u;
    if (!stage_saturn_read_nibble(read_map, address, &low) ||
        !stage_saturn_read_nibble(read_map, address + 1u, &high))
        return 0;

    *value = (uint8_t)(low | (uint8_t)(high << 4));
    return 1;
}

struct stage_launcher_bias {
    uint32_t object;
    uint32_t active;
};

static int stage_hide_launcher_bias(
    const void *saturn_state,
    struct stage_launcher_bias *bias)
{
    static const uint8_t launcher_magic[] = {
        0x4cu, 0xb3u, 0x80u, 0x41u, 0x3eu, 0x43u, 0x50u
    };

    if (saturn_state == NULL || bias == NULL)
        return 0;

    bias->object = 0u;
    bias->active = 0u;

    const uint32_t *read_map = (const uint32_t *)saturn_state;
    uint32_t dsktop = 0u;
    uint32_t object = 0u;
    uint32_t prologue = 0u;
    uint32_t length = 0u;

    if (!stage_saturn_read_word5(read_map, RPL_DSKTOP, &dsktop) ||
        dsktop > RPL_MAIN_END - 5u ||
        !stage_saturn_read_word5(read_map, dsktop, &object) ||
        object > RPL_MAIN_END - 24u ||
        !stage_saturn_read_word5(read_map, object, &prologue) ||
        prologue != RPL_DOCSTR ||
        !stage_saturn_read_word5(read_map, object + 5u, &length) ||
        length < 19u)
        return 0;

    for (uint32_t i = 0u;
         i < (uint32_t)(sizeof(launcher_magic) / sizeof(launcher_magic[0]));
         ++i) {
        uint8_t byte = 0u;
        if (!stage_saturn_read_byte(
                read_map, object + 10u + i * 2u, &byte) ||
            byte != launcher_magic[i])
            return 0;
    }

    /*
     * HPGCC 1.1's sat_stack_init() pseudo-drops this launcher cell without
     * changing AVMEM. Preserve that exact reservation: it is needed again when
     * the launcher object is restored after HPC1 returns.
     */
    if (!stage_saturn_write_word5(read_map, RPL_DSKTOP, dsktop + 5u))
        return -1;

    bias->object = object;
    bias->active = 1u;
    return 1;
}

static int stage_restore_launcher_bias(
    const void *saturn_state,
    const struct stage_launcher_bias *bias)
{
    if (bias == NULL || bias->active == 0u)
        return 1;
    if (saturn_state == NULL)
        return 0;

    const uint32_t *read_map = (const uint32_t *)saturn_state;
    uint32_t dsktop = 0u;
    if (!stage_saturn_read_word5(read_map, RPL_DSKTOP, &dsktop) ||
        dsktop < RPL_MAIN_FIRST + 5u)
        return 0;

    uint32_t slot = dsktop - 5u;
    if (!stage_saturn_write_word5(read_map, slot, bias->object))
        return 0;
    return stage_saturn_write_word5(read_map, RPL_DSKTOP, slot);
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

    /*
     * Publish structural fences even if no growth is currently possible.
     * drop/pop remain safe; push/replace report ENOSPC from their exact plan.
     */
    if (temp_limit < rsktop || user_limit > dsktop)
        return 0;

    bounds->magic = HPCLANG_RPL_BOUNDS_MAGIC;
    bounds->temp_limit = temp_limit;
    bounds->user_limit = user_limit;
    bounds->stack_reserve_nibbles = 0u;
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
    uint32_t model,
    uint32_t saturn_pc,
    uintptr_t emulator_lr,
    uint32_t stock_cpsr,
    uintptr_t stock_frame)
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
    struct hpclang_rpl_memory_bounds rpl_bounds = { 0u, 0u, 0u, 0u };
    struct stage_launcher_bias launcher_bias = { 0u, 0u };

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

    /*
     * Our Launcher-V3 wrapper is a DOCSTR whose payload starts with
     * "L\xB3\x80A>CP". Historical HPGCC 1.1 launchers leave that object as
     * a level-1 bias; newer launcher paths do not necessarily do so. Detect
     * the actual object instead of hard-coding launcher version/model policy.
     */
    if (launch.saturn_state != 0u) {
        int bias_status =
            stage_hide_launcher_bias(saturn_state, &launcher_bias);
        if (bias_status < 0)
            return -5;
    }
    if (launch.mmu.code_window_page_count != 0u) {
        hp39_portal_configure(
            &launch,
            saturn_state,
            model,
            saturn_pc,
            emulator_lr,
            stock_cpsr,
            stock_frame);
        launch.platform_cookie =
            (uintptr_t)&hpclang_hp39_portal_context.public_context |
            HPCLANG_PLATFORM_COOKIE_HP39_TAG;
    } else if (
        launch.saturn_state != 0u &&
        (model == HPCLANG_MODEL_HP48GII ||
         model == HPCLANG_MODEL_HP49GPLUS ||
         model == HPCLANG_MODEL_HP50G) &&
        stage_rpl_memory_bounds(
            saturn_state, old_l2, total_pages, &rpl_bounds)) {
        /*
         * A hidden HPGCC-1.1-style Launcher-V3 pointer needs one physical
         * stack cell when stage1 restores it after HPC1 returns. init/exit do
         * not expose that reservation through AVMEM, so carry it explicitly.
         */
        rpl_bounds.stack_reserve_nibbles =
            launcher_bias.active != 0u ? 5u : 0u;

        /*
         * rpl_bounds lives in this stage1 stack frame, whose ARMToolbox window
         * remains mapped for the complete hpclang_bootstrap_run() call.
         */
        launch.platform_cookie =
            (uintptr_t)&rpl_bounds | HPCLANG_PLATFORM_COOKIE_RPL_BOUNDS_TAG;
    } else {
        /* No mutation fence: read-only Saturn services may still be available. */
        launch.platform_cookie = old_l2_phys;
    }

    int result = hpclang_bootstrap_run(&launch);

    if (!stage_restore_launcher_bias(saturn_state, &launcher_bias))
        return -6;

    return result;
}
