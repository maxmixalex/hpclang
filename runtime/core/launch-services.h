#ifndef HPCLANG_RUNTIME_LAUNCH_SERVICES_H
#define HPCLANG_RUNTIME_LAUNCH_SERVICES_H

#include <stddef.h>
#include <hpclang/abi.h>
#include <hpclang/bootstrap.h>
#include <hpclang/target.h>

/* Inspect launcher-owned RAM only. These checks neither access hardware nor
 * reserve resources; the service must still validate its live memory mapping. */
static inline const struct hpclang_rpl_memory_bounds *
hpclang_launch_rpl_bounds(const struct hpclang_boot_info *boot)
{
    if (boot == NULL || boot->saturn_state == 0u ||
        (boot->model != HPCLANG_MODEL_HP48GII && boot->model != HPCLANG_MODEL_HP49GPLUS &&
         boot->model != HPCLANG_MODEL_HP50G) ||
        (boot->platform_cookie & HPCLANG_PLATFORM_COOKIE_RPL_BOUNDS_TAG) == 0u)
        return NULL;
    uintptr_t raw = boot->platform_cookie & ~HPCLANG_PLATFORM_COOKIE_RPL_BOUNDS_TAG;
    if (raw == 0u || raw % _Alignof(struct hpclang_rpl_memory_bounds) != 0u)
        return NULL;
    const struct hpclang_rpl_memory_bounds *bounds = (const void *)raw;
    if ((bounds->magic != HPCLANG_RPL_BOUNDS_MAGIC &&
         bounds->magic != HPCLANG_RPL_BOUNDS_MAGIC_V2 &&
         bounds->magic != HPCLANG_RPL_BOUNDS_MAGIC_V4) ||
        bounds->temp_limit <= 0x80000u || bounds->temp_limit > 0x100000u ||
        bounds->user_limit < 0x80000u || bounds->user_limit >= 0x100000u ||
        bounds->stack_reserve_nibbles > 0x100000u - bounds->user_limit ||
        bounds->stack_reserve_nibbles % 5u != 0u)
        return NULL;
    return bounds;
}

/* Only RPL4 owns the extension. Never read beyond an older launch prefix. */
static inline const uint32_t *
hpclang_launch_native_root_masks(const struct hpclang_rpl_memory_bounds *bounds)
{
    return bounds && bounds->magic == HPCLANG_RPL_BOUNDS_MAGIC_V4 ? bounds->native_root_masks
                                                                  : NULL;
}

static inline uint32_t hpclang_launch_port1_limit(const struct hpclang_boot_info *boot)
{
    const struct hpclang_rpl_memory_bounds *bounds = hpclang_launch_rpl_bounds(boot);
    if (!bounds ||
        (bounds->magic != HPCLANG_RPL_BOUNDS_MAGIC &&
         bounds->magic != HPCLANG_RPL_BOUNDS_MAGIC_V4) ||
        (boot->model != HPCLANG_MODEL_HP49GPLUS && boot->model != HPCLANG_MODEL_HP50G) ||
        bounds->port1_limit > 0x40000u)
        return 0;
    return bounds->port1_limit;
}

static inline const struct hpclang_hp39_platform_context *
hpclang_launch_hp39_context(const struct hpclang_boot_info *boot)
{
    if (boot == NULL || boot->saturn_state == 0u ||
        (boot->model != HPCLANG_MODEL_HP39GPLUS && boot->model != HPCLANG_MODEL_HP39GS &&
         boot->model != HPCLANG_MODEL_HP40GS) ||
        (boot->platform_cookie & (uintptr_t)3u) != HPCLANG_PLATFORM_COOKIE_HP39_TAG)
        return NULL;
    uintptr_t raw = boot->platform_cookie & ~(uintptr_t)3u;
    if (raw == 0u || raw % _Alignof(struct hpclang_hp39_platform_context) != 0u)
        return NULL;
    const struct hpclang_hp39_platform_context *context = (const void *)raw;
    if (context->magic != HPCLANG_HP39_PLATFORM_MAGIC || context->portal_entry == 0u)
        return NULL;
    return context;
}

#endif
