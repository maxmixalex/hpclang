#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "rpl-object-span.h"

struct image {
    uint8_t nibbles[512];
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
        image->nibbles[at + i] = (uint8_t)((value >> (i * 4u)) & 0x0fu);
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

static void test_fixed_objects(void)
{
    struct image image = {0};
    struct hpclang_rpl_reader reader = reader_for(&image);
    uint32_t span = 0u;

    put_nibbles(&image, 0u, 0x02933u, 5u);
    image.used = 21u;
    assert(hpclang_rpl_object_span(&reader, 0u, 21u, &span) ==
           HPCLANG_RPL_SPAN_OK);
    assert(span == 21u);

    put_nibbles(&image, 0u, 0x02977u, 5u);
    image.used = 37u;
    assert(hpclang_rpl_object_span(&reader, 0u, 37u, &span) ==
           HPCLANG_RPL_SPAN_OK);
    assert(span == 37u);

    put_nibbles(&image, 0u, 0x029bfu, 5u);
    image.used = 7u;
    assert(hpclang_rpl_object_span(&reader, 0u, 7u, &span) ==
           HPCLANG_RPL_SPAN_OK);
    assert(span == 7u);
}

static void test_length_prefixed(void)
{
    struct image image = {0};
    struct hpclang_rpl_reader reader = reader_for(&image);
    uint32_t span = 0u;

    /* String "HP": prologue + length(5 + 2*2) + four character nibbles. */
    put_nibbles(&image, 0u, 0x02a2cu, 5u);
    put_nibbles(&image, 5u, 9u, 5u);
    put_nibbles(&image, 10u, 0x50u, 2u);
    put_nibbles(&image, 12u, 0x48u, 2u);
    assert(hpclang_rpl_object_span(&reader, 0u, image.used, &span) ==
           HPCLANG_RPL_SPAN_OK);
    assert(span == 14u);

    assert(hpclang_rpl_object_span(&reader, 0u, 13u, &span) ==
           HPCLANG_RPL_SPAN_BOUNDS);

    put_nibbles(&image, 5u, 4u, 5u);
    assert(hpclang_rpl_object_span(&reader, 0u, image.used, &span) ==
           HPCLANG_RPL_SPAN_MALFORMED);
}

static void test_names(void)
{
    struct image image = {0};
    struct hpclang_rpl_reader reader = reader_for(&image);
    uint32_t span = 0u;

    put_nibbles(&image, 0u, 0x02e48u, 5u);
    put_nibbles(&image, 5u, 4u, 2u);
    image.used = 15u;
    assert(hpclang_rpl_object_span(&reader, 0u, 15u, &span) ==
           HPCLANG_RPL_SPAN_OK);
    assert(span == 15u);

    put_nibbles(&image, 0u, 0x02e6du, 5u);
    assert(hpclang_rpl_object_span(&reader, 0u, 14u, &span) ==
           HPCLANG_RPL_SPAN_BOUNDS);
}

static void test_composite_with_reference_and_nested_object(void)
{
    struct image image = {0};
    struct hpclang_rpl_reader reader = reader_for(&image);
    uint32_t span = 0u;
    uint32_t at = 0u;

    /* :: <ROM reference> %1 "A" ; */
    put_nibbles(&image, at, 0x02d9du, 5u);
    at += 5u;
    put_nibbles(&image, at, 0x12345u, 5u);
    at += 5u;

    put_nibbles(&image, at, 0x02933u, 5u);
    at += 21u;

    put_nibbles(&image, at, 0x02a2cu, 5u);
    put_nibbles(&image, at + 5u, 7u, 5u);
    image.used = at + 12u;
    at += 12u;

    put_nibbles(&image, at, 0x0312bu, 5u);
    at += 5u;
    image.used = at;

    assert(hpclang_rpl_object_span(&reader, 0u, image.used, &span) ==
           HPCLANG_RPL_SPAN_OK);
    assert(span == image.used);
}

static void test_nested_composite(void)
{
    struct image image = {0};
    struct hpclang_rpl_reader reader = reader_for(&image);
    uint32_t span = 0u;

    /* { :: <ref> ; ; } represented by DOLIST DOCOL ref SEMI SEMI. */
    put_nibbles(&image, 0u, 0x02a74u, 5u);
    put_nibbles(&image, 5u, 0x02d9du, 5u);
    put_nibbles(&image, 10u, 0x22222u, 5u);
    put_nibbles(&image, 15u, 0x0312bu, 5u);
    put_nibbles(&image, 20u, 0x0312bu, 5u);
    image.used = 25u;

    assert(hpclang_rpl_object_span(&reader, 0u, image.used, &span) ==
           HPCLANG_RPL_SPAN_OK);
    assert(span == 25u);
}


static void test_unfrozen_inline_prologue_is_not_a_reference(void)
{
    struct image image = {0};
    struct hpclang_rpl_reader reader = reader_for(&image);
    uint32_t span = 0u;

    /* DOCOL followed by DORRP must stop as unsupported, not skip five nibbles. */
    put_nibbles(&image, 0u, 0x02d9du, 5u);
    put_nibbles(&image, 5u, 0x02a96u, 5u);
    put_nibbles(&image, 10u, 0x0312bu, 5u);
    image.used = 15u;

    assert(hpclang_rpl_object_span(&reader, 0u, image.used, &span) ==
           HPCLANG_RPL_SPAN_UNSUPPORTED);

    /* A known 49G-specific prologue is likewise never treated as a ROM ref. */
    put_nibbles(&image, 5u, 0x02614u, 5u);
    assert(hpclang_rpl_object_span(&reader, 0u, image.used, &span) ==
           HPCLANG_RPL_SPAN_UNSUPPORTED);
}

static void test_unknown_and_unfrozen_objects(void)
{
    struct image image = {0};
    struct hpclang_rpl_reader reader = reader_for(&image);
    uint32_t span = 123u;

    put_nibbles(&image, 0u, 0x54321u, 5u);
    image.used = 5u;
    assert(hpclang_rpl_object_span(&reader, 0u, image.used, &span) ==
           HPCLANG_RPL_SPAN_UNSUPPORTED);
    assert(span == 0u);

    put_nibbles(&image, 0u, 0x02a96u, 5u); /* DORRP directory */
    assert(hpclang_rpl_object_span(&reader, 0u, image.used, &span) ==
           HPCLANG_RPL_SPAN_UNSUPPORTED);
}

int main(void)
{
    test_fixed_objects();
    test_length_prefixed();
    test_names();
    test_composite_with_reference_and_nested_object();
    test_nested_composite();
    test_unfrozen_inline_prologue_is_not_a_reference();
    test_unknown_and_unfrozen_objects();
    return 0;
}
