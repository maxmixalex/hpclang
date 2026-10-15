#include <stdint.h>
#include <stddef.h>

#include <hpclang/abi.h>
#include <hpclang/bootstrap.h>
#include <hpclang/target.h>

#include "hp39-portal-private.h"
#include "hp39-portal-stack.h"

#include "stage-layout.h"
#include "port-memory.h"
#include "stage-image.h"

#define STAGE_WINDOW_BASE HPCLANG_STAGE_WINDOW_BASE
#define STAGE_CODE_OFFSET 0x00000400u
#define STAGE_STACK_PAGES 1u

#define ARMSYS_PAGE_COUNT 0x914u
#define ARMSYS_L2_PHYS 0x920u
#define ARMSYS_C_LO 0x91cu
#define COARSE_L1_FLAGS 0x0011u

#define RPL_RSKTOP 0x000806f3u
#define RPL_DSKTOP 0x000806f8u
#define RPL_EDITLINE 0x000806fdu
#define RPL_INTRPPTR 0x0008076bu
#define RPL_AVMEM 0x00080e9bu
#define RPL_DOCSTR 0x00002a2cu
#define RPL_MAIN_FIRST 0x00080000u
#define RPL_MAIN_END 0x00100000u
#define RPL_PHYS_PAGE_NIBBLES 0x00002000u

#define HP39_MAIN_FIRST_NIBBLE 0x0007c651u
#define HP39_MAIN_LAST_NIBBLE (HP39_MAIN_FIRST_NIBBLE + 255u * 16u + 15u)
#define HP39_MINI_FIRST_NIBBLE 0x0007e695u
#define HP39_MINI_LAST_NIBBLE (HP39_MINI_FIRST_NIBBLE + 255u * 11u + 10u)
#define HP4950_MINI_FIRST_NIBBLE 0x000812c8u
#define HP4950_MINI_LAST_NIBBLE (HP4950_MINI_FIRST_NIBBLE + 7u + 255u * 6u + 5u)
#define HP4950_BIG_OBJECT_NIBBLE 0x00084d82u
/* get_bigfont() skips either 8 bytes or 9 + name_length bytes before the
   256-record, 16-nibble raster. Require the worst-case name length so every
   Read[] page the runtime may dereference is present before publishing the
   Saturn state. */
#define HP4950_BIG_MAX_NAME_BYTES 255u
#define HP4950_BIG_LAST_NIBBLE                                                                     \
    (HP4950_BIG_OBJECT_NIBBLE + 2u * (9u + HP4950_BIG_MAX_NAME_BYTES) + 255u * 16u + 15u)

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

/* HP39g+ D1.55 full-NOR: main/EditLine/mini occupy these two pages. */
static const uint32_t hp39gplus_font_nor_pages[] = {0x0006e000u, 0x0006f000u};

_Static_assert(sizeof(hp39_font_nor_pages) / sizeof(hp39_font_nor_pages[0]) <=
                   HPCLANG_CODE_WINDOW_PAGE_CAPACITY,
               "HP39 native font window must fit before HPCLANG_CODE_BASE");
_Static_assert(sizeof(hp39gplus_font_nor_pages) / sizeof(hp39gplus_font_nor_pages[0]) <=
                   HPCLANG_CODE_WINDOW_PAGE_CAPACITY,
               "HP39g+ native font window must fit before HPCLANG_CODE_BASE");

void hpclang_cache_clean(const void *start, uint32_t size);
void hpclang_mmu_sync(volatile uint32_t *l2_workspace, volatile uint32_t *l1);

uintptr_t hpclang_stage0_resume_target __attribute__((section(".data.stage0.resume"))) = 0u;

struct hpclang_hp39_portal_context hpclang_hp39_portal_context
    __attribute__((section(".data.hp39.portal"), aligned(8))) = {
        .public_context =
            {
                .magic = HPCLANG_HP39_PLATFORM_MAGIC,
                .font_window_base = 0u,
                .portal_entry = 0u,
            },
};

/*
 * Keep the evaluator portal in one bootstrap-private 4 KiB page. The ARM stage
 * rewrites only this page, cleans it, and executes/accesses it only through
 * Stock's Saturn Read[] alias. Read requests use the low part of the page.
 * M7.3f additionally uses the remaining space as bounded staging for one owned
 * object while Stock executes the genuine extended-ID store protocol.
 *
 * The synthetic program deliberately has no application-visible RPL effect:
 *
 *   machine: GETPTR; A=colon; D0=return_stream; PC=(A)
 *   colon:   DOCOL arg op pack normalize finalize SEMI
 *   return:  -> DOCODE
 *   DOCODE:  SAVPTR; restore original INTRPPTR; restore C.W; ARMSYS
 *
 * Five threaded cells are patched before each trip while HPC mappings are
 * still active:
 *   Aplet catalog   = NOP ; GetApDirList ; NOP  ; NOP ; NOP
 *   Program catalog = NOP ; CatPgm       ; {}N  ; NOP ; NOP
 *   Note catalog    = NOP ; CatNot       ; {}N  ; NOP ; NOP
 *   Program body    = PTR(local ID) ; body-getter ; ?SKIP ; PTR(miss DOCSTR) ; NOP
 *   Note body       = PTR(local ID) ; body-getter ; ?SKIP ; PTR(miss DOCSTR) ; NOP
 *   Variable value  = NOP ; PTR(variable-recall helper) ; NOP ; NOP ; NOP
 *   Variable store  = NOP ; PTR(variable-store helper) ; NOP ; NOP ; NOP
 * Catalog requests return one DOLIST. Body requests return one DOCSTR; an
 * absent name returns a private thunk-local sentinel whose Saturn address is
 * distinguishable from a real, possibly empty, Stock body. The result remains
 * on the private RPL data stack
 * until ARM validates the result and drops its private cells relative to the
 * current EDITLINE, allowing Stock to relocate the stack during writes.
 */
static uint8_t hp39_saturn_portal_thunk[HPCLANG_PAGE_SIZE]
    __attribute__((section(".bss.hp39.thunk"), aligned(HPCLANG_PAGE_SIZE)));

enum
{
    HP39P_MACHINE_NIBBLE = 0u,
    HP39P_COLON_NIBBLE = 28u,
    HP39P_ARG_NIBBLE = 43u,
    HP39P_OPERATION_NIBBLE = 48u,
    HP39P_PACK_NIBBLE = 53u,
    HP39P_NORMALIZE_NIBBLE = 58u,
    HP39P_FINALIZE_NIBBLE = 63u,
    HP39P_RETURN_NIBBLE = 93u,
    HP39P_DOCODE_NIBBLE = 98u,
    HP39P_C_TEMPLATE_NIBBLE = 153u,
    HP39P_SAVED_INTRPPTR_NIBBLE = 169u,
    HP39P_EXEC_NIBBLES = 174u,
    HP39P_NAME_NIBBLE = 174u,
    HP39P_NAME_MAX_BYTES = 255u,
    HP39P_BODY_NAME_MAX_BYTES = 19u,
    HP39P_NAME_MAX_NIBBLES = 7u + HP39P_NAME_MAX_BYTES * 2u,
    HP39P_MISS_NIBBLE = 710u,
    HP39P_MISS_NIBBLES = 10u,
    HP39P_APLETVARS_NIBBLE = 720u,
    HP39P_APLETVARS_NIBBLES = 50u,
    HP39P_REFERENCE_LIST_NIBBLE = 770u,
    HP39P_REFERENCE_LIST_NIBBLES = 15u,
    HP39P_VARIABLE_RECALL_NIBBLE = 785u,
    HP39P_VARIABLE_RECALL_NIBBLES = 35u,
    HP39P_VARIABLE_STORE_NIBBLE = 820u,
    HP39P_VARIABLE_STORE_NIBBLES = 240u,
    HP39P_VARIABLE_OBJECT_NIBBLE = HP39P_VARIABLE_STORE_NIBBLE + HP39P_VARIABLE_STORE_NIBBLES + 10u,
    HP39P_STATIC_NIBBLES = HP39P_VARIABLE_OBJECT_NIBBLE + HP39P_STAGED_OBJECT_MAX_NIBBLES + 10u,
    HP39P_DOCODE_MACHINE_NIBBLES = 45u,
    HP39P_DOCODE_SIZE_NIBBLES = 5u + HP39P_DOCODE_MACHINE_NIBBLES
};

_Static_assert(HP39P_NAME_NIBBLE + HP39P_NAME_MAX_NIBBLES + 10u <= HP39P_MISS_NIBBLE &&
                   HP39P_MISS_NIBBLE + HP39P_MISS_NIBBLES <= HP39P_APLETVARS_NIBBLE &&
                   HP39P_APLETVARS_NIBBLE + HP39P_APLETVARS_NIBBLES <=
                       HP39P_REFERENCE_LIST_NIBBLE &&
                   HP39P_REFERENCE_LIST_NIBBLE + HP39P_REFERENCE_LIST_NIBBLES <=
                       HP39P_VARIABLE_RECALL_NIBBLE &&
                   HP39P_VARIABLE_RECALL_NIBBLE + HP39P_VARIABLE_RECALL_NIBBLES <=
                       HP39P_VARIABLE_STORE_NIBBLE &&
                   HP39P_VARIABLE_STORE_NIBBLE + HP39P_VARIABLE_STORE_NIBBLES <=
                       HP39P_VARIABLE_OBJECT_NIBBLE &&
                   HP39P_STATIC_NIBBLES <= sizeof(hp39_saturn_portal_thunk) * 2u,
               "HP39 Saturn portal helpers/staging must fit its private page");
_Static_assert(sizeof(hp39_saturn_portal_thunk) == HPCLANG_PAGE_SIZE,
               "HP39 portal must occupy exactly one private 4 KiB page");

struct hp39_saturn_builder
{
    uint8_t *bytes;
    uint32_t capacity_nibbles;
    uint32_t nibbles;
};

static int hp39_saturn_put_nibble(struct hp39_saturn_builder *builder, uint8_t nibble)
{
    if (builder == NULL || nibble > 0x0fu || builder->nibbles >= builder->capacity_nibbles)
        return 0;

    uint32_t offset = builder->nibbles++;
    if ((offset & 1u) != 0u) {
        builder->bytes[offset >> 1] |= (uint8_t)(nibble << 4);
    } else {
        builder->bytes[offset >> 1] = nibble;
    }
    return 1;
}

static int hp39_saturn_put_word5(struct hp39_saturn_builder *builder, uint32_t value)
{
    if (value >= UINT32_C(0x00100000))
        return 0;
    for (uint32_t i = 0u; i < 5u; ++i) {
        if (!hp39_saturn_put_nibble(builder, (uint8_t)((value >> (i * 4u)) & 0x0fu)))
            return 0;
    }
    return 1;
}

static int hp39_saturn_put_u32(struct hp39_saturn_builder *builder, uint32_t value)
{
    for (uint32_t i = 0u; i < 8u; ++i) {
        if (!hp39_saturn_put_nibble(builder, (uint8_t)((value >> (i * 4u)) & 0x0fu)))
            return 0;
    }
    return 1;
}

static int hp39_saturn_put_sequence(struct hp39_saturn_builder *builder, const uint8_t *nibbles,
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

static uint8_t hp39_saturn_buffer_nibble(const uint8_t *bytes, uint32_t nibble)
{
    uint8_t packed = bytes[nibble >> 1];
    return (nibble & 1u) != 0u ? (uint8_t)(packed >> 4) : (uint8_t)(packed & 0x0fu);
}

static int hp39_saturn_buffer_store_nibble(uint8_t *bytes, uint32_t capacity_nibbles,
                                           uint32_t offset, uint8_t value)
{
    if (bytes == NULL || value > 0x0fu || offset >= capacity_nibbles)
        return 0;

    uint8_t *packed = &bytes[offset >> 1];
    if ((offset & 1u) != 0u)
        *packed = (uint8_t)((*packed & 0x0fu) | (uint8_t)(value << 4));
    else
        *packed = (uint8_t)((*packed & 0xf0u) | value);
    return 1;
}

static int hp39_saturn_buffer_store_word5(uint8_t *bytes, uint32_t capacity_nibbles,
                                          uint32_t offset, uint32_t value)
{
    if (bytes == NULL || value >= UINT32_C(0x00100000) || offset > capacity_nibbles ||
        capacity_nibbles - offset < 5u)
        return 0;

    for (uint32_t i = 0u; i < 5u; ++i) {
        uint32_t nibble = offset + i;
        uint8_t value_nibble = (uint8_t)((value >> (i * 4u)) & 0x0fu);
        uint8_t *packed = &bytes[nibble >> 1];
        if ((nibble & 1u) != 0u)
            *packed = (uint8_t)((*packed & 0x0fu) | (value_nibble << 4));
        else
            *packed = (uint8_t)((*packed & 0xf0u) | value_nibble);
    }
    return 1;
}

static int hp39_request_words(uint32_t kind, uint32_t name_length, uint32_t saturn_base,
                              uint32_t *arg, uint32_t *operation, uint32_t *pack,
                              uint32_t *normalize, uint32_t *finalize, uint32_t *expected_prologue)
{
    if (arg == NULL || operation == NULL || pack == NULL || normalize == NULL || finalize == NULL ||
        expected_prologue == NULL)
        return 0;

    const uint32_t nop = UINT32_C(0x0000765c);
    *finalize = nop;

    if (kind == HP39P_CATALOG_APLET || kind == HP39P_CATALOG_PROGRAM ||
        kind == HP39P_CATALOG_NOTE) {
        if (name_length != 0u)
            return 0;
        *arg = nop;
        *pack = kind == HP39P_CATALOG_APLET ? nop : UINT32_C(0x00005c12); /* {}N */
        *normalize = nop;
        *expected_prologue = UINT32_C(0x00002a74); /* DOLIST */
        if (kind == HP39P_CATALOG_APLET)
            *operation = UINT32_C(0x0003f5ab); /* GetApDirList */
        else if (kind == HP39P_CATALOG_PROGRAM)
            *operation = UINT32_C(0x000281c7); /* CatPgm */
        else
            *operation = UINT32_C(0x00028154); /* CatNot */
        return 1;
    }

    if (kind == HP39P_VARIABLE_SCHEMA && name_length == 0u) {
        *arg = nop;
        *operation = saturn_base + HP39P_APLETVARS_NIBBLE;
        *pack = nop;
        *normalize = nop;
        *expected_prologue = UINT32_C(0x00002a74); /* DOLIST */
        return 1;
    }

    if (kind == HP39P_REFERENCE_EVAL && name_length == 3u) {
        /*
         * Put the referenced object on the Stock data stack first, then ask
         * Stock EVAL to execute it. This preserves the current-object context
         * required by the #11111 -> #3FD06 supported-entry dispatcher.
         * Executing a #3Fxxx value directly as a hand-built threaded cell does
         * not provide that contract.
         */
        *arg = saturn_base + HP39P_REFERENCE_LIST_NIBBLE;
        *operation = UINT32_C(0x00004053); /* ONE */
        *pack = UINT32_C(0x0000f476);      /* NTHCOMPDROP */
        *normalize = UINT32_C(0x0000775c); /* EVAL */
        *expected_prologue = 0u;
        return 1;
    }

    if (kind == HP39P_VARIABLE_VALUE && name_length == 3u) {
        /*
         * The private payload is the genuine Stock extended-ID reference.
         * The thunk-local helper extracts that data object and calls Stock @.
         * HP39's @ DOIDNT path is the native generic special-variable
         * dispatcher: it parses the ID's descriptor and invokes descriptor
         * word 0 with the supported-entry context Stock expects.
         */
        *arg = nop;
        *operation = saturn_base + HP39P_VARIABLE_RECALL_NIBBLE;
        *pack = nop;
        *normalize = nop;
        *expected_prologue = 0u;
        return 1;
    }

    if (kind == HP39P_COPY_OBJECT && name_length == sizeof(struct hpclang_hp39_object_request)) {
        *arg = saturn_base + HP39P_REFERENCE_LIST_NIBBLE;
        *operation = UINT32_C(0x00004053); /* ONE */
        *pack = UINT32_C(0x0000f476);      /* NTHCOMPDROP */
        *normalize = UINT32_C(0x00006e2f); /* TOTEMPOB: independent Stock-owned copy */
        *expected_prologue = 0u;
        return 1;
    }

    if ((kind == HP39P_VARIABLE_STORE &&
         name_length == sizeof(struct hpclang_hp39_variable_store_request)) ||
        (kind >= HP39P_PROGRAM_PUT && kind <= HP39P_NOTE_REMOVE &&
         name_length == sizeof(struct hpclang_hp39_catalog_request)) ||
        (kind >= HP39P_APLET_CLONE && kind <= HP39P_APLET_IS_DEFAULT &&
         name_length == sizeof(struct hpclang_hp39_aplet_request))) {
        *arg = nop;
        *operation = saturn_base + HP39P_VARIABLE_STORE_NIBBLE;
        *pack = nop;
        *normalize = nop;
        *expected_prologue = 0u;
        return 1;
    }

    if ((kind == HP39P_BODY_PROGRAM || kind == HP39P_BODY_NOTE) && name_length != 0u &&
        name_length <= HP39P_BODY_NAME_MAX_BYTES) {
        *arg = saturn_base + HP39P_NAME_NIBBLE;
        *operation = kind == HP39P_BODY_PROGRAM ? UINT32_C(0x00027f6d) : UINT32_C(0x000280f0);
        *pack = UINT32_C(0x00007903); /* ?SKIP */
        *normalize = saturn_base + HP39P_MISS_NIBBLE;
        *expected_prologue = UINT32_C(0x00002a2c); /* DOCSTR */
        return 1;
    }

    return 0;
}

static int hp39_write_operation_helper(const uint32_t *words, size_t count)
{
    if (count < 2u || count * 5u > HP39P_VARIABLE_STORE_NIBBLES || words[count - 1u] != 0x312bu)
        return 0;
    for (uint32_t i = 0u; i < HP39P_VARIABLE_STORE_NIBBLES / 5u; ++i) {
        uint32_t word = i < count - 1u ? words[i] : 0x765cu;
        if (i == HP39P_VARIABLE_STORE_NIBBLES / 5u - 1u)
            word = 0x312bu;
        if (!hp39_saturn_buffer_store_word5(hp39_saturn_portal_thunk,
                                            sizeof(hp39_saturn_portal_thunk) * 2u,
                                            HP39P_VARIABLE_STORE_NIBBLE + i * 5u, word))
            return 0;
    }
    return 1;
}

static int hp39_write_variable_store(uint32_t object, uint32_t predicate, uint32_t id)
{
    const uint32_t words[] = {
        0x2d9du,  0x7665u, object, predicate, 0x3af2u, 0xd585u,
        0x1320cu, 0x7665u, id,     0x8644u,   0x4049u, 0x312bu,
    };
    return hp39_write_operation_helper(words, sizeof(words) / sizeof(words[0]));
}

static int hp39_write_name_object(uint32_t offset, const uint8_t *name, uint32_t length,
                                  uint32_t prologue)
{
    uint32_t maximum = prologue == 0x2e48u ? HP39P_BODY_NAME_MAX_BYTES : 254u;
    if (name == NULL || length == 0u || length > maximum)
        return 0;
    struct hp39_saturn_builder builder = {
        .bytes = hp39_saturn_portal_thunk,
        .capacity_nibbles = sizeof(hp39_saturn_portal_thunk) * 2u,
        .nibbles = offset,
    };
    if (!hp39_saturn_put_word5(&builder, prologue))
        return 0;
    if (prologue == 0x2e48u) {
        if (!hp39_saturn_put_nibble(&builder, length & 15u) ||
            !hp39_saturn_put_nibble(&builder, length >> 4u))
            return 0;
    } else if (!hp39_saturn_put_word5(&builder, 5u + length * 2u)) {
        return 0;
    }
    for (uint32_t i = 0u; i < length; ++i) {
        if (!hp39_saturn_put_nibble(&builder, name[i] & 15u) ||
            !hp39_saturn_put_nibble(&builder, name[i] >> 4u))
            return 0;
    }
    return 1;
}

static int hp39_stage_object(uintptr_t object_data, uint32_t object_nibbles)
{
    if (object_data == 0u || object_nibbles < 5u ||
        object_nibbles > HP39P_STAGED_OBJECT_MAX_NIBBLES)
        return 0;
    const uint8_t *source = (const uint8_t *)object_data;
    for (uint32_t i = 0u; i < object_nibbles; ++i) {
        uint8_t packed = source[i >> 1];
        uint8_t nibble = (i & 1u) != 0u ? packed >> 4u : packed & 15u;
        if (!hp39_saturn_buffer_store_nibble(hp39_saturn_portal_thunk,
                                             sizeof(hp39_saturn_portal_thunk) * 2u,
                                             HP39P_VARIABLE_OBJECT_NIBBLE + i, nibble))
            return 0;
    }
    return 1;
}

static int hp39_store_request_name(uint32_t kind, uint32_t name_length, const uint8_t *name,
                                   uint32_t saturn_base)
{
    if (kind >= HP39P_APLET_CLONE && kind <= HP39P_APLET_IS_DEFAULT) {
        if (name == NULL || name_length != sizeof(struct hpclang_hp39_aplet_request))
            return 0;
        const struct hpclang_hp39_aplet_request *request = (const void *)name;
        if (!hp39_write_name_object(HP39P_NAME_NIBBLE, request->name, request->name_length,
                                    0x2a2cu))
            return 0;
        uint32_t source = saturn_base + HP39P_NAME_NIBBLE;
        if (kind == HP39P_APLET_CLONE) {
            if (!hp39_write_name_object(HP39P_VARIABLE_OBJECT_NIBBLE, request->destination,
                                        request->destination_length, 0x2a2cu))
                return 0;
            const uint32_t words[] = {
                0x2d9du,
                /* Stock save selects its copy. Keep the caller's contexts in
                 *
                   GC-traced native bindings and restore on success or error. */
                0x9660u,
                0x3f063u, /* CONTEXT@ dvbind */
                0x126c6u,
                0x3f063u, /* PGMCXT@ dvbind */
                0x126e6u,
                0x3f063u, /* NOTESCXT@ dvbind */
                0x12706u,
                0x3f063u, /* apletPTR@ dvbind */
                0x565cu,
                0x2d9du, /* ERRSET :: */
                source,
                0x7665u,
                0x3f66eu,
                0x775cu, /* RclAplet?Err */
                saturn_base + HP39P_VARIABLE_OBJECT_NIBBLE,
                0x7665u,
                0x3f3cbu,
                0x775cu,
                0x312bu,
                0x56b6u,
                0x2d9du, /* ; ERRTRAP :: */
                0xfdaeu,
                0x126f6u, /* 1GETABND apletPTR! */
                0xfdaeu,
                0x126d6u, /* 1GETABND NOTESCXT! */
                0xfdaeu,
                0x126b6u, /* 1GETABND PGMCXT! */
                0xfdaeu,
                0x960eu, /* 1GETABND CONTEXT! */
                0x56cfu,
                0x312bu, /* ERRJMP ; */
                0xfdaeu,
                0x126f6u,
                0xfdaeu,
                0x126d6u,
                0xfdaeu,
                0x126b6u,
                0xfdaeu,
                0x960eu,
                0x4049u,
                0x312bu, /* StoAplet */
            };
            return hp39_write_operation_helper(words, sizeof(words) / sizeof(words[0]));
        }
        if (kind == HP39P_APLET_REMOVE) {
            static const uint8_t cache_name[] = "'ApDirList";
            if (!hp39_write_name_object(HP39P_VARIABLE_OBJECT_NIBBLE, cache_name,
                                        sizeof(cache_name) - 1u, 0x2e48u))
                return 0;
            /* PurgeAplet removes storage only. Mirror the library's deletion
             * path:
             * remove the display name and replace Hidden's ApDirList. */
            const uint32_t words[] = {
                0x2d9du, source,
                0x7665u, 0x3f66du,
                0x775cu, source,
                0x7665u, 0x3f5abu,
                0x775cu,           /* name GetApDirList */
                0xf892u, 0x108dfu, /* DUPROT EQUALPOSCOMP */
                0x32c2u, 0x4053u,
                0xcfdeu, 0x3e0eu,
                0x5fdau, 0xcd84u,
                0x3defu, 0x11ea1u,
                0x5fdau, 0x59d3u,
                0xe7d2u, 0x2d9du, /* Stock Hidden-context guard :: */
                0x7665u, saturn_base + HP39P_VARIABLE_OBJECT_NIBBLE,
                0x3188u, 0x81d3u,
                0xd585u, 0x2d9du,
                0x3244u, 0x3188u,
                0x952du, 0x312bu, /* DROP DUP PURGE ; */
                0x8f9cu, 0x312bu, /* CREATE ; */
                0x4049u, 0x312bu,
            }; /* PurgeAplet */
            return hp39_write_operation_helper(words, sizeof(words) / sizeof(words[0]));
        }
        /* Match Stock's default-Aplet test: compare topic type property FIVE
         * with the
         * stored Aplet name. DoInCxt restores the caller's context. */
        const uint32_t words[] = {
            0x2d9du, source,  0x7665u,  0x3f66eu, 0x775cu, 0x3188u, 0x12a5cu, 0x26f7fu,
            0x5842u, 0x407bu, 0x347ccu, 0x3244u,  0x3223u, 0x8bf2u, 0x3b97u,  0x312bu,
        };
        return hp39_write_operation_helper(words, sizeof(words) / sizeof(words[0]));
    }
    if (kind >= HP39P_PROGRAM_PUT && kind <= HP39P_NOTE_REMOVE) {
        if (name == NULL || name_length != sizeof(struct hpclang_hp39_catalog_request))
            return 0;
        const struct hpclang_hp39_catalog_request *request = (const void *)name;
        const int put = kind == HP39P_PROGRAM_PUT || kind == HP39P_NOTE_PUT;
        if (!hp39_write_name_object(HP39P_NAME_NIBBLE, request->name, request->name_length,
                                    0x2e48u))
            return 0;
        if (put && !hp39_stage_object(request->object_data, request->object_nibbles))
            return 0;
        /* Native catalog operations maintain the source, compiled trampoline
         * and editor
         * records together. A plain STO/PURGE of the source does not. */
        const uint32_t operations[] = {0x27c09u, 0x27c68u, 0x28096u, 0x280b9u};
        const uint32_t words[] = {
            0x2d9du, /* DOCOL */
            put ? 0x7665u : 0x765cu,
            put ? saturn_base + HP39P_VARIABLE_OBJECT_NIBBLE : 0x765cu,
            0x7665u,
            saturn_base + HP39P_NAME_NIBBLE,
            operations[kind - HP39P_PROGRAM_PUT],
            0x4049u, /* ZERO: success */
            0x312bu,
        };
        return hp39_write_operation_helper(words, sizeof(words) / sizeof(words[0]));
    }
    const int body_request = kind == HP39P_BODY_PROGRAM || kind == HP39P_BODY_NOTE;
    const int variable_request = kind == HP39P_VARIABLE_VALUE;
    const int reference_request = kind == HP39P_REFERENCE_EVAL;
    const int store_request = kind == HP39P_VARIABLE_STORE;
    const int copy_request = kind == HP39P_COPY_OBJECT;
    if (!body_request && !variable_request && !reference_request && !store_request && !copy_request)
        return name_length == 0u;

    if (store_request || copy_request) {
        const size_t request_size = store_request
                                        ? sizeof(struct hpclang_hp39_variable_store_request)
                                        : sizeof(struct hpclang_hp39_object_request);
        if (name == NULL || name_length != request_size)
            return 0;
        const uint32_t object_address = saturn_base + HP39P_VARIABLE_OBJECT_NIBBLE;
        uintptr_t object_data;
        uint32_t object_nibbles;
        if (store_request) {
            const struct hpclang_hp39_variable_store_request *request = (const void *)name;
            if (request->id_reference == 0u || request->id_reference >= UINT32_C(0x00100000) ||
                request->type_predicate == 0u || request->type_predicate >= UINT32_C(0x00100000))
                return 0;
            object_data = request->object_data;
            object_nibbles = request->object_nibbles;
            if (!hp39_write_variable_store(object_address, request->type_predicate,
                                           request->id_reference))
                return 0;
        } else {
            const struct hpclang_hp39_object_request *request = (const void *)name;
            object_data = request->object_data;
            object_nibbles = request->object_nibbles;
            if (!hp39_saturn_buffer_store_word5(hp39_saturn_portal_thunk,
                                                sizeof(hp39_saturn_portal_thunk) * 2u,
                                                HP39P_REFERENCE_LIST_NIBBLE + 5u, object_address))
                return 0;
        }
        return hp39_stage_object(object_data, object_nibbles);
    }

    if (reference_request) {
        if (name == NULL || name_length != 3u)
            return 0;
        uint32_t reference =
            (uint32_t)name[0] | ((uint32_t)name[1] << 8u) | ((uint32_t)name[2] << 16u);
        if (reference == 0u || reference >= UINT32_C(0x00100000))
            return 0;
        uint32_t patch = HP39P_REFERENCE_LIST_NIBBLE + 5u;
        return hp39_saturn_buffer_store_word5(hp39_saturn_portal_thunk,
                                              (uint32_t)sizeof(hp39_saturn_portal_thunk) * 2u,
                                              patch, reference);
    }

    if (variable_request) {
        if (name == NULL || name_length != 3u)
            return 0;
        uint32_t id_reference =
            (uint32_t)name[0] | ((uint32_t)name[1] << 8u) | ((uint32_t)name[2] << 16u);
        if (id_reference == 0u || id_reference >= UINT32_C(0x00100000))
            return 0;

        /*
         * Quote the genuine extended ID directly in the helper runstream.
         * Stock's own DummyVarProc uses exactly this form (`' DummyVar`);
         * no temporary list/component extraction is needed before @.
         */
        return hp39_saturn_buffer_store_word5(hp39_saturn_portal_thunk,
                                              (uint32_t)sizeof(hp39_saturn_portal_thunk) * 2u,
                                              HP39P_VARIABLE_RECALL_NIBBLE + 10u, id_reference);
    }

    return hp39_write_name_object(HP39P_NAME_NIBBLE, name, name_length, 0x2e48u);
}

static int stage_saturn_read_nibble(const uint32_t *read_map, uint32_t address, uint8_t *value);
static int stage_saturn_read_word5(const uint32_t *read_map, uint32_t address, uint32_t *value);
static int stage_saturn_write_word5(const uint32_t *read_map, uint32_t address, uint32_t value);
static int stage_phys_page_saturn_start(const uint32_t *read_map, uint32_t phys_page,
                                        uint32_t *saturn_start);

static int hp39_portal_prepare_thunk(void *saturn_state, struct hpclang_hp39_portal_context *portal)
{
    /*
     * These machine encodings are fixed by the HP39 Saturn ISA and are also
     * present in the original HPGCC39 LIB275 build:
     *   8F C03F0  GOSBVL $jGETPTR (#0F30C)
     *   8F 37F60  GOSBVL SAVPTR (#06F73)
     *   1B xxxxx  D0=(5)
     *   132       AD0EX
     *   808C      PC=(A)
     *   146       C=DAT0 A
     *   144       DAT0=C A
     *   1567      C=DAT0 W
     *   80BFE     ARMSYS
     */
    static const uint8_t getptr[] = {0x8u, 0xfu, 0xcu, 0x0u, 0x3u, 0xfu, 0x0u};
    static const uint8_t savptr[] = {0x8u, 0xfu, 0x3u, 0x7u, 0xfu, 0x6u, 0x0u};
    static const uint8_t d0_imm[] = {0x1u, 0xbu};
    static const uint8_t ad0ex[] = {0x1u, 0x3u, 0x2u};
    static const uint8_t pc_indirect[] = {0x8u, 0x0u, 0x8u, 0xcu};
    static const uint8_t c_dat0_a[] = {0x1u, 0x4u, 0x6u};
    static const uint8_t dat0_c_a[] = {0x1u, 0x4u, 0x4u};
    static const uint8_t p_zero[] = {0x2u, 0x0u};
    static const uint8_t c_dat0_w[] = {0x1u, 0x5u, 0x6u, 0x7u};
    static const uint8_t armsys[] = {0x8u, 0x0u, 0xbu, 0xfu, 0xeu};

    if (saturn_state == NULL || portal == NULL)
        return 0;

    uintptr_t thunk_address = (uintptr_t)hp39_saturn_portal_thunk;
    uintptr_t stage_limit =
        (uintptr_t)STAGE_WINDOW_BASE + (uintptr_t)HPCLANG_L2_ENTRIES * HPCLANG_PAGE_SIZE;
    if (thunk_address < (uintptr_t)STAGE_WINDOW_BASE ||
        thunk_address > stage_limit - sizeof(hp39_saturn_portal_thunk))
        return 0;

    uintptr_t delta = thunk_address - (uintptr_t)STAGE_WINDOW_BASE;
    uint32_t page_index = (uint32_t)(delta / HPCLANG_PAGE_SIZE);
    uint32_t byte_offset = (uint32_t)(delta % HPCLANG_PAGE_SIZE);
    if (page_index >= HPCLANG_L2_ENTRIES ||
        byte_offset > HPCLANG_PAGE_SIZE - sizeof(hp39_saturn_portal_thunk))
        return 0;

    const volatile uint32_t *stage_l2 = (const volatile uint32_t *)(uintptr_t)STAGE_WINDOW_BASE;
    uint32_t saturn_page = 0u;
    if (!stage_phys_page_saturn_start((const uint32_t *)saturn_state,
                                      stage_l2[page_index] & UINT32_C(0xfffff000), &saturn_page))
        return 0;

    uint32_t saturn_base = saturn_page + byte_offset * 2u;
    if (saturn_base > RPL_MAIN_END - HP39P_STATIC_NIBBLES)
        return 0;

    uint32_t colon = saturn_base + HP39P_COLON_NIBBLE;
    uint32_t return_stream = saturn_base + HP39P_RETURN_NIBBLE;
    uint32_t docode = saturn_base + HP39P_DOCODE_NIBBLE;
    uint32_t c_template = saturn_base + HP39P_C_TEMPLATE_NIBBLE;
    uint32_t saved_intrpptr = saturn_base + HP39P_SAVED_INTRPPTR_NIBBLE;

    struct hp39_saturn_builder builder = {
        .bytes = hp39_saturn_portal_thunk,
        .capacity_nibbles = (uint32_t)sizeof(hp39_saturn_portal_thunk) * 2u,
        .nibbles = 0u,
    };

    /* Machine entry: restore interpreter state and enter synthetic DOCOL. */
    if (!hp39_saturn_put_sequence(&builder, getptr, (uint32_t)sizeof(getptr)) ||
        !hp39_saturn_put_sequence(&builder, d0_imm, (uint32_t)sizeof(d0_imm)) ||
        !hp39_saturn_put_word5(&builder, colon) ||
        !hp39_saturn_put_sequence(&builder, ad0ex, (uint32_t)sizeof(ad0ex)) ||
        !hp39_saturn_put_sequence(&builder, d0_imm, (uint32_t)sizeof(d0_imm)) ||
        !hp39_saturn_put_word5(&builder, return_stream) ||
        !hp39_saturn_put_sequence(&builder, pc_indirect, (uint32_t)sizeof(pc_indirect)) ||
        builder.nibbles != HP39P_COLON_NIBBLE)
        return 0;

    /* One native error boundary covers every operation, including allocation
     * during schema/catalog reads. Success returns value ZERO; failure returns
     * ERROR@ with any private operands left by Stock. */
    if (!hp39_saturn_put_word5(&builder, UINT32_C(0x00002d9d)) ||
        !hp39_saturn_put_word5(&builder, UINT32_C(0x0000565c)) || /* ERRSET */
        !hp39_saturn_put_word5(&builder, UINT32_C(0x00002d9d)) ||
        !hp39_saturn_put_word5(&builder, UINT32_C(0x0000765c)) ||
        !hp39_saturn_put_word5(&builder, UINT32_C(0x0003f5ab)) ||
        !hp39_saturn_put_word5(&builder, UINT32_C(0x0000765c)) ||
        !hp39_saturn_put_word5(&builder, UINT32_C(0x0000765c)) ||
        !hp39_saturn_put_word5(&builder, UINT32_C(0x0000765c)) ||
        !hp39_saturn_put_word5(&builder, UINT32_C(0x00004049)) || /* ZERO */
        !hp39_saturn_put_word5(&builder, UINT32_C(0x0000312b)) ||
        !hp39_saturn_put_word5(&builder, UINT32_C(0x000056b6)) || /* ERRTRAP */
        !hp39_saturn_put_word5(&builder, UINT32_C(0x000054f8)) || /* ERROR@ */
        !hp39_saturn_put_word5(&builder, UINT32_C(0x0000312b)) ||
        builder.nibbles != HP39P_RETURN_NIBBLE)
        return 0;

    /* SEMI returns here and executes the synthetic DOCODE object. */
    if (!hp39_saturn_put_word5(&builder, docode) || builder.nibbles != HP39P_DOCODE_NIBBLE)
        return 0;

    /*
     * Save the final live interpreter pointers through Stock's own SAVPTR.
     * This is the exact inverse of the GETPTR used at portal entry and avoids
     * duplicating its register-exchange contract in synthetic machine code.
     * SAVPTR also writes the current synthetic D0 to INTRPPTR, so restore the
     * original suspended Stock runstream from a thunk-private cell before
     * crossing back to ARM.
     */
    if (!hp39_saturn_put_word5(&builder, UINT32_C(0x00002dcc)) ||
        !hp39_saturn_put_word5(&builder, HP39P_DOCODE_SIZE_NIBBLES) ||
        !hp39_saturn_put_sequence(&builder, savptr, (uint32_t)sizeof(savptr)) ||
        !hp39_saturn_put_sequence(&builder, d0_imm, (uint32_t)sizeof(d0_imm)) ||
        !hp39_saturn_put_word5(&builder, saved_intrpptr) ||
        !hp39_saturn_put_sequence(&builder, c_dat0_a, (uint32_t)sizeof(c_dat0_a)) ||
        !hp39_saturn_put_sequence(&builder, d0_imm, (uint32_t)sizeof(d0_imm)) ||
        !hp39_saturn_put_word5(&builder, RPL_INTRPPTR) ||
        !hp39_saturn_put_sequence(&builder, dat0_c_a, (uint32_t)sizeof(dat0_c_a)) ||
        !hp39_saturn_put_sequence(&builder, d0_imm, (uint32_t)sizeof(d0_imm)) ||
        !hp39_saturn_put_word5(&builder, c_template) ||
        !hp39_saturn_put_sequence(&builder, p_zero, (uint32_t)sizeof(p_zero)) ||
        !hp39_saturn_put_sequence(&builder, c_dat0_w, (uint32_t)sizeof(c_dat0_w)) ||
        !hp39_saturn_put_sequence(&builder, armsys, (uint32_t)sizeof(armsys)) ||
        builder.nibbles != HP39P_C_TEMPLATE_NIBBLE)
        return 0;

    /*
     * ARMSYS consumes C.LO as the ARM entry and C.HI as the Stock ARM RAM
     * context. Keep the launcher's original high half, but target the portal
     * resume stub in the low half. ARMSYS_L2_PHYS and C.HI intentionally share
     * offset 0x920 in the historical HPGCC39 ARMSYS block.
     */
    uint32_t c_hi = *(const volatile uint32_t *)((const uint8_t *)saturn_state + ARMSYS_L2_PHYS);
    if (!hp39_saturn_put_u32(&builder, (uint32_t)(uintptr_t)&hpclang_hp39_portal_resume) ||
        !hp39_saturn_put_u32(&builder, c_hi) || !hp39_saturn_put_word5(&builder, 0u) ||
        builder.nibbles != HP39P_EXEC_NIBBLES ||
        !hp39_saturn_buffer_store_word5(hp39_saturn_portal_thunk,
                                        (uint32_t)sizeof(hp39_saturn_portal_thunk) * 2u,
                                        HP39P_MISS_NIBBLE, UINT32_C(0x00002a2c)) ||
        !hp39_saturn_buffer_store_word5(hp39_saturn_portal_thunk,
                                        (uint32_t)sizeof(hp39_saturn_portal_thunk) * 2u,
                                        HP39P_MISS_NIBBLE + 5u, 5u))
        return 0;

    /*
     * Current-Aplet variable schema helper:
     *   :: tTYPE@ CARCOMP EIGHT PTYPE>PINFO DROP
     *      DUPTYPELIST? ?SKIP EVAL ;
     *
     * HPGCC39 =Head39.h defines ApletVars#=EIGHT. Stock uses the same
     * tTYPE@ -> CARCOMP -> property-index -> PTYPE>PINFO protocol for
     * ApletReset#=NINE at bank-7 logical 0x73D1C. Topic properties may be
     * direct objects or executable/computed properties; Stock's AltViews
     * consumer uses this exact DUPTYPELIST?/?SKIP/EVAL normalization pattern.
     * ApletVars needs the same normalization so HPGCC/custom Aplets can return
     * their schema dynamically instead of being mistaken for a non-list.
     */
    static const uint32_t aplet_vars_words[] = {
        UINT32_C(0x00002d9d), /* DOCOL */
        UINT32_C(0x00026f7f), /* tTYPE@ */
        UINT32_C(0x00005842), /* CARCOMP */
        UINT32_C(0x00004099), /* EIGHT */
        UINT32_C(0x000347cc), /* PTYPE>PINFO */
        UINT32_C(0x00003244), /* DROP */
        UINT32_C(0x0000dce4), /* DUPTYPELIST? */
        UINT32_C(0x00007903), /* ?SKIP */
        UINT32_C(0x0000775c), /* EVAL */
        UINT32_C(0x0000312b), /* SEMI */
    };
    for (uint32_t i = 0u; i < (uint32_t)(sizeof(aplet_vars_words) / sizeof(aplet_vars_words[0]));
         ++i) {
        if (!hp39_saturn_buffer_store_word5(hp39_saturn_portal_thunk,
                                            (uint32_t)sizeof(hp39_saturn_portal_thunk) * 2u,
                                            HP39P_APLETVARS_NIBBLE + i * 5u, aplet_vars_words[i]))
            return 0;
    }

    /*
     * Supported-entry reference container used only by REFERENCE_EVAL.
     */
    if (!hp39_saturn_buffer_store_word5(
            hp39_saturn_portal_thunk, (uint32_t)sizeof(hp39_saturn_portal_thunk) * 2u,
            HP39P_REFERENCE_LIST_NIBBLE, UINT32_C(0x00002a74)) || /* DOLIST */
        !hp39_saturn_buffer_store_word5(
            hp39_saturn_portal_thunk, (uint32_t)sizeof(hp39_saturn_portal_thunk) * 2u,
            HP39P_REFERENCE_LIST_NIBBLE + 5u, UINT32_C(0x00004053)) || /* safe placeholder: ONE */
        !hp39_saturn_buffer_store_word5(
            hp39_saturn_portal_thunk, (uint32_t)sizeof(hp39_saturn_portal_thunk) * 2u,
            HP39P_REFERENCE_LIST_NIBBLE + 10u, UINT32_C(0x0000312b))) /* SEMI */
        return 0;

    /*
     * Generic HP39 extended-variable recall:
     *
     *   :: ' <genuine-id> @ ?SKIP <miss> ;
     *
     * Stock's own DummyVarProc proves the quote form for extended IDs. Stock
     * @ follows the trailing descriptor pointer and tail-dispatches word 0,
     * including the #11111 / #3Fxxx supported-entry path. Standard variable
     * getters consume the quoted ID and return value TRUE | FALSE; ?SKIP
     * collapses that to exactly one value or the private miss sentinel.
     */
    const uint32_t variable_recall_words[] = {
        UINT32_C(0x00002d9d), /* DOCOL */
        UINT32_C(0x00007665), /* ' */
        UINT32_C(0x00004053), /* patched: genuine ID; safe placeholder ONE */
        UINT32_C(0x000081d3), /* @ -> value TRUE | FALSE */
        UINT32_C(0x00007903), /* ?SKIP consumes recall flag */
        saturn_base + HP39P_MISS_NIBBLE,
        UINT32_C(0x0000312b), /* SEMI */
    };
    for (uint32_t i = 0u;
         i < (uint32_t)(sizeof(variable_recall_words) / sizeof(variable_recall_words[0])); ++i) {
        if (!hp39_saturn_buffer_store_word5(
                hp39_saturn_portal_thunk, (uint32_t)sizeof(hp39_saturn_portal_thunk) * 2u,
                HP39P_VARIABLE_RECALL_NIBBLE + i * 5u, variable_recall_words[i]))
            return 0;
    }

    /* The descriptor predicate preserves the value and supplies a type flag.
     * Reject false through Stock SETTYPEERR before STO dispatches the genuine ID.
     * The enclosing portal traps native rejections for reads and writes alike. */
    if (!hp39_write_variable_store(0x4053u, 0x4053u, 0x4053u))
        return 0;

    /*
     * Prove the complete generated stream through the Saturn alias before the
     * portal is published. This catches wrong physical-page inversion, odd
     * nibble packing and stale aliasing without executing guessed memory.
     */
    for (uint32_t i = 0u; i < HP39P_STATIC_NIBBLES; ++i) {
        uint8_t nibble = 0u;
        if (!stage_saturn_read_nibble((const uint32_t *)saturn_state, saturn_base + i, &nibble) ||
            nibble != hp39_saturn_buffer_nibble(hp39_saturn_portal_thunk, i))
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

static void hp39_portal_configure(const struct hpclang_launch_context *launch, void *saturn_state,
                                  uint32_t model, uint32_t saturn_pc, uintptr_t emulator_lr,
                                  uint32_t stock_cpsr, uintptr_t stock_frame)
{
    struct hpclang_hp39_portal_context *portal = &hpclang_hp39_portal_context;

    portal->public_context.magic = HPCLANG_HP39_PLATFORM_MAGIC;
    portal->public_context.font_window_base = (uintptr_t)HPCLANG_CODE_WINDOW_BASE;
    portal->public_context.portal_entry = 0u;
    portal->saturn_state = (uintptr_t)saturn_state;
    portal->emulator_lr = emulator_lr;
    portal->saturn_entry = 0u;
    portal->armsys_target = 0u;
    portal->stock_cpsr = stock_cpsr;
    portal->l1 = (uintptr_t)launch->mmu.l1;
    portal->l2_workspace = (uintptr_t)launch->mmu.l2_workspace;
    portal->stock_data_l1 = launch->mmu.l1[HPCLANG_L1_DATA_INDEX];
    portal->stock_code_l1 = launch->mmu.l1[HPCLANG_L1_CODE_INDEX];
    portal->hpc_data_l1 = launch->mmu.l2_workspace_phys | COARSE_L1_FLAGS;
    portal->hpc_code_l1 = (launch->mmu.l2_workspace_phys + HPCLANG_L2_BYTES) | COARSE_L1_FLAGS;
    portal->stock_frame = stock_frame;
    portal->stock_sp = stock_frame + 14u * sizeof(uint32_t);
    portal->active = 0u;
    portal->trips = 0u;
    portal->last_cpsr = 0u;
    portal->pre_stack_span = 0u;
    portal->pinned_cells = 0u;
    portal->incoming_stack_span = 0u;
    /* Stage1 has no libc; volatile stores prevent a compiler memset call. */
    for (uint32_t i = 0u; i < HP39P_STACK_GUARD_SIZE; ++i)
        ((volatile uint8_t *)portal->portal_stack)[i] = 0xa5u;
    for (uint32_t i = 0u; i < 4u; ++i)
        portal->native_call[i] = load32(saturn_state, ARMSYS_PAGE_COUNT + 4u * i);
    portal->result_object = 0u;
    portal->result_prologue = 0u;
    portal->request_kind = 0u;
    portal->request_name = 0u;
    portal->request_name_length = 0u;
    portal->expected_prologue = UINT32_C(0x00002a74);

    if (stock_frame != 0u) {
        const volatile uint32_t *saved = (const volatile uint32_t *)stock_frame;
        for (uint32_t i = 0u; i < 14u; ++i)
            portal->stock_regs[i] = saved[i];
        for (uint32_t i = 0u; i < 8u; ++i)
            portal->emulator_regs[i] = saved[i + 4u];
    }

    if ((model == HPCLANG_MODEL_HP39GPLUS || model == HPCLANG_MODEL_HP39GS ||
         model == HPCLANG_MODEL_HP40GS) &&
        launch->saturn_state != 0u && saturn_pc >= 5u && emulator_lr != 0u && stock_frame != 0u) {
        portal->armsys_target = load32(saturn_state, ARMSYS_C_LO);
        if (hp39_portal_prepare_thunk(saturn_state, portal)) {
            portal->public_context.portal_entry = (uintptr_t)&hpclang_hp39_portal_enter;
        }
    }
}

static int hp39_portal_stack_intact(const struct hpclang_hp39_portal_context *portal)
{
    for (uint32_t i = 0u; i < HP39P_STACK_GUARD_SIZE; ++i) {
        if (portal->portal_stack[i] != 0xa5u)
            return 0;
    }
    return 1;
}

/* Stock GC finds a TEMP block by repeatedly skipping objects from each live
 * pointer. Embedded
 * RPL streams must therefore lead to the enclosing block's
 * end, even though the native arena
 * itself is an opaque string. A DOCODE tail
 * provides that path without exposing ARM instructions
 * as RPL objects. */
static int hp39_portal_gc_tail(const struct hpclang_hp39_portal_context *portal, uint32_t offset)
{
    uint32_t address = portal->saturn_entry + offset;
    if (portal->arena_object_end < address + 10u)
        return 0;
    return hp39_saturn_buffer_store_word5(hp39_saturn_portal_thunk,
                                          (uint32_t)sizeof(hp39_saturn_portal_thunk) * 2u, offset,
                                          0x02dccu) &&
           hp39_saturn_buffer_store_word5(hp39_saturn_portal_thunk,
                                          (uint32_t)sizeof(hp39_saturn_portal_thunk) * 2u,
                                          offset + 5u, portal->arena_object_end - address - 5u);
}

int hpclang_hp39_portal_prepare_stock(void)
{
    struct hpclang_hp39_portal_context *portal = &hpclang_hp39_portal_context;
    volatile uint32_t *l1 = (volatile uint32_t *)portal->l1;
    const uint32_t *read_map = (const uint32_t *)portal->saturn_state;

    uint32_t dsktop = 0u;
    uint32_t intrpptr = 0u;
    uint32_t editline = 0u;
    if (!stage_saturn_read_word5(read_map, RPL_EDITLINE, &editline) ||
        !stage_saturn_read_word5(read_map, RPL_DSKTOP, &dsktop) ||
        !stage_saturn_read_word5(read_map, RPL_INTRPPTR, &intrpptr) || dsktop < 5u ||
        dsktop > RPL_MAIN_END || editline < dsktop || (editline - dsktop) % 5u != 0u ||
        intrpptr >= RPL_MAIN_END) {
        portal->pre_stack_span = 0u;
        return -3;
    }

    portal->pre_stack_span = editline - dsktop;
    portal->result_object = 0u;
    portal->result_prologue = 0u;

    uint32_t arg = 0u;
    uint32_t operation = 0u;
    uint32_t pack = 0u;
    uint32_t normalize = 0u;
    uint32_t finalize = 0u;
    uint32_t expected_prologue = 0u;
    if (!hp39_request_words(portal->request_kind, portal->request_name_length, portal->saturn_entry,
                            &arg, &operation, &pack, &normalize, &finalize, &expected_prologue) ||
        !hp39_store_request_name(portal->request_kind, portal->request_name_length,
                                 (const uint8_t *)(uintptr_t)portal->request_name,
                                 portal->saturn_entry) ||
        !hp39_saturn_buffer_store_word5(hp39_saturn_portal_thunk,
                                        (uint32_t)sizeof(hp39_saturn_portal_thunk) * 2u,
                                        HP39P_ARG_NIBBLE, arg) ||
        !hp39_saturn_buffer_store_word5(hp39_saturn_portal_thunk,
                                        (uint32_t)sizeof(hp39_saturn_portal_thunk) * 2u,
                                        HP39P_OPERATION_NIBBLE, operation) ||
        !hp39_saturn_buffer_store_word5(hp39_saturn_portal_thunk,
                                        (uint32_t)sizeof(hp39_saturn_portal_thunk) * 2u,
                                        HP39P_PACK_NIBBLE, pack) ||
        !hp39_saturn_buffer_store_word5(hp39_saturn_portal_thunk,
                                        (uint32_t)sizeof(hp39_saturn_portal_thunk) * 2u,
                                        HP39P_NORMALIZE_NIBBLE, normalize) ||
        !hp39_saturn_buffer_store_word5(hp39_saturn_portal_thunk,
                                        (uint32_t)sizeof(hp39_saturn_portal_thunk) * 2u,
                                        HP39P_FINALIZE_NIBBLE, finalize) ||
        !hp39_saturn_buffer_store_word5(hp39_saturn_portal_thunk,
                                        (uint32_t)sizeof(hp39_saturn_portal_thunk) * 2u,
                                        HP39P_SAVED_INTRPPTR_NIBBLE, intrpptr))
        return -7;

    portal->expected_prologue = expected_prologue;
    const uint32_t kind = portal->request_kind;
    const uint32_t name_length = portal->request_name_length;
    if (!hp39_portal_gc_tail(portal, HP39P_DOCODE_NIBBLE) ||
        !hp39_portal_gc_tail(portal, HP39P_VARIABLE_STORE_NIBBLE + HP39P_VARIABLE_STORE_NIBBLES))
        return -7;
    if (kind >= HP39P_APLET_CLONE && kind <= HP39P_APLET_IS_DEFAULT) {
        const struct hpclang_hp39_aplet_request *request = (const void *)portal->request_name;
        if (!hp39_portal_gc_tail(portal, HP39P_NAME_NIBBLE + 10u + request->name_length * 2u))
            return -7;
        if (kind == HP39P_APLET_CLONE &&
            !hp39_portal_gc_tail(portal, HP39P_VARIABLE_OBJECT_NIBBLE + 10u +
                                             request->destination_length * 2u))
            return -7;
        if (kind == HP39P_APLET_REMOVE &&
            !hp39_portal_gc_tail(portal, HP39P_VARIABLE_OBJECT_NIBBLE + 7u +
                                             2u * (sizeof("'ApDirList") - 1u)))
            return -7;
    } else if (kind >= HP39P_PROGRAM_PUT && kind <= HP39P_NOTE_REMOVE) {
        const struct hpclang_hp39_catalog_request *request = (const void *)portal->request_name;
        if (!hp39_portal_gc_tail(portal, HP39P_NAME_NIBBLE + 7u + request->name_length * 2u))
            return -7;
        if ((kind == HP39P_PROGRAM_PUT || kind == HP39P_NOTE_PUT) &&
            !hp39_portal_gc_tail(portal, HP39P_VARIABLE_OBJECT_NIBBLE + request->object_nibbles))
            return -7;
    } else if (kind == HP39P_BODY_PROGRAM || kind == HP39P_BODY_NOTE) {
        if (!hp39_portal_gc_tail(portal, HP39P_NAME_NIBBLE + 7u + name_length * 2u))
            return -7;
    } else if (kind == HP39P_COPY_OBJECT) {
        const struct hpclang_hp39_object_request *request = (const void *)portal->request_name;
        if (!hp39_portal_gc_tail(portal, HP39P_VARIABLE_OBJECT_NIBBLE + request->object_nibbles))
            return -7;
    } else if (kind == HP39P_VARIABLE_STORE) {
        const struct hpclang_hp39_variable_store_request *request =
            (const struct hpclang_hp39_variable_store_request *)(uintptr_t)portal->request_name;
        if (!hp39_portal_gc_tail(portal, HP39P_VARIABLE_OBJECT_NIBBLE + request->object_nibbles))
            return -7;
    }

    if (!hp39_portal_stack_intact(portal))
        return -17;
    hpclang_cache_clean(hp39_saturn_portal_thunk, (uint32_t)sizeof(hp39_saturn_portal_thunk));

    store32((void *)portal->saturn_state, ARMSYS_C_LO,
            (uint32_t)(uintptr_t)&hpclang_hp39_portal_resume);

    l1[HPCLANG_L1_DATA_INDEX] = portal->stock_data_l1;
    l1[HPCLANG_L1_CODE_INDEX] = portal->stock_code_l1;
    hpclang_mmu_sync((volatile uint32_t *)portal->l2_workspace, l1);

    return 0;
}

int hpclang_hp39_portal_restore_hpc(void)
{
    struct hpclang_hp39_portal_context *portal = &hpclang_hp39_portal_context;
    volatile uint32_t *l1 = (volatile uint32_t *)portal->l1;

    int result = 0;
    const uint32_t *read_map = (const uint32_t *)portal->saturn_state;
    uint32_t post_dsktop = 0u;
    uint32_t restored_top = 0u, dropped = 0u;
    uint32_t object = 0u;
    uint32_t prologue = 0u;

    uint32_t editline = 0u;
    if (!stage_saturn_read_word5(read_map, RPL_EDITLINE, &editline) ||
        !stage_saturn_read_word5(read_map, RPL_DSKTOP, &post_dsktop) || editline < post_dsktop) {
        result = -4;
    } else {
        /* Normal calls produce a value and a success marker. Errors may leave
         * private operands below ERROR@; all are relative to the moved stack. */
        result = hpclang_hp39_portal_stack_plan(portal->pre_stack_span, post_dsktop, editline,
                                                &restored_top, &dropped);
        if (result == 0 && !stage_saturn_read_word5(read_map, post_dsktop, &object))
            result = -11;
        if (result == 0) {
            uint32_t type = 0u, error = 0u;
            if (!stage_saturn_read_word5(read_map, object, &type) || type != 0x02911u ||
                !stage_saturn_read_word5(read_map, object + 5u, &error))
                result = -14;
            else if (error != 0u)
                result = error == 1u ? -16 : -15; /* SUPROM39: MEMERR = ONE */
            else if (dropped != 2u)
                result = -10;
        }

        if (result == 0 && !stage_saturn_read_word5(read_map, post_dsktop + 5u, &object))
            result = -11;

        if (result == 0 && object > RPL_MAIN_END - 5u) {
            result = -12; /* Result object pointer outside Saturn main. */
        } else if (result == 0 && !stage_saturn_read_word5(read_map, object, &prologue)) {
            result = -13; /* Result prologue unreadable. */
        } else if (result == 0 && portal->expected_prologue != 0u &&
                   prologue != portal->expected_prologue) {
            result = -14; /* Result type violates request contract. */
        }

        if (result == 0) {
            const uint32_t kind = portal->request_kind;
            const int miss_request = kind == HP39P_BODY_PROGRAM || kind == HP39P_BODY_NOTE ||
                                     kind == HP39P_VARIABLE_VALUE;
            const uint32_t miss_object = portal->saturn_entry + HP39P_MISS_NIBBLE;
            if (miss_request && object == miss_object) {
                /*
                 * Keep semantic miss distinct from an existing empty body.
                 * The thunk-local sentinel is never exposed to runtime.
                 */
                result = 1;
            } else {
                portal->result_object = object;
                portal->result_prologue = prologue;
            }
        }

        /* Drop private results/operands relative to the current EDITLINE:
         * Stock may have moved the stack while resizing the variable. Preserve
         * its AVMEM changes; release only the cells removed by this cleanup. */
        if (result == 0 || result == 1 || result == -15 || result == -16) {
            uint32_t avmem = 0u;
            if (!stage_saturn_read_word5(read_map, RPL_AVMEM, &avmem) ||
                dropped > 0xfffffu - avmem ||
                !stage_saturn_write_word5(read_map, RPL_DSKTOP, restored_top) ||
                !stage_saturn_write_word5(read_map, RPL_AVMEM, avmem + dropped))
                result = -6;
        }
    }

    if (portal->pinned_cells != 0u) {
        uint32_t caller_top = editline - portal->incoming_stack_span;
        uint32_t pc = 0u, stream = 0u;
        if (!stage_saturn_read_word5(read_map, caller_top - 5u, &pc) ||
            !stage_saturn_read_word5(read_map, caller_top - 10u, &stream) ||
            !stage_saturn_write_word5(read_map, RPL_INTRPPTR, stream))
            result = -6;
        else
            portal->stock_regs[0] = pc;
    }
    store32((void *)portal->saturn_state, ARMSYS_C_LO, portal->armsys_target);

    /*
     * The emulator may have reused the original stage0 save slots while Stock
     * mappings/SP were active. Rebuild them from the immutable first-entry
     * copy so final stage0 teardown still restores the true launcher caller.
     */
    volatile uint32_t *saved = (volatile uint32_t *)portal->stock_frame;
    for (uint32_t i = 0u; i < 14u; ++i)
        saved[i] = portal->stock_regs[i];

    l1[HPCLANG_L1_DATA_INDEX] = portal->hpc_data_l1;
    l1[HPCLANG_L1_CODE_INDEX] = portal->hpc_code_l1;
    hpclang_mmu_sync((volatile uint32_t *)portal->l2_workspace, l1);

    ++portal->trips;
    portal->active = 0u;
    return hp39_portal_stack_intact(portal) ? result : -17;
}

static int saturn_read_range_present(const void *saturn_state, uint32_t first_nibble,
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

static int stage_saturn_read_nibble(const uint32_t *read_map, uint32_t address, uint8_t *value)
{
    if (read_map == NULL || value == NULL || address >= RPL_MAIN_END)
        return 0;

    uint32_t base = read_map[address >> 12];
    if (base == 0u)
        return 0;

    const volatile uint8_t *p =
        (const volatile uint8_t *)(uintptr_t)(base + ((address & UINT32_C(0x0fff)) >> 1));
    uint8_t packed = *p;
    *value = (address & 1u) != 0u ? (uint8_t)(packed >> 4) : (uint8_t)(packed & 0x0fu);
    return 1;
}

static int stage_saturn_read_word5(const uint32_t *read_map, uint32_t address, uint32_t *value)
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

static int stage_saturn_write_nibble(const uint32_t *read_map, uint32_t address, uint8_t value)
{
    if (read_map == NULL || value > 0x0fu || address >= RPL_MAIN_END)
        return 0;

    uint32_t base = read_map[address >> 12];
    if (base == 0u)
        return 0;

    volatile uint8_t *p =
        (volatile uint8_t *)(uintptr_t)(base + ((address & UINT32_C(0x0fff)) >> 1));
    uint8_t packed = *p;
    if ((address & 1u) != 0u)
        packed = (uint8_t)((packed & 0x0fu) | (uint8_t)(value << 4));
    else
        packed = (uint8_t)((packed & 0xf0u) | value);
    *p = packed;
    return 1;
}

static int stage_saturn_write_word5(const uint32_t *read_map, uint32_t address, uint32_t value)
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
        if (!stage_saturn_write_nibble(read_map, address + i,
                                       (uint8_t)((value >> (i * 4u)) & 0x0fu)))
            return 0;
    }
    return 1;
}

static int stage_saturn_read_byte(const uint32_t *read_map, uint32_t address, uint8_t *value)
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

struct stage_launcher_bias
{
    uint32_t object;
    uint32_t active;
};

static int stage_hide_launcher_bias(const void *saturn_state, uintptr_t stage0_code,
                                    struct stage_launcher_bias *bias)
{
    static const uint8_t launcher_magic[] = {0x4cu, 0xb3u, 0x80u, 0x41u, 0x3eu, 0x43u, 0x50u};

    if (saturn_state == NULL || bias == NULL)
        return 0;

    bias->object = 0u;
    bias->active = 0u;

    const uint32_t *read_map = (const uint32_t *)saturn_state;
    uint32_t dsktop = 0u;
    uint32_t object = 0u;
    uint32_t prologue = 0u;
    uint32_t length = 0u;

    if (!stage_saturn_read_word5(read_map, RPL_DSKTOP, &dsktop) || dsktop > RPL_MAIN_END - 5u ||
        !stage_saturn_read_word5(read_map, dsktop, &object) || object > RPL_MAIN_END - 24u ||
        !stage_saturn_read_word5(read_map, object, &prologue) ||
        !stage_saturn_read_word5(read_map, object + 5u, &length) || length < 19u)
        return 0;

    if (prologue == RPL_DOCSTR) {
        for (uint32_t i = 0u; i < (uint32_t)(sizeof(launcher_magic) / sizeof(launcher_magic[0]));
             ++i) {
            uint8_t byte = 0u;
            if (!stage_saturn_read_byte(read_map, object + 10u + i * 2u, &byte) ||
                byte != launcher_magic[i])
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
            if (!stage_saturn_read_byte(read_map, object + 18u + 2u * i, &entry_byte) ||
                !stage_saturn_read_byte(read_map, object + 34u + 2u * i, &base_byte))
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
    if (!stage_saturn_write_word5(read_map, RPL_DSKTOP, dsktop + 5u))
        return -1;

    bias->object = object;
    bias->active = 1u;
    return 1;
}

static int stage_restore_launcher_bias(const void *saturn_state,
                                       const struct stage_launcher_bias *bias)
{
    if (bias == NULL || bias->active == 0u)
        return 1;
    if (saturn_state == NULL)
        return 0;

    const uint32_t *read_map = (const uint32_t *)saturn_state;
    uint32_t dsktop = 0u;
    if (!stage_saturn_read_word5(read_map, RPL_DSKTOP, &dsktop) || dsktop < RPL_MAIN_FIRST + 5u)
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
static int stage_phys_page_saturn_start(const uint32_t *read_map, uint32_t phys_page,
                                        uint32_t *saturn_start)
{
    if (read_map == NULL || saturn_start == NULL)
        return 0;

    phys_page &= UINT32_C(0xfffff000);
    for (uint32_t page = RPL_MAIN_FIRST >> 12; page < RPL_MAIN_END >> 12; ++page) {
        uint32_t base = read_map[page];
        if (base < phys_page || base >= phys_page + HPCLANG_PAGE_SIZE)
            continue;

        uint32_t byte_offset = base - phys_page;
        uint32_t page_nibble = page << 12;
        uint32_t nibble_offset = byte_offset << 1;
        if (nibble_offset > page_nibble)
            continue;

        uint32_t candidate = page_nibble - nibble_offset;
        if (candidate < RPL_MAIN_FIRST || candidate > RPL_MAIN_END - RPL_PHYS_PAGE_NIBBLES)
            continue;

        *saturn_start = candidate;
        return 1;
    }
    return 0;
}

static int stage_rpl_memory_bounds(const void *saturn_state, volatile const uint32_t *pool_l2,
                                   uint32_t pool_pages, struct hpclang_rpl_memory_bounds *bounds,
                                   uint32_t model)
{
    if (saturn_state == NULL || pool_l2 == NULL || bounds == NULL)
        return 0;

    const uint32_t *read_map = (const uint32_t *)saturn_state;
    uint32_t rsktop = 0u;
    uint32_t dsktop = 0u;
    if (!stage_saturn_read_word5(read_map, RPL_RSKTOP, &rsktop) ||
        !stage_saturn_read_word5(read_map, RPL_DSKTOP, &dsktop) || rsktop < RPL_MAIN_FIRST ||
        dsktop > RPL_MAIN_END || rsktop >= dsktop)
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
        if (!stage_phys_page_saturn_start(read_map, pool_l2[i] & UINT32_C(0xfffff000), &sat_start))
            continue;

        uint32_t sat_end = sat_start + RPL_PHYS_PAGE_NIBBLES;

        /* A native page must not already overlap a live RPL stack boundary. */
        if ((sat_start < rsktop && sat_end > rsktop) || (sat_start < dsktop && sat_end > dsktop))
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
    bounds->native_roots = NULL;
    bounds->native_root_count = 0u;
    bounds->port1_limit = 0u;
    if (!hpclang_stage_port1_limit(model, pool_l2, pool_pages, &bounds->port1_limit))
        bounds->magic = HPCLANG_RPL_BOUNDS_MAGIC_V2;
    return 1;
}

static uintptr_t live_saturn_state(void *saturn_state, uint32_t model)
{
    switch (model) {
    case HPCLANG_MODEL_HP39GPLUS:
    case HPCLANG_MODEL_HP39GS:
    case HPCLANG_MODEL_HP40GS:
        return saturn_read_range_present(saturn_state, HP39_MAIN_FIRST_NIBBLE,
                                         HP39_MAIN_LAST_NIBBLE) &&
                       saturn_read_range_present(saturn_state, HP39_MINI_FIRST_NIBBLE,
                                                 HP39_MINI_LAST_NIBBLE)
                   ? (uintptr_t)saturn_state
                   : 0u;

    case HPCLANG_MODEL_HP48GII:
    case HPCLANG_MODEL_HP49GPLUS:
    case HPCLANG_MODEL_HP50G:
        return saturn_read_range_present(saturn_state, HP4950_MINI_FIRST_NIBBLE,
                                         HP4950_MINI_LAST_NIBBLE) &&
                       saturn_read_range_present(saturn_state, HP4950_BIG_OBJECT_NIBBLE,
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
 * the temporary stage window. We use that pool only as startup storage;
 * HPC1 itself sees the normal hpclang v1 fixed layout.
 */
int hpclang_armtoolbox_stage1(void *saturn_state, const struct hpclang_image_header *hpc1,
                              uint32_t stage1_size, uint32_t model, uint32_t saturn_pc,
                              uintptr_t emulator_lr, uint32_t stock_cpsr, uintptr_t stock_frame,
                              uintptr_t stage0_code, uint32_t stage0_size, uint32_t stored_size)
{
    if (!hpclang_stage_image_valid(hpc1, stored_size))
        return -1;

    uint32_t total_pages = load32(saturn_state, ARMSYS_PAGE_COUNT);
    uint32_t old_l2_phys = load32(saturn_state, ARMSYS_L2_PHYS);
    volatile uint32_t *old_l2 = (volatile uint32_t *)(uintptr_t)STAGE_WINDOW_BASE;

    uint32_t stage_pages =
        (STAGE_CODE_OFFSET + stage1_size + HPCLANG_PAGE_SIZE - 1u) / HPCLANG_PAGE_SIZE;
    uint32_t code_count = (hpc1->image_size + HPCLANG_PAGE_SIZE - 1u) / HPCLANG_PAGE_SIZE;

    uintptr_t live_state = live_saturn_state(saturn_state, model);
    int preserve_stage0 = live_state != 0u;
    /* The unused half of the L2 workspace holds the PIC return path. */
    if (preserve_stage0 &&
        (stage0_size == 0u || stage0_size > HPCLANG_L2_WORKSPACE_BYTES - HPCLANG_L2_TABLES_BYTES ||
         hpclang_stage0_resume_target < stage0_code ||
         hpclang_stage0_resume_target - stage0_code >= stage0_size))
        return -5;
    uint32_t workspace_page = stage_pages;
    uint32_t code_first = workspace_page + 1u;
    uint32_t rw_first = code_first + code_count;

    if (code_count > HPCLANG_CODE_PAGE_CAPACITY)
        return -2;
    if (total_pages > HPCLANG_L2_ENTRIES || rw_first + STAGE_STACK_PAGES >= total_pages)
        return -3;

    uint32_t rw_count = total_pages - STAGE_STACK_PAGES - rw_first;
    uint32_t rw_needed = 1u + (hpc1->rw_static_size + HPCLANG_PAGE_SIZE - 1u) / HPCLANG_PAGE_SIZE +
                         HPCLANG_STACK_MIN / HPCLANG_PAGE_SIZE;

    if (rw_count < rw_needed || rw_count > HPCLANG_RW_PAGE_CAPACITY)
        return -4;

    if (preserve_stage0) {
        uint32_t page = old_l2[workspace_page] & UINT32_C(0xfffff000);
        /* Native pool pages may be system RAM outside Saturn Read[]. Prove
           the Stock identity section from its live ARM page table instead. */
        volatile const uint32_t *stock_l1 = (volatile const uint32_t *)(uintptr_t)0x07f00000u;
        uint32_t descriptor = stock_l1[page >> 20];
        if ((descriptor & 3u) != 2u ||
            (descriptor & UINT32_C(0xfff00000)) != (page & UINT32_C(0xfff00000)))
            return -6;
        uintptr_t stock_alias = page + HPCLANG_L2_TABLES_BYTES;
        volatile uint8_t *copy =
            (volatile uint8_t *)(uintptr_t)(STAGE_WINDOW_BASE + workspace_page * HPCLANG_PAGE_SIZE +
                                            HPCLANG_L2_TABLES_BYTES);
        const uint8_t *source = (const uint8_t *)stage0_code;
        for (uint32_t i = 0u; i < stage0_size; ++i)
            copy[i] = source[i];
        hpclang_cache_clean((const void *)copy, stage0_size);
        /* bootstrap MMU publication invalidates I-cache before this is used.
           The Stock alias remains valid after stage0 removes the stage mapping. */
        hpclang_stage0_resume_target = stock_alias + (hpclang_stage0_resume_target - stage0_code);
    }

    /* Both sets come from one coarse page pool. Share its bounded address
       array so decoding also fits comfortably in the one-page stage stack. */
    uint32_t pages[HPCLANG_L2_ENTRIES];
    uint32_t *code_pages = pages;
    uint32_t *rw_pages = pages + code_count;

    for (uint32_t i = 0; i < code_count; ++i)
        code_pages[i] = old_l2[code_first + i] & 0xfffff000u;

    for (uint32_t i = 0; i < rw_count; ++i)
        rw_pages[i] = old_l2[rw_first + i] & 0xfffff000u;

    if (hpclang_stage_image_unpack(
            hpc1, stored_size,
            (void *)(uintptr_t)(STAGE_WINDOW_BASE + code_first * HPCLANG_PAGE_SIZE),
            code_count * HPCLANG_PAGE_SIZE))
        return -7;

    hpclang_cache_clean(
        (const void *)(uintptr_t)(STAGE_WINDOW_BASE + code_first * HPCLANG_PAGE_SIZE),
        code_count * HPCLANG_PAGE_SIZE);

    struct hpclang_launch_context launch;
    struct hpclang_rpl_memory_bounds rpl_bounds;
    struct stage_launcher_bias launcher_bias = {0u, 0u};
    uint32_t *native_roots[] = {&launcher_bias.object, (uint32_t *)stock_frame};

    launch.mmu.l1 = (volatile uint32_t *)(uintptr_t)0x07f00000u;
    launch.mmu.l2_workspace =
        (volatile uint32_t *)(uintptr_t)(STAGE_WINDOW_BASE + workspace_page * HPCLANG_PAGE_SIZE);
    launch.mmu.l2_workspace_phys = old_l2[workspace_page] & 0xfffff000u;
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
     * model's exact nibble offset. HP39g+ D1.55 uses pages 0x6e and 0x6f
     * established from its physical full-NOR fixture.
     */
    if (model == HPCLANG_MODEL_HP39GS || model == HPCLANG_MODEL_HP40GS) {
        launch.mmu.code_window_pages = hp39_font_nor_pages;
        launch.mmu.code_window_page_count =
            (uint32_t)(sizeof(hp39_font_nor_pages) / sizeof(hp39_font_nor_pages[0]));
    } else if (model == HPCLANG_MODEL_HP39GPLUS) {
        launch.mmu.code_window_pages = hp39gplus_font_nor_pages;
        launch.mmu.code_window_page_count =
            (uint32_t)(sizeof(hp39gplus_font_nor_pages) / sizeof(hp39gplus_font_nor_pages[0]));
    }

    launch.model = model;
    launch.entry_offset = hpc1->entry_offset;
    /* Require the established ROM ranges in Read[] before advertising a live
       Stock context. This keeps Direct E2E's synthetic allocator block from
       masquerading as a live map; HP39 fixed fonts use the private window. */
    launch.saturn_state = live_state;

    /*
     * Our Launcher-V3 wrapper is a DOCSTR whose payload starts with
     * "L\xB3\x80A>CP". Historical HPGCC 1.1 launchers leave that object as
     * a level-1 bias; newer launcher paths do not necessarily do so. Detect
     * the actual object instead of hard-coding launcher version/model policy.
     */
    if (launch.saturn_state != 0u) {
        int bias_status = stage_hide_launcher_bias(saturn_state, stage0_code, &launcher_bias);
        if (bias_status < 0)
            return -5;
    }
    if (model == HPCLANG_MODEL_HP39GPLUS || model == HPCLANG_MODEL_HP39GS ||
        model == HPCLANG_MODEL_HP40GS) {
        hp39_portal_configure(&launch, saturn_state, model, saturn_pc, emulator_lr, stock_cpsr,
                              stock_frame);
        struct hpclang_hp39_portal_context *portal = &hpclang_hp39_portal_context;
        portal->arena_first = RPL_MAIN_END;
        portal->arena_end = 0u;
        portal->launcher_object = launcher_bias.object;
        if (live_state != 0u) {
            for (uint32_t i = 0u; i < total_pages; ++i) {
                uint32_t start = 0u;
                if (stage_phys_page_saturn_start((const uint32_t *)saturn_state,
                                                 old_l2[i] & 0xfffff000u, &start)) {
                    if (start < portal->arena_first)
                        portal->arena_first = start;
                    if (start + RPL_PHYS_PAGE_NIBBLES > portal->arena_end)
                        portal->arena_end = start + RPL_PHYS_PAGE_NIBBLES;
                }
            }
        }
        launch.platform_cookie = (uintptr_t)&hpclang_hp39_portal_context.public_context |
                                 HPCLANG_PLATFORM_COOKIE_HP39_TAG;
    } else if (launch.saturn_state != 0u &&
               (model == HPCLANG_MODEL_HP48GII || model == HPCLANG_MODEL_HP49GPLUS ||
                model == HPCLANG_MODEL_HP50G) &&
               stage_rpl_memory_bounds(saturn_state, old_l2, total_pages, &rpl_bounds, model)) {
        /*
         * A hidden HPGCC-1.1-style Launcher-V3 pointer needs one physical
         * stack cell when stage1 restores it after HPC1 returns. init/exit do
         * not expose that reservation through AVMEM, so carry it explicitly.
         */
        rpl_bounds.stack_reserve_nibbles = launcher_bias.active != 0u ? 5u : 0u;
        /* Hidden launcher value and saved post-ARMSYS Saturn PC are live
           roots even though neither is in a Stock-visible pointer table. */
        rpl_bounds.native_roots = native_roots;
        rpl_bounds.native_root_count = sizeof(native_roots) / sizeof(native_roots[0]);
        /* Do not publish Saturn's machine return stack as unconditional roots:
         * RSTK=C can also save numbers. Operations that move TEMP code must
         * classify the suspended caller's slots before supplying extra roots. */

        /*
         * rpl_bounds lives in this stage1 stack frame, whose ARMToolbox window
         * remains mapped for the complete hpclang_bootstrap_run() call.
         */
        launch.platform_cookie = (uintptr_t)&rpl_bounds | HPCLANG_PLATFORM_COOKIE_RPL_BOUNDS_TAG;
    } else {
        /* No mutation fence: read-only Saturn services may still be available. */
        launch.platform_cookie = old_l2_phys;
    }

    int result = hpclang_bootstrap_run(&launch);

    struct hpclang_hp39_portal_context *portal = &hpclang_hp39_portal_context;
    if (portal->pinned_cells != 0u) {
        const uint32_t *map = saturn_state;
        uint32_t editline = 0u, top = 0u, avmem = 0u;
        if (!stage_saturn_read_word5(map, RPL_EDITLINE, &editline))
            return -8;
        top = editline - portal->incoming_stack_span;
        if (!stage_saturn_read_word5(map, top - 15u, &launcher_bias.object) ||
            !stage_saturn_read_word5(map, RPL_AVMEM, &avmem) ||
            !stage_saturn_write_word5(map, RPL_DSKTOP, top) ||
            !stage_saturn_write_word5(map, RPL_AVMEM, avmem + portal->pinned_cells))
            return -8;
        for (uint32_t i = 0u; i < 4u; ++i)
            store32(saturn_state, ARMSYS_PAGE_COUNT + 4u * i, portal->native_call[i]);
    }

    if (!stage_restore_launcher_bias(saturn_state, &launcher_bias))
        return -6;

    return result;
}
