#include <errno.h>
#include <hpclang/abi.h>
#include <hpclang/bootstrap.h>
#include <hpclang/device.h>
#include "launch-services.h"

#define DEVICE(model_, family_, width_, height_)                                                   \
    {sizeof(struct hpclang_device_info),                                                           \
     HPCLANG_DEVICE_INFO_VERSION,                                                                  \
     (model_),                                                                                     \
     (family_),                                                                                    \
     (width_),                                                                                     \
     (height_)}

#ifndef HPCLANG_COMPILED_MODEL
static const struct hpclang_device_info unknown =
    DEVICE(HPCLANG_MODEL_UNKNOWN, HPCLANG_FAMILY_UNKNOWN, 0u, 0u);
#endif

static const struct hpclang_device_info devices[] = {
    DEVICE(HPCLANG_MODEL_UNKNOWN, HPCLANG_FAMILY_UNKNOWN, 0u, 0u),
    DEVICE(HPCLANG_MODEL_HP39GPLUS, HPCLANG_FAMILY_HP39, 131u, 64u),
    DEVICE(HPCLANG_MODEL_HP39GS, HPCLANG_FAMILY_HP39, 131u, 64u),
    DEVICE(HPCLANG_MODEL_HP40GS, HPCLANG_FAMILY_HP39, 131u, 64u),
    DEVICE(HPCLANG_MODEL_HP48GII, HPCLANG_FAMILY_HP48, 131u, 64u),
    DEVICE(HPCLANG_MODEL_HP49GPLUS, HPCLANG_FAMILY_HP4950, 131u, 80u),
    DEVICE(HPCLANG_MODEL_HP50G, HPCLANG_FAMILY_HP4950, 131u, 80u),
};

const struct hpclang_device_info *hpclang_device_info(void)
{
#ifdef HPCLANG_COMPILED_MODEL
    return &devices[HPCLANG_COMPILED_MODEL];
#else
    const struct hpclang_boot_info *boot = hpclang_boot_info();
    uint32_t model = boot != 0 ? boot->model : HPCLANG_MODEL_UNKNOWN;

    if (model >= sizeof(devices) / sizeof(devices[0]))
        return &unknown;

    return &devices[model];
#endif
}

int hpclang_device_capability(enum hpclang_capability capability)
{
    uint32_t family = hpclang_device_info()->family;
    switch (capability) {
    case HPCLANG_CAPABILITY_DISPLAY:
    case HPCLANG_CAPABILITY_DISPLAY_MODES:
    case HPCLANG_CAPABILITY_DISPLAY_INDICATORS:
    case HPCLANG_CAPABILITY_KEYBOARD:
    case HPCLANG_CAPABILITY_MONOTONIC_TIME:
    case HPCLANG_CAPABILITY_CIVIL_TIME_READ:
    case HPCLANG_CAPABILITY_POLL_TIMERS:
    case HPCLANG_CAPABILITY_SOUND:
    case HPCLANG_CAPABILITY_USB_POWER_DETECT:
    case HPCLANG_CAPABILITY_CIVIL_TIME_SET:
    case HPCLANG_CAPABILITY_CONTRAST_READ:
    case HPCLANG_CAPABILITY_CONTRAST_SET:
    case HPCLANG_CAPABILITY_POWER_PROFILE:
        return family != HPCLANG_FAMILY_UNKNOWN ? HPCLANG_CAPABILITY_SUPPORTED
                                                : HPCLANG_CAPABILITY_UNSUPPORTED;
    case HPCLANG_CAPABILITY_STREAM_STORAGE:
        return family == HPCLANG_FAMILY_HP4950 ? HPCLANG_CAPABILITY_SUPPORTED
                                               : HPCLANG_CAPABILITY_UNSUPPORTED;
    case HPCLANG_CAPABILITY_RPL_STACK:
    case HPCLANG_CAPABILITY_RPL_OBJECT_STORE: {
        if (family != HPCLANG_FAMILY_HP48 && family != HPCLANG_FAMILY_HP4950)
            return HPCLANG_CAPABILITY_UNSUPPORTED;
        const struct hpclang_boot_info *boot = hpclang_boot_info();
        return boot && boot->saturn_state ? HPCLANG_CAPABILITY_SUPPORTED
                                          : HPCLANG_CAPABILITY_CONTEXT_UNAVAILABLE;
    }
    case HPCLANG_CAPABILITY_RPL_STACK_MUTATION:
    case HPCLANG_CAPABILITY_RPL_OBJECT_STORE_MUTATION:
        if (family != HPCLANG_FAMILY_HP48 && family != HPCLANG_FAMILY_HP4950)
            return HPCLANG_CAPABILITY_UNSUPPORTED;
        return hpclang_launch_rpl_bounds(hpclang_boot_info()) != NULL
                   ? HPCLANG_CAPABILITY_SUPPORTED
                   : HPCLANG_CAPABILITY_CONTEXT_UNAVAILABLE;
    case HPCLANG_CAPABILITY_APLET_DATA:
    case HPCLANG_CAPABILITY_APLET_VARIABLES:
    case HPCLANG_CAPABILITY_TEXT_CATALOG:
    case HPCLANG_CAPABILITY_APLET_CATALOG: {
        if (family != HPCLANG_FAMILY_HP39)
            return HPCLANG_CAPABILITY_UNSUPPORTED;
        return hpclang_launch_hp39_context(hpclang_boot_info()) != NULL
                   ? HPCLANG_CAPABILITY_SUPPORTED
                   : HPCLANG_CAPABILITY_CONTEXT_UNAVAILABLE;
    }
    default:
        errno = EINVAL;
        return -1;
    }
}
