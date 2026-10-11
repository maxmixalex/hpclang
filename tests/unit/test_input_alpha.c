#include <assert.h>
#include <errno.h>
#include <hpclang/input.h>
#include "keyboard-key-map.h"

int main(void)
{
    /* Physical positions from the manual diagrams, independent of public key
     * identities. The accepted electrical map connects these to model keys. */
    static const unsigned char hp39_scans[] = {28, 21, 14, 7,  29, 22, 15, 8,  1,  30, 23, 16, 9,
                                               2,  57, 24, 17, 10, 3,  25, 18, 11, 4,  26, 19, 12};
    static const unsigned char classic_scans[] = {35, 36, 37, 38, 39, 40, 41, 46, 47,
                                                  48, 49, 50, 28, 21, 14, 7,  29, 22,
                                                  15, 8,  1,  30, 23, 16, 9,  2};
    for (uint32_t model = HPCLANG_MODEL_HP39GPLUS; model <= HPCLANG_MODEL_HP50G; ++model) {
        const unsigned char *scans = model <= HPCLANG_MODEL_HP40GS ? hp39_scans : classic_scans;
        for (unsigned i = 0; i < 26; ++i) {
            uint16_t key = hpclang_keyboard_key_from_scan(model, scans[i]);
            uint32_t cp = 0;
            errno = EDOM;
            assert(hpclang_text_alpha_lookup(model, key, 0, &cp) == 1);
            assert(cp == 'A' + i && errno == EDOM);
            assert(hpclang_text_alpha_lookup(model, key, 1, &cp) == 1);
            assert(cp == 'a' + i && errno == EDOM);
        }
        uint16_t space =
            hpclang_keyboard_key_from_scan(model, model <= HPCLANG_MODEL_HP40GS ? 5 : 13);
        uint32_t cp = 99;
        assert(hpclang_text_alpha_lookup(model, space, 0, &cp) == 1 && cp == ' ');
        cp = 1234;
        assert(hpclang_text_alpha_lookup(model, HPCLANG_KEY_UP, 0, &cp) == 0 && cp == 1234);
        assert(hpclang_text_alpha_lookup(model, HPCLANG_KEY_UNKNOWN, 0, &cp) == 0 && cp == 1234);
        if (model <= HPCLANG_MODEL_HP40GS) {
            /* Stock HOME: Alpha+2 is Y; Alpha+Plus is space. Bottom-row
             * punctuation and Enter are not Latin letters or Alpha-space. */
            assert(hpclang_text_alpha_lookup(model, HPCLANG_KEY_2, 0, &cp) == 1 && cp == 'Y');
            assert(hpclang_text_alpha_lookup(model, HPCLANG_KEY_ENTER, 0, &cp) == 0 && cp == 'Y');
            assert(hpclang_text_alpha_lookup(model, HPCLANG_KEY_PERIOD, 0, &cp) == 0 && cp == 'Y');
            assert(hpclang_text_alpha_lookup(model, HPCLANG_KEY_HP39_DEL, 0, &cp) == 0 &&
                   cp == 'Y');
        }
    }
    uint32_t cp = 987;
    assert(hpclang_text_alpha_lookup(0, HPCLANG_KEY_F1, 0, &cp) == -1 && errno == ENOSYS &&
           cp == 987);
    assert(hpclang_text_alpha_lookup(4, HPCLANG_KEY_F1, 2, &cp) == -1 && errno == EINVAL &&
           cp == 987);
    assert(hpclang_text_alpha_lookup(4, HPCLANG_KEY_F1, 0, NULL) == -1 && errno == EINVAL);
    assert(hpclang_text_alpha_lookup(5, HPCLANG_KEY_HP50_EVAL, 0, &cp) == 0 && cp == 987);
    assert(hpclang_text_alpha_lookup(6, HPCLANG_KEY_HP49_EQW, 0, &cp) == 0 && cp == 987);
    assert(hpclang_text_alpha_lookup(1, HPCLANG_KEY_F1, 0, &cp) == 0 && cp == 987);
    assert(hpclang_text_alpha_lookup(4, HPCLANG_KEY_HP39_SIN, 0, &cp) == 0 && cp == 987);
    return 0;
}
