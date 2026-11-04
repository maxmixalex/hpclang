#include <hpstdio.h>
#include <cstring>

extern "C" unsigned modern_formats(void)
{
    char text[32];
    if (std::snprintf(text, sizeof(text), "%f", 1.25) != 8 || std::strcmp(text, "1.250000"))
        return 1;
    if (hpclang_hpgcc_snprintf(text, sizeof(text), "%f", 1.25) != 4 || std::strcmp(text, "1,25"))
        return 2;
    return 0;
}
