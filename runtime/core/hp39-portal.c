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

#define HP39_RPL_DOLIST UINT32_C(0x00002a74)
#define HP39_SATURN_LIMIT UINT32_C(0x00100000)

static int hp39_snapshot_read_nibble(
    void *context,
    uint32_t nibble_address,
    uint8_t *value)
{
    (void)context;
    return hpclang_saturn_read_nibble(
        hpclang_saturn_address_make(nibble_address), value);
}

static int hp39_portal_catalog_result(
    uint32_t kind,
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

    int (*entry)(uint32_t *, uint32_t *, uint32_t) =
        (int (*)(uint32_t *, uint32_t *, uint32_t))(uintptr_t)
            context->portal_entry;
    int result = entry(object, prologue, kind);
    if (result == -2) {
        errno = EBUSY;
        return -1;
    }
    if (result != 0) {
        errno = EIO;
        return -1;
    }
    if (*object >= HP39_SATURN_LIMIT ||
        *prologue != HP39_RPL_DOLIST) {
        errno = EILSEQ;
        return -1;
    }
    return 0;
}

int hpclang_hp39_portal_roundtrip(void)
{
    uint32_t object = 0u;
    uint32_t prologue = 0u;
    return hp39_portal_catalog_result(
        HP39P_CATALOG_APLET, &object, &prologue);
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
    if (hp39_portal_catalog_result(
            kind, &address, &prologue) != 0)
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
