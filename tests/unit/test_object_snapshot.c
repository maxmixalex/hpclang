#include <assert.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <hpclang/object.h>

#include "object-snapshot.h"

struct image {
    uint8_t nibbles[128];
    uint32_t used;
};

static void put_nibbles(
    struct image *image,
    uint32_t at,
    uint32_t value,
    uint32_t count)
{
    assert(image != NULL);
    assert(at + count <= sizeof(image->nibbles));
    for (uint32_t i = 0u; i < count; ++i)
        image->nibbles[at + i] =
            (uint8_t)((value >> (i * 4u)) & 0x0fu);
    if (image->used < at + count)
        image->used = at + count;
}

static int read_nibble(void *context, uint32_t address, uint8_t *value)
{
    struct image *image = context;
    if (image == NULL || value == NULL || address >= image->used)
        return 0;
    *value = image->nibbles[address];
    return 1;
}

static struct hpclang_rpl_reader reader_for(struct image *image)
{
    const struct hpclang_rpl_reader reader = {
        .read_nibble = read_nibble,
        .context = image,
    };
    return reader;
}

static void test_real_snapshot(void)
{
    struct image image = {0};
    put_nibbles(&image, 0u, 0x02933u, 5u);
    for (uint32_t i = 5u; i < 21u; ++i)
        image.nibbles[i] = (uint8_t)(i & 0x0fu);
    image.used = 21u;

    struct hpclang_rpl_reader reader = reader_for(&image);
    struct hpclang_object *object = NULL;
    assert(hpclang_object_snapshot_copy(
               &reader, 0u, image.used, &object) == 0);
    assert(object != NULL);
    assert(hpclang_object_kind(object) == HPCLANG_OBJECT_REAL);
    assert(object->nibble_count == 21u);
    assert(object->native_prologue == 0x02933u);

    for (uint32_t i = 0u; i < image.used; ++i) {
        uint8_t value = 0xffu;
        assert(hpclang_object_snapshot_read_nibble(object, i, &value));
        assert(value == image.nibbles[i]);
    }
    uint8_t value = 0u;
    assert(!hpclang_object_snapshot_read_nibble(
        object, image.used, &value));

    hpclang_object_release(object);
}

static void test_odd_character_snapshot(void)
{
    struct image image = {0};
    put_nibbles(&image, 0u, 0x029bfu, 5u);
    put_nibbles(&image, 5u, 0x41u, 2u);
    image.used = 7u;

    struct hpclang_rpl_reader reader = reader_for(&image);
    struct hpclang_object *object = NULL;
    assert(hpclang_object_snapshot_copy(
               &reader, 0u, image.used, &object) == 0);
    assert(hpclang_object_kind(object) == HPCLANG_OBJECT_CHARACTER);
    assert(object->nibble_count == 7u);

    uint8_t value = 0u;
    assert(hpclang_object_snapshot_read_nibble(object, 6u, &value));
    assert(value == 4u);
    /* Unused high half of the final packed byte stays zero. */
    assert((object->data[3] & 0xf0u) == 0u);
    hpclang_object_release(object);
}

static void test_errors(void)
{
    struct image image = {0};
    struct hpclang_rpl_reader reader = reader_for(&image);
    struct hpclang_object *object = (struct hpclang_object *)(uintptr_t)1u;

    put_nibbles(&image, 0u, 0x02a96u, 5u); /* directory: unfrozen */
    image.used = 5u;
    errno = 0;
    assert(hpclang_object_snapshot_copy(
               &reader, 0u, image.used, &object) == -1);
    assert(object == NULL);
    assert(errno == ENOTSUP);

    memset(&image, 0, sizeof(image));
    put_nibbles(&image, 0u, 0x02a2cu, 5u);
    put_nibbles(&image, 5u, 4u, 5u); /* invalid self-relative length */
    image.used = 10u;
    errno = 0;
    assert(hpclang_object_snapshot_copy(
               &reader, 0u, image.used, &object) == -1);
    assert(object == NULL);
    assert(errno == EILSEQ);
}

static void test_character_constructor(void)
{
    struct hpclang_object *object = NULL;
    assert(hpclang_object_create_character(0xa5u, &object) == 0);
    assert(object != NULL);
    assert(hpclang_object_kind(object) == HPCLANG_OBJECT_CHARACTER);
    assert(object->nibble_count == 7u);
    assert(object->native_prologue == 0x029bfu);

    uint8_t value = 0u;
    assert(hpclang_object_character_value(object, &value) == 0);
    assert(value == 0xa5u);

    hpclang_object_release(object);
}

int main(void)
{
    test_real_snapshot();
    test_odd_character_snapshot();
    test_character_constructor();
    test_errors();
    return 0;
}
