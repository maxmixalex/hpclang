#include <hpconio.h>
#include <cstdio>

/* A second translation unit may include the compatibility header. Standard
 * C++ stdio names remain intact; registration is process-wide and idempotent. */
extern "C" int print_status(void)
{
    return std::printf("C and C++ share this screen\n");
}
