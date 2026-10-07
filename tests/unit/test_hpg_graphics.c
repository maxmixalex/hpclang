#include <assert.h>
struct hpclang_surface;
unsigned hpclang_hpg_graphics_witness(struct hpclang_surface *destination);
int main(void)
{
    assert(hpclang_hpg_graphics_witness(0) == 0);
    return 0;
}
