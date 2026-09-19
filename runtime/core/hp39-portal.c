#include <errno.h>
#include <stdint.h>

#include <hpclang/abi.h>
#include <hpclang/bootstrap.h>
#include <hpclang/target.h>

int hpclang_hp39_portal_roundtrip(void)
{
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

    int (*entry)(void) =
        (int (*)(void))(uintptr_t)context->portal_entry;
    int result = entry();
    if (result == -2) {
        errno = EBUSY;
        return -1;
    }
    if (result != 0) {
        errno = EIO;
        return -1;
    }
    return 0;
}
