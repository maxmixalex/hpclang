#define _DEFAULT_SOURCE
#include <assert.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include "directory-order.h"

int main(void)
{
    char first[] = "/tmp/hpclang-directory-a-XXXXXX";
    char second[] = "/tmp/hpclang-directory-b-XXXXXX";
    assert(mkdtemp(first) && mkdtemp(second));
    const char *names[] = {"ZZHPDIR", "HPCLANG.TST", "HPCLANG.HP", "HPCLANG.OUT", "mixed.txt"};
    const char *expected[] = {".",           "..",      "HPCLANG.HP", "HPCLANG.OUT",
                              "HPCLANG.TST", "ZZHPDIR", "mixed.txt"};
    const char *roots[] = {first, second};
    for (int root = 0; root < 2; ++root) {
        for (int i = 0; i < 5; ++i) {
            int index = root ? 4 - i : i;
            char path[256];
            snprintf(path, sizeof path, "%s/%s", roots[root], names[index]);
            if (index == 0) {
                assert(mkdir(path, 0700) == 0);
            } else {
                FILE *file = fopen(path, "w");
                assert(file && fclose(file) == 0);
            }
        }
        struct dirent **entries = NULL;
        int count = scandir(roots[root], &entries, NULL, hpclang_directory_compare);
        assert(count == 7);
        for (int i = 0; i < count; ++i)
            assert(strcmp(entries[i]->d_name, expected[i]) == 0);
        hpclang_directory_free(entries, count);
        for (int i = 0; i < 5; ++i) {
            char path[256];
            snprintf(path, sizeof path, "%s/%s", roots[root], names[i]);
            assert((i == 0 ? rmdir(path) : unlink(path)) == 0);
        }
        assert(rmdir(roots[root]) == 0);
    }
    return 0;
}
