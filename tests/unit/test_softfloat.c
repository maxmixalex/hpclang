#include <assert.h>
#include <math.h>
#include <stdint.h>

double hpclang_soft_dmul(double, double);
double hpclang_soft_dadd(double, double);
double hpclang_soft_dsub(double, double);
int hpclang_soft_dcmpeq(double, double);
int hpclang_soft_dcmplt(double, double);
int hpclang_soft_dcmple(double, double);
int hpclang_soft_dcmpge(double, double);
int hpclang_soft_dcmpgt(double, double);
int hpclang_soft_dcmpun(double, double);

static uint64_t bits(double x)
{
    union
    {
        double d;
        uint64_t u;
    } r = {.d = x};
    return r.u;
}

int main(void)
{
    assert(bits(hpclang_soft_dmul(1.25, 3.5)) == bits(4.375));
    assert(bits(hpclang_soft_dadd(4.375, 0.5)) == bits(4.875));
    assert(bits(hpclang_soft_dsub(4.875, 0.125)) == bits(4.75));

    assert(hpclang_soft_dcmpeq(4.875, 4.875));
    assert(hpclang_soft_dcmplt(4.87, 4.875));
    assert(hpclang_soft_dcmple(4.875, 4.875));
    assert(hpclang_soft_dcmpge(4.875, 4.875));
    assert(hpclang_soft_dcmpgt(4.875, 4.87));

    const double nan = NAN;
    assert(hpclang_soft_dcmpun(nan, 1.0));
    assert(!hpclang_soft_dcmpeq(nan, nan));
    assert(!hpclang_soft_dcmplt(nan, 1.0));
    assert(!hpclang_soft_dcmpgt(nan, 1.0));

    return 0;
}
