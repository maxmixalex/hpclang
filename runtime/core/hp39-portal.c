#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <hpclang/aplet.h>
#include "hp39-aplet-frame.h"

#include <hpclang/abi.h>
#include <hpclang/bootstrap.h>
#include <hpclang/object.h>
#include <hpclang/variable.h>
#include <hpclang/target.h>

#include "hp39-native-directory.h"
#include "hp39-variable-schema.h"
#include "object-snapshot.h"
#include "../../platform/armtoolbox/hp39-portal-private.h"
#include "saturn-view.h"
#include "rpl-stock-memory.h"

#define HP39_RPL_DOLIST UINT32_C(0x00002a74)
#define HP39_SUPPORTED_ENTRY UINT32_C(0x00011111)
#define HP39_SATURN_LIMIT UINT32_C(0x00100000)
#define HP39_BODY_NAME_MAX 19u
#define HP39_APLET_NAME_MAX 254u
#define HP39_VARIABLE_ID_MAX 255u
#define HP39_PGMCXT_POINTER UINT32_C(0x00080739)
#define HP39_NOTESCXT_POINTER UINT32_C(0x0008073e)

static int hp39_snapshot_read_nibble(void *context, uint32_t nibble_address, uint8_t *value)
{
    (void)context;
    return hpclang_saturn_read_nibble(hpclang_saturn_address_make(nibble_address), value);
}

static const struct hpclang_rpl_reader hp39_stock_reader = {
    .read_nibble = hp39_snapshot_read_nibble,
    .context = NULL,
};

static int hp39_stock_read_word5(uint32_t address, uint32_t *value)
{
    if (value == NULL || address > HP39_SATURN_LIMIT - 5u)
        return 0;

    uint32_t out = 0u;
    for (uint32_t i = 0u; i < 5u; ++i) {
        uint8_t nibble = 0u;
        if (!hp39_stock_reader.read_nibble(hp39_stock_reader.context, address + i, &nibble) ||
            nibble > 0x0fu)
            return 0;
        out |= (uint32_t)nibble << (i * 4u);
    }
    *value = out;
    return 1;
}

static int hp39_live_stock_available(void)
{
    const struct hpclang_boot_info *boot = hpclang_boot_info();
    return boot != NULL &&
           (boot->model == HPCLANG_MODEL_HP39GPLUS || boot->model == HPCLANG_MODEL_HP39GS ||
            boot->model == HPCLANG_MODEL_HP40GS) &&
           boot->saturn_state != 0u;
}

static int hp39_pin_native_arena(struct hpclang_hp39_portal_context *portal)
{
    if (portal->pinned_cells != 0u)
        return 0;
    uint32_t top = 0u, editline = 0u, stream = 0u;
    if (portal->arena_end < 0x80000u || portal->arena_end > 0xffff0u ||
        !hp39_stock_read_word5(0x806f8u, &top) || !hp39_stock_read_word5(0x806fdu, &editline) ||
        editline < top || !hp39_stock_read_word5(0x8076bu, &stream)) {
        errno = EILSEQ;
        return -1;
    }
    struct hpclang_rpl_stock_memory memory;
    const volatile uint32_t *l1 = (const void *)portal->l1;
    if (!hpclang_rpl_stock_memory_capture(&memory, (const void *)portal->saturn_state, l1[0x80])) {
        errno = ENOSYS;
        return -1;
    }
    portal->incoming_stack_span = editline - top;
    uint32_t roots[] = {portal->stock_regs[0], stream,
                        portal->launcher_object != 0u ? portal->launcher_object : 0x04049u};
    struct hpclang_rpl_mutation_io io = {
        .read_nibble = hpclang_rpl_stock_memory_read,
        .write_nibble = hpclang_rpl_stock_memory_write,
        .context = &memory,
        .temp_limit = top,
        .user_limit = 0x80000u,
    };
    int result = hpclang_rpl_mutation_pin_arena(&io, portal->arena_first, portal->arena_end + 16u,
                                                roots, 3u, &portal->pinned_cells);
    if (result != 0) {
        errno = result == HPCLANG_RPL_MUTATION_NOSPACE ? ENOSPC : EILSEQ;
        return -1;
    }
    uint32_t temp_top = 0u;
    if (!hp39_stock_read_word5(0x806eeu, &temp_top)) {
        errno = EIO;
        return -1;
    }
    portal->arena_object_end = temp_top - 5u;
    return 0;
}

static int hp39_portal_result(uint32_t kind, const uint8_t *name, size_t name_length,
                              uint32_t expected_prologue, uint32_t *object, uint32_t *prologue)
{
    if (object == NULL || prologue == NULL) {
        errno = EINVAL;
        return -1;
    }
    *object = 0u;
    *prologue = 0u;

    const struct hpclang_boot_info *boot = hpclang_boot_info();
    if (boot == NULL ||
        (boot->model != HPCLANG_MODEL_HP39GPLUS && boot->model != HPCLANG_MODEL_HP39GS &&
         boot->model != HPCLANG_MODEL_HP40GS) ||
        boot->saturn_state == 0u ||
        (boot->platform_cookie & (uintptr_t)3u) != HPCLANG_PLATFORM_COOKIE_HP39_TAG) {
        errno = ENOSYS;
        return -1;
    }

    const struct hpclang_hp39_platform_context *context =
        (const struct hpclang_hp39_platform_context *)(boot->platform_cookie & ~(uintptr_t)3u);
    if (context->magic != HPCLANG_HP39_PLATFORM_MAGIC || context->portal_entry == 0u) {
        errno = ENOSYS;
        return -1;
    }

    size_t maximum_name_length =
        kind == HP39P_VARIABLE_STORE
            ? sizeof(struct hpclang_hp39_variable_store_request)
            : (kind == HP39P_VARIABLE_VALUE || kind == HP39P_REFERENCE_EVAL ? 3u
                                                                            : HP39_BODY_NAME_MAX);
    if (kind == HP39P_COPY_OBJECT)
        maximum_name_length = sizeof(struct hpclang_hp39_object_request);
    if (name_length > maximum_name_length || (name == NULL && name_length != 0u)) {
        errno = EINVAL;
        return -1;
    }

    int (*entry)(uint32_t *, uint32_t *, uint32_t, const uint8_t *, uint32_t) =
        (int (*)(uint32_t *, uint32_t *, uint32_t, const uint8_t *, uint32_t))(
            uintptr_t)context->portal_entry;
    if (hp39_pin_native_arena((struct hpclang_hp39_portal_context *)(uintptr_t)context) != 0)
        return -1;
    int result = entry(object, prologue, kind, name, (uint32_t)name_length);
    ((struct hpclang_hp39_portal_context *)(uintptr_t)context)->last_result = result;
    if (result == 1) {
        errno = ENOENT;
        return -1;
    }
    if (result == -2) {
        errno = EBUSY;
        return -1;
    }
    /* Structural portal faults are I/O failures. Internal diagnostic codes
     * remain private instead of being disguised as unrelated errno values. */
    if (result == -16) {
        errno = ENOSPC;
        return -1;
    }
    if (result == -15) {
        errno = EINVAL;
        return -1;
    }
    if (result != 0) {
        errno = EIO;
        return -1;
    }
    if (*object >= HP39_SATURN_LIMIT ||
        (expected_prologue != 0u && *prologue != expected_prologue)) {
        errno = EILSEQ;
        return -1;
    }
    return 0;
}

int hpclang_hp39_portal_roundtrip(void)
{
    uint32_t object = 0u;
    uint32_t prologue = 0u;
    return hp39_portal_result(HP39P_CATALOG_APLET, NULL, 0u, HP39_RPL_DOLIST, &object, &prologue);
}

int hpclang_hp39_variable_schema_snapshot(struct hpclang_object **object)
{
    if (object == NULL) {
        errno = EINVAL;
        return -1;
    }
    *object = NULL;

    if (!hp39_live_stock_available()) {
        errno = ENOSYS;
        return -1;
    }

    uint32_t address = 0u;
    uint32_t prologue = 0u;
    if (hp39_portal_result(HP39P_VARIABLE_SCHEMA, NULL, 0u, HP39_RPL_DOLIST, &address, &prologue) !=
        0)
        return -1;

    if (hpclang_object_snapshot_copy(&hp39_stock_reader, address, HP39_SATURN_LIMIT - address,
                                     object) != 0)
        return -1;
    if (*object == NULL || hpclang_object_kind(*object) != HPCLANG_OBJECT_LIST) {
        hpclang_object_release(*object);
        *object = NULL;
        errno = EILSEQ;
        return -1;
    }
    return 0;
}

static int hp39_variable_reference_snapshot(uint32_t kind, const uint8_t *payload,
                                            size_t payload_length, uint32_t expected_prologue,
                                            struct hpclang_object **object)
{
    if (object == NULL) {
        errno = EINVAL;
        return -1;
    }
    *object = NULL;

    uint32_t address = 0u;
    uint32_t prologue = 0u;
    if (hp39_portal_result(kind, payload, payload_length, 0u, &address, &prologue) != 0)
        return -1;

    if (hpclang_object_snapshot_copy(&hp39_stock_reader, address, HP39_SATURN_LIMIT - address,
                                     object) != 0)
        return -1;
    if (*object == NULL) {
        errno = EILSEQ;
        return -1;
    }

    if (expected_prologue != 0u && (*object)->native_prologue != expected_prologue) {
        hpclang_object_release(*object);
        *object = NULL;
        errno = EILSEQ;
        return -1;
    }
    return 0;
}

static int hp39_variable_reference_resolve(void *context, uint32_t reference,
                                           uint32_t expected_prologue,
                                           struct hpclang_object **object)
{
    (void)context;
    if (object == NULL || reference == 0u || reference >= HP39_SATURN_LIMIT) {
        if (object != NULL)
            *object = NULL;
        errno = EINVAL;
        return -1;
    }
    *object = NULL;

    uint32_t prologue = 0u;
    if (!hp39_stock_read_word5(reference, &prologue)) {
        errno = EIO;
        return -1;
    }

    /*
     * Ordinary schema references, including xNoteText (#2C812), are real
     * object addresses. Preserve their data semantics by snapshotting them
     * directly; do not route them through EVAL.
     */
    if (prologue == expected_prologue) {
        if (hpclang_object_snapshot_copy(&hp39_stock_reader, reference,
                                         HP39_SATURN_LIMIT - reference, object) != 0)
            return -1;
        if (*object == NULL || (*object)->native_prologue != expected_prologue) {
            hpclang_object_release(*object);
            *object = NULL;
            errno = EILSEQ;
            return -1;
        }
        return 0;
    }

    /*
     * Stock virtual #3Fxxx supported entries carry the #11111 trampoline in
     * the logical cell. The reviewed HP39 ROM proves MiniVar's {NoteText} and
     * {SketchSet} entries use this mechanism. Execute only this explicitly
     * recognized trampoline through Stock, then require the caller's exact
     * expected data prologue. Unknown references fail closed.
     */
    if (prologue != HP39_SUPPORTED_ENTRY) {
        errno = EILSEQ;
        return -1;
    }

    uint8_t payload[3] = {
        (uint8_t)(reference & 0xffu),
        (uint8_t)((reference >> 8u) & 0xffu),
        (uint8_t)((reference >> 16u) & 0xffu),
    };
    return hp39_variable_reference_snapshot(HP39P_REFERENCE_EVAL, payload, 3u, expected_prologue,
                                            object);
}

int hpclang_variable_get(const uint8_t *category, size_t category_length, const uint8_t *name,
                         size_t name_length, struct hpclang_object **object)
{
    if (object == NULL || category == NULL || category_length == 0u || name == NULL ||
        name_length == 0u || name_length > HP39_VARIABLE_ID_MAX - 3u) {
        if (object != NULL)
            *object = NULL;
        errno = EINVAL;
        return -1;
    }
    *object = NULL;

    struct hpclang_object *schema = NULL;
    if (hpclang_hp39_variable_schema_snapshot(&schema) != 0)
        return -1;

    struct hpclang_hp39_variable_schema_match match;
    if (hpclang_hp39_variable_schema_find_resolved(schema, category, category_length, name,
                                                   name_length, hp39_variable_reference_resolve,
                                                   NULL, &match) != 0) {
        hpclang_object_release(schema);
        return -1;
    }
    hpclang_object_release(schema);

    /*
     * HP39 Stock @ is the native generic recall dispatcher for special /
     * extended IDs. Its DOIDNT path reads the ID's trailing descriptor
     * pointer, selects descriptor word 0 and preserves the execution-token
     * context required by #11111 / #3Fxxx supported entries.
     *
     * Therefore value recall must hand @ the calculator's genuine extended-ID
     * object. Do not reconstruct the ID and do not execute its descriptor
     * getter separately in hpclang; both shortcuts bypass part of Stock's
     * special-variable dispatch semantics.
     *
     * Inline IDs in an owned schema snapshot have no trustworthy live
     * calculator address. They remain fail-closed until a concrete HPGCC
     * schema requires safe Stock-side materialization.
     */
    if (match.source_reference == 0u) {
        errno = ENOTSUP;
        return -1;
    }

    uint8_t payload[3] = {
        (uint8_t)(match.source_reference & 0xffu),
        (uint8_t)((match.source_reference >> 8u) & 0xffu),
        (uint8_t)((match.source_reference >> 16u) & 0xffu),
    };
    return hp39_variable_reference_snapshot(HP39P_VARIABLE_VALUE, payload, sizeof(payload), 0u,
                                            object);
}

/* Resolve a genuine schema ID before handing the owned value to Stock STO.
 * Stock owns native type validation, context selection and variable resizing. */
int hpclang_variable_set(const uint8_t *category, size_t category_length, const uint8_t *name,
                         size_t name_length, const struct hpclang_object *object)
{
    if (object == NULL || category == NULL || category_length == 0u || name == NULL ||
        name_length == 0u || name_length > HP39_VARIABLE_ID_MAX - 3u) {
        errno = EINVAL;
        return -1;
    }
    if (!hp39_live_stock_available()) {
        errno = ENOSYS;
        return -1;
    }
    if (object->nibble_count < 5u || object->nibble_count > HP39P_STAGED_OBJECT_MAX_NIBBLES) {
        errno = E2BIG;
        return -1;
    }

    struct hpclang_object *schema = NULL;
    if (hpclang_hp39_variable_schema_snapshot(&schema) != 0)
        return -1;

    struct hpclang_hp39_variable_schema_match match;
    if (hpclang_hp39_variable_schema_find_resolved(schema, category, category_length, name,
                                                   name_length, hp39_variable_reference_resolve,
                                                   NULL, &match) != 0) {
        hpclang_object_release(schema);
        return -1;
    }
    hpclang_object_release(schema);

    if (match.source_reference == 0u) {
        errno = ENOTSUP;
        return -1;
    }

    struct hpclang_hp39_variable_descriptor descriptor;
    if (hpclang_hp39_variable_descriptor_read(&hp39_stock_reader, match.descriptor_address,
                                              &descriptor) != 0)
        return -1;

    struct hpclang_hp39_variable_store_request request = {
        .id_reference = match.source_reference,
        .type_predicate = descriptor.type_predicate_word,
        .object_data = (uintptr_t)object->data,
        .object_nibbles = object->nibble_count,
    };
    uint32_t marker = 0u;
    uint32_t marker_prologue = 0u;
    return hp39_portal_result(HP39P_VARIABLE_STORE, (const uint8_t *)&request, sizeof(request), 0u,
                              &marker, &marker_prologue);
}

int hpclang_hp39_aplet_catalog_snapshot(struct hpclang_object **object)
{
    if (!hp39_live_stock_available()) {
        if (object != NULL)
            *object = NULL;
        errno = ENOSYS;
        return -1;
    }
    return hpclang_hp39_native_aplet_catalog_snapshot(&hp39_stock_reader, object);
}

int hpclang_hp39_aplet_snapshot(const uint8_t *name, size_t name_length,
                                struct hpclang_object **object)
{
    if (object == NULL || name == NULL || name_length == 0u || name_length > HP39_APLET_NAME_MAX) {
        if (object != NULL)
            *object = NULL;
        errno = EINVAL;
        return -1;
    }
    *object = NULL;

    if (!hp39_live_stock_available()) {
        errno = ENOSYS;
        return -1;
    }
    return hpclang_hp39_native_aplet_snapshot(&hp39_stock_reader, name, name_length, object);
}

static int hp39_native_catalog_snapshot(uint32_t context_pointer, struct hpclang_object **object)
{
    if (!hp39_live_stock_available()) {
        if (object != NULL)
            *object = NULL;
        errno = ENOSYS;
        return -1;
    }
    return hpclang_hp39_native_catalog_snapshot(&hp39_stock_reader, context_pointer, object);
}

int hpclang_hp39_program_catalog_snapshot(struct hpclang_object **object)
{
    return hp39_native_catalog_snapshot(HP39_PGMCXT_POINTER, object);
}

int hpclang_hp39_note_catalog_snapshot(struct hpclang_object **object)
{
    return hp39_native_catalog_snapshot(HP39_NOTESCXT_POINTER, object);
}

static int hp39_body_snapshot(uint32_t context_pointer, const uint8_t *name, size_t name_length,
                              struct hpclang_object **object)
{
    if (object == NULL || name == NULL || name_length == 0u || name_length > HP39_BODY_NAME_MAX) {
        if (object != NULL)
            *object = NULL;
        errno = EINVAL;
        return -1;
    }
    *object = NULL;

    if (!hp39_live_stock_available()) {
        errno = ENOSYS;
        return -1;
    }

    return hpclang_hp39_native_body_snapshot(&hp39_stock_reader, context_pointer, name, name_length,
                                             object);
}

int hpclang_hp39_program_body_snapshot(const uint8_t *name, size_t name_length,
                                       struct hpclang_object **object)
{
    return hp39_body_snapshot(HP39_PGMCXT_POINTER, name, name_length, object);
}

int hpclang_hp39_note_body_snapshot(const uint8_t *name, size_t name_length,
                                    struct hpclang_object **object)
{
    return hp39_body_snapshot(HP39_NOTESCXT_POINTER, name, name_length, object);
}

static struct hpclang_hp39_portal_context *hp39_aplet_context(uint32_t *slot)
{
    const struct hpclang_boot_info *boot = hpclang_boot_info();
    if (!hp39_live_stock_available() ||
        (boot->platform_cookie & (uintptr_t)3u) != HPCLANG_PLATFORM_COOKIE_HP39_TAG) {
        errno = ENOSYS;
        return NULL;
    }
    struct hpclang_hp39_portal_context *portal = (void *)(boot->platform_cookie & ~(uintptr_t)3u);
    if (portal->public_context.magic != HPCLANG_HP39_PLATFORM_MAGIC ||
        portal->public_context.portal_entry == 0u) {
        errno = ENOSYS;
        return NULL;
    }
    if (portal->active != 0u) {
        errno = EBUSY;
        return NULL;
    }
    if (hpclang_hp39_aplet_data_slot(&hp39_stock_reader,
                                     portal->pinned_cells != 0u ? portal->incoming_stack_span : 0u,
                                     slot) != 0)
        return NULL;
    return portal;
}

int hpclang_aplet_data_get(struct hpclang_object **object)
{
    if (object == NULL) {
        errno = EINVAL;
        return -1;
    }
    *object = NULL;
    uint32_t slot, address;
    if (hp39_aplet_context(&slot) == NULL)
        return -1;
    if (!hp39_stock_read_word5(slot, &address)) {
        errno = EIO;
        return -1;
    }
    return hpclang_object_snapshot_copy(&hp39_stock_reader, address, HP39_SATURN_LIMIT - address,
                                        object);
}

int hpclang_aplet_data_set(const struct hpclang_object *object)
{
    if (object == NULL) {
        errno = EINVAL;
        return -1;
    }
    uint32_t slot;
    struct hpclang_hp39_portal_context *portal = hp39_aplet_context(&slot);
    if (portal == NULL)
        return -1;
    if (object->nibble_count < 5u || object->nibble_count > HP39P_STAGED_OBJECT_MAX_NIBBLES) {
        errno = E2BIG;
        return -1;
    }
    const struct hpclang_hp39_object_request request = {
        .object_data = (uintptr_t)object->data,
        .object_nibbles = object->nibble_count,
    };
    uint32_t copy, prologue;
    if (hp39_portal_result(HP39P_COPY_OBJECT, (const uint8_t *)&request, sizeof(request),
                           object->native_prologue, &copy, &prologue) != 0)
        return -1;
    /* Stock allocation may collect and move both the saved frame and values.
     * Re-resolve after the native copy; no Stock call occurs before publication. */
    if (hp39_aplet_context(&slot) == NULL)
        return -1;
    struct hpclang_rpl_stock_memory memory;
    const volatile uint32_t *l1 = (const void *)portal->l1;
    if (!hpclang_rpl_stock_memory_capture(&memory, (const void *)portal->saturn_state, l1[0x80])) {
        errno = EIO;
        return -1;
    }
    for (uint32_t i = 0u; i < 5u; ++i) {
        if (!hpclang_rpl_stock_memory_write(&memory, slot + i, (copy >> (4u * i)) & 15u)) {
            errno = EIO;
            return -1;
        }
    }
    return 0;
}

int hpclang_aplet_data_clear(void)
{
    static const uint8_t empty_list[] = {0x74u, 0x2au, 0xb0u, 0x12u, 0x03u};
    struct hpclang_object *object = malloc(sizeof(*object) + sizeof(empty_list));
    if (object == NULL) {
        errno = ENOMEM;
        return -1;
    }
    object->kind = HPCLANG_OBJECT_LIST;
    object->native_prologue = HP39_RPL_DOLIST;
    object->nibble_count = 10u;
    memcpy(object->data, empty_list, sizeof(empty_list));
    int result = hpclang_aplet_data_set(object);
    hpclang_object_release(object);
    return result;
}
