#ifndef HPCLANG_WINKEYS_H
#define HPCLANG_WINKEYS_H
#include <winevent.h>
#define WIN_KEY_PRESSED 1
#define WIN_KEY_RELEASED 2
#define WIN_KEY_TYPED 3
#define WIN_ALPHA_CHANGED 4
#define WIN_LSHIFT_CHANGED 5
#define WIN_RSHIFT_CHANGED 6
#define WIN_KEYSTATE_OFF 0
#define WIN_KEYSTATE_ON 1
#define WIN_KEYSTATE_LOCK 2
#define WIN_KEYSTATE_PRESSED 3

/* Historical hpkeyb49.h numeric semantics on every model, including HP39:
 * these character tables do not reinterpret model-specific key legends.
 * Press/release payloads encode key 0..63 in (void *)(uintptr_t)key;
 * modifier payloads encode a WIN_KEYSTATE_* value in the same way.
 * Typed payloads below are borrowed only during synchronous dispatch.
 * The default press/release handlers have priority 2000000000. A handler
 * running before them can consume physical events and suppress translation.
 * Modifier changes synchronously post repaint event 7, as in HPGCC; they
 * do not begin/present an HPG frame or draw built-in modifier indicators.
 */
typedef struct
{
    unsigned keycode : 8;
    char ascii;
    unsigned lshift : 2;
    unsigned rshift : 2;
    unsigned alpha : 2;
} win_keytyped_t;
#endif
