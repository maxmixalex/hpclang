#ifndef HPCLANG_TARGET_H
#define HPCLANG_TARGET_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Physical calculator identity.
 *
 * Execution features are not represented as historical launcher capabilities:
 * every supported target implements the same hpclang v1 application ABI.
 */
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

#ifdef __cplusplus
}
#endif

#endif
