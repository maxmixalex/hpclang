#include <assert.h>
#include "lcd-stability.h"

int main(void)
{
    struct hpclang_lcd_quiescence s = {0};
    /* Initial application screen must not satisfy a return gate. */
    for (int i = 0; i < 8; ++i)
        assert(!hpclang_lcd_observe_sample(&s, 11, 1, 4));
    /* Stock may clear the display and leave it blank while repainting. */
    for (int i = 0; i < 20; ++i)
        assert(!hpclang_lcd_observe_sample(&s, 0, 0, 4));
    for (int i = 0; i < 3; ++i)
        assert(!hpclang_lcd_observe_sample(&s, 22, 1, 4));
    assert(hpclang_lcd_observe_sample(&s, 22, 1, 4));
    /* Another blank interval invalidates an accumulated stability window. */
    assert(!hpclang_lcd_observe_sample(&s, 0, 0, 4));
    for (int i = 0; i < 3; ++i)
        assert(!hpclang_lcd_observe_sample(&s, 22, 1, 4));
    assert(hpclang_lcd_observe_sample(&s, 22, 1, 4));
    return 0;
}
