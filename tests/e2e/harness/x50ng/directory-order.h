#ifndef HPCLANG_DIRECTORY_ORDER_H
#define HPCLANG_DIRECTORY_ORDER_H

#include <dirent.h>
#include <stdlib.h>
#include <string.h>

/* Byte order makes virtual FAT layout independent of host filesystem/locale. */
static int hpclang_directory_compare(const struct dirent **a, const struct dirent **b)
{
    return strcmp((*a)->d_name, (*b)->d_name);
}

static void hpclang_directory_free(struct dirent **entries, int count)
{
    for (int i = 0; i < count; ++i)
        free(entries[i]);
    free(entries);
}

#endif
