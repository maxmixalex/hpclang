#ifndef HPCLANG_TARGET_H
#define HPCLANG_TARGET_H

#ifdef __cplusplus
extern "C" {
#endif

/* Physical calculator identity. */
enum hpclang_model {
    HPCLANG_MODEL_UNKNOWN = 0,
    HPCLANG_MODEL_HP39GPLUS = 1,
    HPCLANG_MODEL_HP39GS = 2,
    HPCLANG_MODEL_HP40GS = 3,
    HPCLANG_MODEL_HP48GII = 4,
    HPCLANG_MODEL_HP49GPLUS = 5,
    HPCLANG_MODEL_HP50G = 6
};

/* Platform families share launch, packaging and low-level integration. */
enum hpclang_family {
    HPCLANG_FAMILY_UNKNOWN = 0,
    HPCLANG_FAMILY_HP39 = 1,
    HPCLANG_FAMILY_HP48 = 2,
    HPCLANG_FAMILY_HP4950 = 3
};

#ifdef __cplusplus
}
#endif

#endif
