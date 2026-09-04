#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

volatile uint32_t hpclang_u32_a = 1000003u;
volatile uint32_t hpclang_u32_b = 97u;
volatile uint64_t hpclang_u64_a = UINT64_C(0x123456789abcdef0);
volatile uint64_t hpclang_u64_b = UINT64_C(1000003);
volatile double hpclang_double_a = 1.25;
volatile double hpclang_double_b = 3.5;

int main(void)
{
    uint32_t q32 = hpclang_u32_a / hpclang_u32_b;
    uint64_t q64 = hpclang_u64_a / hpclang_u64_b;
    double fp = hpclang_double_a * hpclang_double_b + 0.5;

    char src[] = "hpclang";
    char copy[16];
    memcpy(copy, src, sizeof(src));

    char *heap = malloc(24);
    if (heap)
        memcpy(heap, copy, sizeof(src));

    char formatted[64];
    int n = snprintf(
        formatted,
        sizeof(formatted),
        "%s:%lu:%llu",
        copy,
        (unsigned long)q32,
        (unsigned long long)q64
    );

    return (n > 0 && strlen(copy) == 7 && fp > 4.0) ? 0 : 1;
}
