#define HPCLANG_HPGCC_LEGACY_FORMAT 1
#include <hpstdio.h>
#include <cstring>

extern "C" unsigned legacy_cpp_formats(void)
{
    char text[32];
    if (sprintf(text, "%.4f", 1.25) != 6 || std::strcmp(text, "1,2500"))
        return 1;
    if ((std::snprintf)(text, sizeof(text), "%f", 1.25) != 8 || std::strcmp(text, "1.250000"))
        return 2;
    return get_decimal_digits() != 2;
}
