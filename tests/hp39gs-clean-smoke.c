#include <stdint.h>

volatile uint32_t hp39_smoke_counter = 0x39u;

int main(void)
{
    hp39_smoke_counter += 1u;
    return (int)hp39_smoke_counter;
}
