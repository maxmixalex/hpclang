#include <errno.h>
#include <hpclang/input.h>

/* Printed Latin legends, in alphabet order. Source keyboard diagrams are
 * cited in hpgcc-compat.md. These are text-layer maps, not scan-code maps. */
static const uint16_t hp39[] = {HPCLANG_KEY_HP39_VARS,
                                HPCLANG_KEY_HP39_MATH,
                                HPCLANG_KEY_HP39_DERIVATIVE,
                                HPCLANG_KEY_HP39_X_THETA_T,
                                HPCLANG_KEY_HP39_SIN,
                                HPCLANG_KEY_HP39_COS,
                                HPCLANG_KEY_HP39_TAN,
                                HPCLANG_KEY_HP39_LN,
                                HPCLANG_KEY_HP39_LOG,
                                HPCLANG_KEY_HP39_SQUARE,
                                HPCLANG_KEY_HP39_POWER,
                                HPCLANG_KEY_HP39_LEFT_PAREN,
                                HPCLANG_KEY_HP39_RIGHT_PAREN,
                                HPCLANG_KEY_DIVIDE,
                                HPCLANG_KEY_HP39_COMMA,
                                HPCLANG_KEY_7,
                                HPCLANG_KEY_8,
                                HPCLANG_KEY_9,
                                HPCLANG_KEY_MULTIPLY,
                                HPCLANG_KEY_4,
                                HPCLANG_KEY_5,
                                HPCLANG_KEY_6,
                                HPCLANG_KEY_MINUS,
                                HPCLANG_KEY_1,
                                HPCLANG_KEY_2,
                                HPCLANG_KEY_3};
static const uint16_t hp48[] = {HPCLANG_KEY_F1,
                                HPCLANG_KEY_F2,
                                HPCLANG_KEY_F3,
                                HPCLANG_KEY_F4,
                                HPCLANG_KEY_F5,
                                HPCLANG_KEY_F6,
                                HPCLANG_KEY_HP48_APPS,
                                HPCLANG_KEY_HP48_MODE,
                                HPCLANG_KEY_HP48_TOOL,
                                HPCLANG_KEY_HP48_VAR,
                                HPCLANG_KEY_HP48_STO,
                                HPCLANG_KEY_HP48_NXT,
                                HPCLANG_KEY_HP48_HIST,
                                HPCLANG_KEY_HP48_EVAL,
                                HPCLANG_KEY_HP48_QUOTE,
                                HPCLANG_KEY_HP48_SYMB,
                                HPCLANG_KEY_HP48_POWER,
                                HPCLANG_KEY_HP48_SQRT,
                                HPCLANG_KEY_HP48_SIN,
                                HPCLANG_KEY_HP48_COS,
                                HPCLANG_KEY_HP48_TAN,
                                HPCLANG_KEY_HP48_EEX,
                                HPCLANG_KEY_HP48_NEGATE,
                                HPCLANG_KEY_HP48_X,
                                HPCLANG_KEY_HP48_RECIPROCAL,
                                HPCLANG_KEY_DIVIDE};
static const uint16_t hp4950[] = {HPCLANG_KEY_F1,
                                  HPCLANG_KEY_F2,
                                  HPCLANG_KEY_F3,
                                  HPCLANG_KEY_F4,
                                  HPCLANG_KEY_F5,
                                  HPCLANG_KEY_F6,
                                  HPCLANG_KEY_HP4950_APPS,
                                  HPCLANG_KEY_HP4950_MODE,
                                  HPCLANG_KEY_HP4950_TOOL,
                                  HPCLANG_KEY_HP4950_VAR,
                                  HPCLANG_KEY_HP4950_STO,
                                  HPCLANG_KEY_HP4950_NXT,
                                  HPCLANG_KEY_HP4950_HIST,
                                  HPCLANG_KEY_UNKNOWN,
                                  HPCLANG_KEY_UNKNOWN,
                                  HPCLANG_KEY_HP4950_SYMB,
                                  HPCLANG_KEY_HP4950_POWER,
                                  HPCLANG_KEY_HP4950_SQRT,
                                  HPCLANG_KEY_HP4950_SIN,
                                  HPCLANG_KEY_HP4950_COS,
                                  HPCLANG_KEY_HP4950_TAN,
                                  HPCLANG_KEY_HP4950_EEX,
                                  HPCLANG_KEY_HP4950_NEGATE,
                                  HPCLANG_KEY_HP4950_X,
                                  HPCLANG_KEY_HP4950_RECIPROCAL,
                                  HPCLANG_KEY_DIVIDE};
int hpclang_text_alpha_lookup(uint32_t model, uint16_t key, int lowercase, uint32_t *codepoint)
{
    if (!codepoint || (lowercase != 0 && lowercase != 1)) {
        errno = EINVAL;
        return -1;
    }
    const uint16_t *letters;
    uint16_t space;
    switch (model) {
    case HPCLANG_MODEL_HP39GPLUS:
    case HPCLANG_MODEL_HP39GS:
    case HPCLANG_MODEL_HP40GS:
        letters = hp39;
        space = HPCLANG_KEY_PLUS;
        break;
    case HPCLANG_MODEL_HP48GII:
        letters = hp48;
        space = HPCLANG_KEY_HP48_SPACE;
        break;
    case HPCLANG_MODEL_HP49GPLUS:
    case HPCLANG_MODEL_HP50G:
        letters = hp4950;
        space = HPCLANG_KEY_HP4950_SPACE;
        break;
    default:
        errno = ENOSYS;
        return -1;
    }
    if (key == HPCLANG_KEY_UNKNOWN)
        return 0;
    uint32_t base = lowercase ? 'a' : 'A';
    if ((model == HPCLANG_MODEL_HP49GPLUS && key == HPCLANG_KEY_HP49_CAT) ||
        (model == HPCLANG_MODEL_HP50G && key == HPCLANG_KEY_HP50_EVAL)) {
        *codepoint = base + 13;
        return 1;
    }
    if ((model == HPCLANG_MODEL_HP49GPLUS && key == HPCLANG_KEY_HP49_EQW) ||
        (model == HPCLANG_MODEL_HP50G && key == HPCLANG_KEY_HP50_QUOTE)) {
        *codepoint = base + 14;
        return 1;
    }
    if (key == space) {
        *codepoint = ' ';
        return 1;
    }
    for (unsigned i = 0; i < 26; ++i)
        if (key == letters[i]) {
            *codepoint = base + i;
            return 1;
        }
    return 0;
}
