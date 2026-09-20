#include <errno.h>
#include <stddef.h>
#include <stdint.h>

#include <hpclang/abi.h>
#include <hpclang/bootstrap.h>
#include <hpclang/object.h>
#include <hpclang/target.h>

#include "object-snapshot.h"
#include "../../platform/armtoolbox/hp39-portal-private.h"
#include "saturn-view.h"

#define HP39_RPL_DOCSTR UINT32_C(0x00002a2c)
#define HP39_RPL_DOLIST UINT32_C(0x00002a74)
#define HP39_SATURN_LIMIT UINT32_C(0x00100000)
#define HP39_BODY_NAME_MAX 19u

static int hp39_snapshot_read_nibble(
    void *context,
    uint32_t nibble_address,
    uint8_t *value)
{
    (void)context;
    return hpclang_saturn_read_nibble(
        hpclang_saturn_address_make(nibble_address), value);
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

    if (name_length > HP39_BODY_NAME_MAX ||
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
    if (result == -2) {
        errno = EBUSY;
        return -1;
    }
    if (result != 0) {
        errno = EIO;
        return -1;
    }
    if (*object >= HP39_SATURN_LIMIT ||
        *prologue != expected_prologue) {
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

static int hp39_catalog_snapshot(
    uint32_t kind,
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
            NULL,
            0u,
            HP39_RPL_DOLIST,
            &address,
            &prologue) != 0)
        return -1;

    struct hpclang_rpl_reader reader = {
        .read_nibble = hp39_snapshot_read_nibble,
        .context = NULL,
    };
    if (hpclang_object_snapshot_copy(
            &reader,
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

int hpclang_hp39_aplet_catalog_snapshot(struct hpclang_object **object)
{
    return hp39_catalog_snapshot(HP39P_CATALOG_APLET, object);
}

int hpclang_hp39_program_catalog_snapshot(struct hpclang_object **object)
{
    return hp39_catalog_snapshot(HP39P_CATALOG_PROGRAM, object);
}

int hpclang_hp39_note_catalog_snapshot(struct hpclang_object **object)
{
    return hp39_catalog_snapshot(HP39P_CATALOG_NOTE, object);
}

static int hp39_body_snapshot(
    uint32_t kind,
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

    uint32_t address = 0u;
    uint32_t prologue = 0u;
    if (hp39_portal_result(
            kind,
            name,
            name_length,
            HP39_RPL_DOCSTR,
            &address,
            &prologue) != 0)
        return -1;

    struct hpclang_rpl_reader reader = {
        .read_nibble = hp39_snapshot_read_nibble,
        .context = NULL,
    };
    if (hpclang_object_snapshot_copy(
            &reader,
            address,
            HP39_SATURN_LIMIT - address,
            object) != 0)
        return -1;

    if (*object == NULL ||
        hpclang_object_kind(*object) != HPCLANG_OBJECT_STRING) {
        hpclang_object_release(*object);
        *object = NULL;
        errno = EILSEQ;
        return -1;
    }
    return 0;
}

int hpclang_hp39_program_body_snapshot(
    const uint8_t *name,
    size_t name_length,
    struct hpclang_object **object)
{
    return hp39_body_snapshot(
        HP39P_BODY_PROGRAM, name, name_length, object);
}

int hpclang_hp39_note_body_snapshot(
    const uint8_t *name,
    size_t name_length,
    struct hpclang_object **object)
{
    return hp39_body_snapshot(
        HP39P_BODY_NOTE, name, name_length, object);
}
