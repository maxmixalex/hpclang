#include <errno.h>
#include <hpclang/input.h>

int hpclang_text_key_lookup(uint32_t model, uint16_t key, enum hpclang_text_plane plane,
                            uint32_t held_modifiers, uint32_t *codepoint)
{
    if (!codepoint || plane < HPCLANG_TEXT_PLAIN || plane > HPCLANG_TEXT_ALPHA_SYMBOL) {
        errno = EINVAL;
        return -1;
    }
    if (model < HPCLANG_MODEL_HP39GPLUS || model > HPCLANG_MODEL_HP50G) {
        errno = ENOSYS;
        return -1;
    }
    int hp39 = model <= HPCLANG_MODEL_HP40GS;
    uint32_t cp = 0;
    if (plane == HPCLANG_TEXT_ALPHA_UPPER || plane == HPCLANG_TEXT_ALPHA_LOWER) {
        int mapped =
            hpclang_text_alpha_lookup(model, key, plane == HPCLANG_TEXT_ALPHA_LOWER, codepoint);
        if (mapped)
            return mapped;
        if (hp39) {
            if (key == HPCLANG_KEY_0)
                cp = 0x03b8;
            else if (key == HPCLANG_KEY_PERIOD)
                cp = ':';
            else if (key == HPCLANG_KEY_HP39_NEGATE)
                cp = ';';
        } else {
            /* Classic number keys have no Latin legend and remain available
             * while Alpha is locked. */
            return hpclang_text_key_lookup(model, key, HPCLANG_TEXT_PLAIN, 0, codepoint);
        }
    }
    if (plane == HPCLANG_TEXT_ALPHA_SYMBOL && !hp39) {
        /* Independently observed Stock command-line characters, indexed by the
         * printed Latin legend. Values are Unicode, never native HP byte codes.
         * Command-only keys have no scalar. */
        static const uint16_t symbols[26] = {0x03b1, 0x03b2, 0x0394, 0x03b4, 0x03b5, 0x03c1, 0,
                                             0,      '|',    0,      0,      0,      0x03bc, 0x03bb,
                                             '\'',   0x03a0, '^',    0x221a, 0x03c3, 0x03b8, 0x03c4,
                                             0x03c9, '=',    '<',    '>',    '/'};
        uint32_t letter;
        int mapped = hpclang_text_alpha_lookup(model, key, 0, &letter);
        if (mapped < 0)
            return mapped;
        if (mapped && letter >= 'A' && letter <= 'Z')
            cp = symbols[letter - 'A'];
        else {
            switch (key) {
            case HPCLANG_KEY_0:
                cp = 0x2192;
                break;
            case HPCLANG_KEY_1:
                cp = '~';
                break;
            case HPCLANG_KEY_2:
                cp = '!';
                break;
            case HPCLANG_KEY_3:
                cp = '?';
                break;
            case HPCLANG_KEY_5:
                cp = '\\';
                break;
            case HPCLANG_KEY_6:
                cp = 0x2220;
                break;
            case HPCLANG_KEY_MULTIPLY:
                cp = '"';
                break;
            case HPCLANG_KEY_MINUS:
                cp = '_';
                break;
            case HPCLANG_KEY_PLUS:
                cp = 0x00ab;
                break;
            case HPCLANG_KEY_HP48_SPACE:
                if (model == HPCLANG_MODEL_HP48GII)
                    cp = ',';
                break;
            case HPCLANG_KEY_HP4950_SPACE:
                if (model != HPCLANG_MODEL_HP48GII)
                    cp = ',';
                break;
            default:
                break;
            }
        }
        if (held_modifiers & HPCLANG_KEYBOARD_MOD_RIGHT_SHIFT) {
            switch (key) {
            case HPCLANG_KEY_2:
                cp = 0x00a1;
                break;
            case HPCLANG_KEY_3:
                cp = 0x00bf;
                break;
            case HPCLANG_KEY_6:
                cp = 0x00b0;
                break;
            case HPCLANG_KEY_HP48_SPACE:
                if (model == HPCLANG_MODEL_HP48GII)
                    cp = ';';
                break;
            case HPCLANG_KEY_HP4950_SPACE:
                if (model != HPCLANG_MODEL_HP48GII)
                    cp = ';';
                break;
            default:
                break;
            }
        }
    } else if (plane == HPCLANG_TEXT_LEFT_SHIFT && !hp39) {
        switch (key) {
        case HPCLANG_KEY_MULTIPLY:
            cp = '[';
            break;
        case HPCLANG_KEY_MINUS:
            cp = '(';
            break;
        case HPCLANG_KEY_PLUS:
            cp = '{';
            break;
        case HPCLANG_KEY_PERIOD:
            cp = ':';
            break;
        case HPCLANG_KEY_HP48_SPACE:
            if (model == HPCLANG_MODEL_HP48GII)
                cp = 0x03c0;
            break;
        case HPCLANG_KEY_HP4950_SPACE:
            if (model != HPCLANG_MODEL_HP48GII)
                cp = 0x03c0;
            break;
        default:
            break;
        }
    } else if (plane == HPCLANG_TEXT_RIGHT_SHIFT && !hp39) {
        switch (key) {
        case HPCLANG_KEY_MULTIPLY:
            cp = '"';
            break;
        case HPCLANG_KEY_MINUS:
            cp = '_';
            break;
        case HPCLANG_KEY_PLUS:
            cp = 0x00ab;
            break;
        case HPCLANG_KEY_HP48_SPACE:
            if (model == HPCLANG_MODEL_HP48GII)
                cp = held_modifiers & HPCLANG_KEYBOARD_MOD_RIGHT_SHIFT ? ';' : ',';
            break;
        case HPCLANG_KEY_HP4950_SPACE:
            if (model != HPCLANG_MODEL_HP48GII)
                cp = held_modifiers & HPCLANG_KEYBOARD_MOD_RIGHT_SHIFT ? ';' : ',';
            break;
        default:
            break;
        }
    } else if (plane == HPCLANG_TEXT_LEFT_SHIFT && hp39) {
        switch (key) {
        case HPCLANG_KEY_8:
            cp = '{';
            break;
        case HPCLANG_KEY_9:
            cp = '}';
            break;
        case HPCLANG_KEY_5:
            cp = '[';
            break;
        case HPCLANG_KEY_6:
            cp = ']';
            break;
        case HPCLANG_KEY_MULTIPLY:
            cp = '!';
            break;
        case HPCLANG_KEY_MINUS:
            cp = 0x2220;
            break;
        case HPCLANG_KEY_3:
            cp = 0x03c0;
            break;
        case HPCLANG_KEY_PLUS:
            cp = 0x03a3;
            break;
        case HPCLANG_KEY_PERIOD:
            cp = '=';
            break;
        case HPCLANG_KEY_HP39_X_THETA_T:
            cp = 'E';
            break;
        default:
            break;
        }
    } else if (plane == HPCLANG_TEXT_PLAIN) {
        if (key >= HPCLANG_KEY_0 && key <= HPCLANG_KEY_9)
            cp = '0' + key - HPCLANG_KEY_0;
        else {
            switch (key) {
            case HPCLANG_KEY_PERIOD:
                cp = '.';
                break;
            case HPCLANG_KEY_PLUS:
                cp = '+';
                break;
            case HPCLANG_KEY_MINUS:
                cp = '-';
                break;
            case HPCLANG_KEY_MULTIPLY:
                cp = '*';
                break;
            case HPCLANG_KEY_DIVIDE:
                cp = '/';
                break;
            default:
                break;
            }
        }
        if (hp39) {
            switch (key) {
            case HPCLANG_KEY_HP39_LEFT_PAREN:
                cp = '(';
                break;
            case HPCLANG_KEY_HP39_RIGHT_PAREN:
                cp = ')';
                break;
            case HPCLANG_KEY_HP39_COMMA:
                cp = ',';
                break;
            case HPCLANG_KEY_HP39_NEGATE:
                cp = '-';
                break;
            case HPCLANG_KEY_HP39_POWER:
                cp = '^';
                break;
            default:
                break;
            }
        } else if (model == HPCLANG_MODEL_HP48GII) {
            switch (key) {
            case HPCLANG_KEY_HP48_SPACE:
                cp = ' ';
                break;
            case HPCLANG_KEY_HP48_QUOTE:
                cp = '\'';
                break;
            case HPCLANG_KEY_HP48_NEGATE:
                cp = '-';
                break;
            case HPCLANG_KEY_HP48_POWER:
                cp = '^';
                break;
            case HPCLANG_KEY_HP48_EEX:
                cp = 'E';
                break;
            default:
                break;
            }
        } else {
            switch (key) {
            case HPCLANG_KEY_HP4950_SPACE:
                cp = ' ';
                break;
            case HPCLANG_KEY_HP4950_NEGATE:
                cp = '-';
                break;
            case HPCLANG_KEY_HP4950_POWER:
                cp = '^';
                break;
            case HPCLANG_KEY_HP4950_EEX:
                cp = 'E';
                break;
            case HPCLANG_KEY_HP50_QUOTE:
                if (model == HPCLANG_MODEL_HP50G)
                    cp = '\'';
                break;
            default:
                break;
            }
        }
    }
    if (!cp)
        return 0;
    *codepoint = cp;
    return 1;
}
