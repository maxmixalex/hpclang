#ifndef HPCLANG_TARGET_H
#define HPCLANG_TARGET_H

#ifdef __cplusplus
extern "C" {
#endif

enum hpclang_model {
    HPCLANG_MODEL_UNKNOWN = 0,
    HPCLANG_MODEL_HP39GPLUS,
    HPCLANG_MODEL_HP39GS,
    HPCLANG_MODEL_HP40GS,
    HPCLANG_MODEL_HP48GII,
    HPCLANG_MODEL_HP49GPLUS,
    HPCLANG_MODEL_HP50G
};

enum hpclang_family {
    HPCLANG_FAMILY_UNKNOWN = 0,
    HPCLANG_FAMILY_HP39,
    HPCLANG_FAMILY_HP48,
    HPCLANG_FAMILY_HP49
};

enum hpclang_capability {
    HPCLANG_CAP_L3_SIMPLE          = 1u << 0,
    HPCLANG_CAP_MMU_PACKER        = 1u << 1,
    HPCLANG_CAP_OS_RAM            = 1u << 2,
    HPCLANG_CAP_MULTI_ENTRY       = 1u << 3,
    HPCLANG_CAP_ERAM              = 1u << 4,
    HPCLANG_CAP_L3_VIRTUAL        = 1u << 5,
    HPCLANG_CAP_ARM_THUMB_INTERWORK = 1u << 6,
    HPCLANG_CAP_ROMLIB            = 1u << 7
};

/*
 * These identifiers are intentionally capability-oriented.  No common source
 * should infer a feature merely from a model number when a capability query
 * can express the requirement directly.
 *
 * The runtime descriptor ABI is not frozen yet; this header only fixes names
 * used by research, build profiles and future public headers.
 */

#ifdef __cplusplus
}
#endif

#endif
