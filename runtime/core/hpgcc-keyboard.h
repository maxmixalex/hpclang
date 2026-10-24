#ifndef HPCLANG_HPGCC_KEYBOARD_INTERNAL_H
#define HPCLANG_HPGCC_KEYBOARD_INTERNAL_H
#include <hpkeyb49.h>
/* Checked counterpart to the historical void API. Output is unchanged on
 * failure, and success preserves errno. Uses the same physical matrix map. */
int hpclang_hpgcc_keymatrix_read(keymatrix *);
#endif
