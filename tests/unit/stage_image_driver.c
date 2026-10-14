#include "stage-image.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    if (argc != 3)
        return 2;
    int result = 2;
    unsigned char *packed = NULL;
    unsigned char *output = NULL;
    FILE *file = fopen(argv[1], "rb");
    if (!file || fseek(file, 0, SEEK_END))
        goto done;
    long length = ftell(file);
    if (length < 0 || fseek(file, 0, SEEK_SET))
        goto done;
    packed = malloc(length ? (size_t)length : 1);
    size_t capacity = (size_t)strtoul(argv[2], NULL, 10);
    output = malloc(capacity ? capacity : 1);
    if (!packed || !output || fread(packed, 1, (size_t)length, file) != (size_t)length)
        goto done;
    result =
        hpclang_stage_image_unpack((const void *)packed, (size_t)length, output, capacity) ? 1 : 0;
    if (!result && fwrite(output, 1, capacity, stdout) != capacity)
        result = 2;
done:
    if (file)
        fclose(file);
    free(output);
    free(packed);
    return result;
}
