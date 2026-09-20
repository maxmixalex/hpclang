#include <errno.h>
#include <stddef.h>
#include <stdint.h>

#include <hpclang/abi.h>
#include <hpclang/bootstrap.h>
#include <hpclang/object.h>
#include <hpclang/target.h>

#include "hp39-native-directory.h"
#include "hp39-variable-schema.h"
#include "object-snapshot.h"
#include "../../platform/armtoolbox/hp39-portal-private.h"
#include "saturn-view.h"

#define HP39_RPL_DOLIST UINT32_C(0x00002a74)
#define HP39_RPL_DOCOL UINT32_C(0x00002d9d)
#define HP39_SATURN_LIMIT UINT32_C(0x00100000)
#define HP39_BODY_NAME_MAX 19u
#define HP39_VARIABLE_ID_MAX 255u
#define HP39_PGMCXT_POINTER UINT32_C(0x00080739)
#define HP39_NOTESCXT_POINTER UINT32_C(0x0008073e)

static int hp39_snapshot_read_nibble(
    void *context,
    uint32_t nibble_address,
    uint8_t *value)
{
    (void)context;
    return hpclang_saturn_read_nibble(
        hpclang_saturn_address_make(nibble_address), value);
}

static const struct hpclang_rpl_reader hp39_stock_reader = {
    .read_nibble = hp39_snapshot_read_nibble,
    .context = NULL,
};

static int hp39_live_stock_available(void)
{
    const struct hpclang_boot_info *boot = hpclang_boot_info();
    return boot != NULL &&
        (boot->model == HPCLANG_MODEL_HP39GS ||
         boot->model == HPCLANG_MODEL_HP40GS) &&
        boot->saturn_state != 0u;
}

static int hp39_portal_result(
    uint32_t kind,
    const uint8_t *name,
    size_t name_length,
    uint32_t expected_prologue,
    uint32_t *object,
    uint32_t *prologue)
{
    if (object == NULL || prologue == NULL) {
        errno = EINVAL;
        return -1;
    }
    *object = 0u;
    *prologue = 0u;

    const struct hpclang_boot_info *boot = hpclang_boot_info();
    if (boot == NULL ||
        (boot->model != HPCLANG_MODEL_HP39GS &&
         boot->model != HPCLANG_MODEL_HP40GS) ||
        boot->saturn_state == 0u ||
        (boot->platform_cookie & (uintptr_t)3u) !=
            HPCLANG_PLATFORM_COOKIE_HP39_TAG) {
        errno = ENOSYS;
        return -1;
    }

    const struct hpclang_hp39_platform_context *context =
        (const struct hpclang_hp39_platform_context *)(
            boot->platform_cookie & ~(uintptr_t)3u);
    if (context->magic != HPCLANG_HP39_PLATFORM_MAGIC ||
        context->portal_entry == 0u) {
        errno = ENOSYS;
        return -1;
    }

    size_t maximum_name_length =
        kind == HP39P_VARIABLE_VALUE
            ? HP39_VARIABLE_ID_MAX
            : (kind == HP39P_REFERENCE_OBJECT ||
               kind == HP39P_REFERENCE_EVAL)
                ? 3u
                : HP39_BODY_NAME_MAX;
    if (name_length > maximum_name_length ||
        (name == NULL && name_length != 0u)) {
        errno = EINVAL;
        return -1;
    }

    int (*entry)(
        uint32_t *, uint32_t *, uint32_t, const uint8_t *, uint32_t) =
        (int (*)(
            uint32_t *, uint32_t *, uint32_t,
            const uint8_t *, uint32_t))(uintptr_t)
            context->portal_entry;
    int result = entry(
        object, prologue, kind, name, (uint32_t)name_length);
    if (result == 1) {
        errno = ENOENT;
        return -1;
    }
    if (result == -2) {
        errno = EBUSY;
        return -1;
    }
    if (result != 0) {
        errno = EIO;
        return -1;
    }
    if (*object >= HP39_SATURN_LIMIT ||
        (expected_prologue != 0u &&
         *prologue != expected_prologue)) {
        errno = EILSEQ;
        return -1;
    }
    return 0;
}

int hpclang_hp39_portal_roundtrip(void)
{
    uint32_t object = 0u;
    uint32_t prologue = 0u;
    return hp39_portal_result(
        HP39P_CATALOG_APLET,
        NULL,
        0u,
        HP39_RPL_DOLIST,
        &object,
        &prologue);
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
    if (hp39_portal_result(
            HP39P_VARIABLE_SCHEMA,
            NULL,
            0u,
            HP39_RPL_DOLIST,
            &address,
            &prologue) != 0)
        return -1;

    if (hpclang_object_snapshot_copy(
            &hp39_stock_reader,
            address,
            HP39_SATURN_LIMIT - address,
            object) != 0)
        return -1;
    if (*object == NULL ||
        hpclang_object_kind(*object) != HPCLANG_OBJECT_LIST) {
        hpclang_object_release(*object);
        *object = NULL;
        errno = EILSEQ;
        return -1;
    }
    return 0;
}

static int hp39_variable_reference_snapshot(
    uint32_t kind,
    const uint8_t payload[3],
    uint32_t expected_prologue,
    struct hpclang_object **object)
{
    if (object == NULL) {
        errno = EINVAL;
        return -1;
    }
    *object = NULL;

    uint32_t address = 0u;
    uint32_t prologue = 0u;
    if (hp39_portal_result(
            kind,
            payload,
            3u,
            0u,
            &address,
            &prologue) != 0)
        return -1;

    if (hpclang_object_snapshot_copy(
            &hp39_stock_reader,
            address,
            HP39_SATURN_LIMIT - address,
            object) != 0)
        return -1;
    if (*object == NULL) {
        errno = EILSEQ;
        return -1;
    }

    if ((*object)->native_prologue != expected_prologue) {
        hpclang_object_release(*object);
        *object = NULL;
        errno = EILSEQ;
        return -1;
    }
    return 0;
}

static int hp39_variable_reference_resolve(
    void *context,
    uint32_t reference,
    uint32_t expected_prologue,
    struct hpclang_object **object)
{
    (void)context;
    if (object == NULL ||
        reference == 0u ||
        reference >= HP39_SATURN_LIMIT) {
        if (object != NULL)
            *object = NULL;
        errno = EINVAL;
        return -1;
    }
    *object = NULL;

    uint8_t payload[3] = {
        (uint8_t)(reference & 0xffu),
        (uint8_t)((reference >> 8u) & 0xffu),
        (uint8_t)((reference >> 16u) & 0xffu),
    };

    /*
     * Stage one always materializes the reference as a composite element.
     * Direct data references (for example xNoteText inside the returned
     * {xNoteText} list) therefore stay inert and can be type-checked directly.
     */
    uint32_t address = 0u;
    uint32_t prologue = 0u;
    if (hp39_portal_result(
            HP39P_REFERENCE_OBJECT,
            payload,
            sizeof(payload),
            0u,
            &address,
            &prologue) != 0)
        return -1;

    if (prologue == expected_prologue) {
        if (hpclang_object_snapshot_copy(
                &hp39_stock_reader,
                address,
                HP39_SATURN_LIMIT - address,
                object) != 0)
            return -1;
        if (*object == NULL ||
            (*object)->native_prologue != expected_prologue) {
            hpclang_object_release(*object);
            *object = NULL;
            errno = EILSEQ;
            return -1;
        }
        return 0;
    }

    /*
     * Stock ApletVars permits computed nested schema entries. Real MiniVar
     * uses #3F8FB/#3F900, which materialize as DOCOL handlers such as
     *   :: { xNoteText } TOTEMPOB ;
     * Execute only this explicitly identified program case, then require the
     * caller's exact expected data prologue. Arbitrary referenced IDs/data are
     * never EVALed by this resolver.
     */
    if (prologue != HP39_RPL_DOCOL) {
        errno = EILSEQ;
        return -1;
    }

    return hp39_variable_reference_snapshot(
        HP39P_REFERENCE_EVAL,
        payload,
        expected_prologue,
        object);
}

int hpclang_hp39_variable_value_snapshot(
    const uint8_t *category,
    size_t category_length,
    const uint8_t *name,
    size_t name_length,
    struct hpclang_object **object)
{
    if (object == NULL ||
        category == NULL || category_length == 0u ||
        name == NULL || name_length == 0u ||
        name_length > HP39_VARIABLE_ID_MAX - 3u) {
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
    if (hpclang_hp39_variable_schema_find_resolved(
            schema,
            category,
            category_length,
            name,
            name_length,
            hp39_variable_reference_resolve,
            NULL,
            &match) != 0) {
        hpclang_object_release(schema);
        return -1;
    }
    hpclang_object_release(schema);

    uint32_t payload_length = (uint32_t)name_length + 3u;
    uint8_t payload[HP39_VARIABLE_ID_MAX];
    for (size_t i = 0u; i < name_length; ++i)
        payload[i] = name[i];
    payload[name_length] =
        (uint8_t)(match.descriptor_address & 0xffu);
    payload[name_length + 1u] =
        (uint8_t)((match.descriptor_address >> 8u) & 0xffu);
    payload[name_length + 2u] =
        (uint8_t)((match.descriptor_address >> 16u) & 0xffu);

    uint32_t address = 0u;
    uint32_t prologue = 0u;
    if (hp39_portal_result(
            HP39P_VARIABLE_VALUE,
            payload,
            payload_length,
            0u,
            &address,
            &prologue) != 0)
        return -1;

    if (hpclang_object_snapshot_copy(
            &hp39_stock_reader,
            address,
            HP39_SATURN_LIMIT - address,
            object) != 0)
        return -1;
    if (*object == NULL) {
        errno = EILSEQ;
        return -1;
    }
    return 0;
}

int hpclang_hp39_aplet_catalog_snapshot(struct hpclang_object **object)
{
    if (!hp39_live_stock_available()) {
        if (object != NULL)
            *object = NULL;
        errno = ENOSYS;
        return -1;
    }
    return hpclang_hp39_native_aplet_catalog_snapshot(
        &hp39_stock_reader, object);
}

static int hp39_native_catalog_snapshot(
    uint32_t context_pointer,
    struct hpclang_object **object)
{
    if (!hp39_live_stock_available()) {
        if (object != NULL)
            *object = NULL;
        errno = ENOSYS;
        return -1;
    }
    return hpclang_hp39_native_catalog_snapshot(
        &hp39_stock_reader, context_pointer, object);
}

int hpclang_hp39_program_catalog_snapshot(struct hpclang_object **object)
{
    return hp39_native_catalog_snapshot(HP39_PGMCXT_POINTER, object);
}

int hpclang_hp39_note_catalog_snapshot(struct hpclang_object **object)
{
    return hp39_native_catalog_snapshot(HP39_NOTESCXT_POINTER, object);
}

static int hp39_body_snapshot(
    uint32_t context_pointer,
    const uint8_t *name,
    size_t name_length,
    struct hpclang_object **object)
{
    if (object == NULL ||
        name == NULL ||
        name_length == 0u ||
        name_length > HP39_BODY_NAME_MAX) {
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

    return hpclang_hp39_native_body_snapshot(
        &hp39_stock_reader,
        context_pointer,
        name,
        name_length,
        object);
}

int hpclang_hp39_program_body_snapshot(
    const uint8_t *name,
    size_t name_length,
    struct hpclang_object **object)
{
    return hp39_body_snapshot(
        HP39_PGMCXT_POINTER, name, name_length, object);
}

int hpclang_hp39_note_body_snapshot(
    const uint8_t *name,
    size_t name_length,
    struct hpclang_object **object)
{
    return hp39_body_snapshot(
        HP39_NOTESCXT_POINTER, name, name_length, object);
}
