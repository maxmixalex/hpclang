#include <hpclang/abi.h>
#include <hpclang/device.h>

#define DEVICE(model_, family_, width_, height_) \
    { \
        sizeof(struct hpclang_device_info), \
        HPCLANG_DEVICE_INFO_VERSION, \
        (model_), \
        (family_), \
        (width_), \
        (height_) \
    }

static const struct hpclang_device_info unknown =
    DEVICE(HPCLANG_MODEL_UNKNOWN, HPCLANG_FAMILY_UNKNOWN, 0u, 0u);

static const struct hpclang_device_info devices[] = {
    DEVICE(HPCLANG_MODEL_UNKNOWN,  HPCLANG_FAMILY_UNKNOWN, 0u,   0u),
    DEVICE(HPCLANG_MODEL_HP39GPLUS, HPCLANG_FAMILY_HP39,   131u, 64u),
    DEVICE(HPCLANG_MODEL_HP39GS,     HPCLANG_FAMILY_HP39,   131u, 64u),
    DEVICE(HPCLANG_MODEL_HP40GS,     HPCLANG_FAMILY_HP39,   131u, 64u),
    DEVICE(HPCLANG_MODEL_HP48GII,    HPCLANG_FAMILY_HP48,   131u, 64u),
    DEVICE(HPCLANG_MODEL_HP49GPLUS,  HPCLANG_FAMILY_HP4950, 131u, 80u),
    DEVICE(HPCLANG_MODEL_HP50G,      HPCLANG_FAMILY_HP4950, 131u, 80u),
};

const struct hpclang_device_info *hpclang_device_info(void)
{
    const struct hpclang_boot_info *boot = hpclang_boot_info();
    uint32_t model = boot != 0 ? boot->model : HPCLANG_MODEL_UNKNOWN;

    if (model >= sizeof(devices) / sizeof(devices[0]))
        return &unknown;

    return &devices[model];
}
