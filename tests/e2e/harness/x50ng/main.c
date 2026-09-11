#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int hpclang_direct_main(int argc, char **argv);
int hpclang_stock_main(int argc, char **argv);
int hpclang_gallery_main(int argc, char **argv);

int main(int argc, char **argv)
{
    const char *harness = getenv("HPCLANG_X50NG_HARNESS");

    if (harness == NULL || strcmp(harness, "direct") == 0)
        return hpclang_direct_main(argc, argv);
    if (strcmp(harness, "stock") == 0)
        return hpclang_stock_main(argc, argv);
    if (strcmp(harness, "gallery") == 0)
        return hpclang_gallery_main(argc, argv);

    fprintf(stderr,
            "unsupported HPCLANG_X50NG_HARNESS=%s "
            "(expected direct, stock, or gallery)\n",
            harness);
    return 2;
}
