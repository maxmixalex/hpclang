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
    HPCLANG_MODEL_HP39GPLUS = 1,
    HPCLANG_MODEL_HP39GS = 2,
    HPCLANG_MODEL_HP40GS = 3,
    HPCLANG_MODEL_HP48GII = 4,
    HPCLANG_MODEL_HP49GPLUS = 5,
    HPCLANG_MODEL_HP50G = 6
};

enum hpclang_family {
    HPCLANG_FAMILY_UNKNOWN = 0,
    HPCLANG_FAMILY_HP39 = 1,
    HPCLANG_FAMILY_HP48 = 2,
    HPCLANG_FAMILY_HP49 = 3
};

#ifdef __cplusplus
}
#endif

#endif
