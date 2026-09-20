#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "rpl-object-span.h"

enum {
    TEST_RPL_USEROB = 0x00080711u
};

struct image {
    uint8_t nibbles[512];
    uint32_t used;
    uint32_t home;
    int expose_home;
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
    if (image == NULL || value == NULL)
        return 0;

    if (image->expose_home &&
        address >= TEST_RPL_USEROB &&
        address < TEST_RPL_USEROB + 5u) {
        uint32_t shift = (address - TEST_RPL_USEROB) * 4u;
        *value = (uint8_t)((image->home >> shift) & 0x0fu);
        return 1;
    }

    if (address >= image->used)
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

static int read_zero(void *context, uint32_t address, uint8_t *value)
{
    (void)context;
    (void)address;
    if (value == NULL)
        return 0;
    *value = 0u;
    return 1;
}

static void test_address_overflow_is_bounded(void)
{
    const struct hpclang_rpl_reader reader = {
        .read_nibble = read_zero,
        .context = NULL,
    };
    uint32_t span = 99u;

    assert(hpclang_rpl_object_span(
               &reader, UINT32_MAX - 2u, 5u, &span) ==
           HPCLANG_RPL_SPAN_BOUNDS);
    assert(span == 0u);
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


static void test_hp49_extended_formats(void)
{
    struct image image = {0};
    struct hpclang_rpl_reader reader = reader_for(&image);
    uint32_t span = 0u;

    /* ZINT: prologue + self-relative size field/body. */
    put_nibbles(&image, 0u, 0x02614u, 5u);
    put_nibbles(&image, 5u, 7u, 5u);
    image.used = 12u;
    assert(hpclang_rpl_object_span(&reader, 0u, image.used, &span) ==
           HPCLANG_RPL_SPAN_OK);
    assert(span == 12u);

    memset(&image, 0, sizeof(image));
    put_nibbles(&image, 0u, 0x026acu, 5u); /* Flash pointer */
    image.used = 12u;
    assert(hpclang_rpl_object_span(&reader, 0u, image.used, &span) ==
           HPCLANG_RPL_SPAN_OK);
    assert(span == 12u);

    memset(&image, 0, sizeof(image));
    put_nibbles(&image, 0u, 0x02baau, 5u); /* Extended pointer */
    image.used = 15u;
    assert(hpclang_rpl_object_span(&reader, 0u, image.used, &span) ==
           HPCLANG_RPL_SPAN_OK);
    assert(span == 15u);

    memset(&image, 0, sizeof(image));
    put_nibbles(&image, 0u, 0x026feu, 5u); /* Minifont */
    put_nibbles(&image, 5u, 8u, 5u);
    image.used = 13u;
    assert(hpclang_rpl_object_span(&reader, 0u, image.used, &span) ==
           HPCLANG_RPL_SPAN_OK);
    assert(span == 13u);
}

static void test_library_crc_extent(void)
{
    struct image image = {0};
    struct hpclang_rpl_reader reader = reader_for(&image);
    uint32_t span = 0u;

    /*
     * Library length excludes the five-nibble prologue and the trailing
     * four-nibble CRC.  A parser returning only 5+length would truncate it.
     */
    put_nibbles(&image, 0u, 0x02b40u, 5u);
    put_nibbles(&image, 5u, 9u, 5u);
    image.used = 18u; /* 5 + 9 + 4 */
    assert(hpclang_rpl_object_span(&reader, 0u, image.used, &span) ==
           HPCLANG_RPL_SPAN_OK);
    assert(span == 18u);
    assert(hpclang_rpl_object_span(&reader, 0u, 17u, &span) ==
           HPCLANG_RPL_SPAN_BOUNDS);
}

static void test_long_numbers(void)
{
    struct image image = {0};
    struct hpclang_rpl_reader reader = reader_for(&image);
    uint32_t span = 0u;

    /* Long real = prologue + two self-sized infinite-integer components. */
    put_nibbles(&image, 0u, 0x0263au, 5u);
    put_nibbles(&image, 5u, 6u, 5u);
    put_nibbles(&image, 11u, 7u, 5u);
    image.used = 18u;
    assert(hpclang_rpl_object_span(&reader, 0u, image.used, &span) ==
           HPCLANG_RPL_SPAN_OK);
    assert(span == 18u);

    /* Long complex has four such components. */
    memset(&image, 0, sizeof(image));
    put_nibbles(&image, 0u, 0x02660u, 5u);
    uint32_t at = 5u;
    for (uint32_t i = 0u; i < 4u; ++i) {
        put_nibbles(&image, at, 6u, 5u);
        at += 6u;
    }
    image.used = at;
    assert(hpclang_rpl_object_span(&reader, 0u, image.used, &span) ==
           HPCLANG_RPL_SPAN_OK);
    assert(span == at);

    put_nibbles(&image, 5u, 5u, 5u);
    assert(hpclang_rpl_object_span(&reader, 0u, image.used, &span) ==
           HPCLANG_RPL_SPAN_MALFORMED);
}

static void test_tagged_and_symbolic_matrix(void)
{
    struct image image = {0};
    struct hpclang_rpl_reader reader = reader_for(&image);
    uint32_t span = 0u;

    /* Tagged: prologue, 2-nibble tag length, tag bytes, embedded CHAR. */
    put_nibbles(&image, 0u, 0x02afcu, 5u);
    put_nibbles(&image, 5u, 2u, 2u);
    put_nibbles(&image, 7u, 0x41u, 2u);
    put_nibbles(&image, 9u, 0x42u, 2u);
    put_nibbles(&image, 11u, 0x029bfu, 5u);
    put_nibbles(&image, 16u, 0x43u, 2u);
    image.used = 18u;
    assert(hpclang_rpl_object_span(&reader, 0u, image.used, &span) ==
           HPCLANG_RPL_SPAN_OK);
    assert(span == 18u);

    /* Symbolic matrix is a SEMI-terminated composite runstream. */
    memset(&image, 0, sizeof(image));
    put_nibbles(&image, 0u, 0x02686u, 5u);
    put_nibbles(&image, 5u, 0x12345u, 5u);
    put_nibbles(&image, 10u, 0x0312bu, 5u);
    image.used = 15u;
    assert(hpclang_rpl_object_span(&reader, 0u, image.used, &span) ==
           HPCLANG_RPL_SPAN_OK);
    assert(span == 15u);
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


static void test_directory_spans(void)
{
    struct image image = {0};
    struct hpclang_rpl_reader reader = reader_for(&image);
    uint32_t span = 0u;

    /* Empty non-HOME directory: DORRP, 3-nibble header, empty chain. */
    image.expose_home = 1;
    image.home = 0x100u;
    put_nibbles(&image, 0u, 0x02a96u, 5u);
    put_nibbles(&image, 8u, 0u, 5u);
    image.used = 13u;
    assert(hpclang_rpl_object_span(&reader, 0u, image.used, &span) ==
           HPCLANG_RPL_SPAN_OK);
    assert(span == 13u);

    /*
     * One ordinary named item. The chain points past a five-nibble terminal
     * link to [len][name][duplicate len][object].
     */
    memset(&image, 0, sizeof(image));
    image.expose_home = 1;
    image.home = 0x100u;
    put_nibbles(&image, 0u, 0x02a96u, 5u);
    put_nibbles(&image, 8u, 10u, 5u);
    put_nibbles(&image, 13u, 0u, 5u);
    put_nibbles(&image, 18u, 1u, 2u);
    put_nibbles(&image, 20u, 0x41u, 2u);
    put_nibbles(&image, 22u, 1u, 2u);
    put_nibbles(&image, 24u, 0x029bfu, 5u);
    put_nibbles(&image, 29u, 0x42u, 2u);
    image.used = 31u;
    assert(hpclang_rpl_object_span(&reader, 0u, image.used, &span) ==
           HPCLANG_RPL_SPAN_OK);
    assert(span == 31u);
    assert(hpclang_rpl_object_span(&reader, 0u, 30u, &span) ==
           HPCLANG_RPL_SPAN_BOUNDS);

    /*
     * Real HP39's HOME Hidden entry proves len=0 has no duplicate trailing
     * length: its object begins exactly two nibbles after the item.
     */
    memset(&image, 0, sizeof(image));
    image.expose_home = 1;
    image.home = 0x100u;
    put_nibbles(&image, 0u, 0x02a96u, 5u);
    put_nibbles(&image, 8u, 10u, 5u);
    put_nibbles(&image, 13u, 0u, 5u);
    put_nibbles(&image, 18u, 0u, 2u);
    put_nibbles(&image, 20u, 0x029bfu, 5u);
    put_nibbles(&image, 25u, 0x43u, 2u);
    image.used = 27u;
    assert(hpclang_rpl_object_span(&reader, 0u, image.used, &span) ==
           HPCLANG_RPL_SPAN_OK);
    assert(span == 27u);

    /* The newest object may itself be a directory; recurse to its exact end. */
    memset(&image, 0, sizeof(image));
    image.expose_home = 1;
    image.home = 0x100u;
    put_nibbles(&image, 0u, 0x02a96u, 5u);
    put_nibbles(&image, 8u, 10u, 5u);
    put_nibbles(&image, 13u, 0u, 5u);
    put_nibbles(&image, 18u, 1u, 2u);
    put_nibbles(&image, 20u, 0x41u, 2u);
    put_nibbles(&image, 22u, 1u, 2u);
    put_nibbles(&image, 24u, 0x02a96u, 5u);
    put_nibbles(&image, 32u, 0u, 5u);
    image.used = 37u;
    assert(hpclang_rpl_object_span(&reader, 0u, image.used, &span) ==
           HPCLANG_RPL_SPAN_OK);
    assert(span == 37u);

    /* HOME inserts 13 nibbles per attached library before the 3-nibble header. */
    memset(&image, 0, sizeof(image));
    image.expose_home = 1;
    image.home = 0u;
    put_nibbles(&image, 0u, 0x02a96u, 5u);
    put_nibbles(&image, 5u, 2u, 3u);
    put_nibbles(&image, 34u, 0u, 5u);
    image.used = 39u;
    assert(hpclang_rpl_object_span(&reader, 0u, image.used, &span) ==
           HPCLANG_RPL_SPAN_OK);
    assert(span == 39u);

    /* A non-empty name must repeat its length immediately before the object. */
    memset(&image, 0, sizeof(image));
    image.expose_home = 1;
    image.home = 0x100u;
    put_nibbles(&image, 0u, 0x02a96u, 5u);
    put_nibbles(&image, 8u, 10u, 5u);
    put_nibbles(&image, 13u, 0u, 5u);
    put_nibbles(&image, 18u, 1u, 2u);
    put_nibbles(&image, 20u, 0x41u, 2u);
    put_nibbles(&image, 22u, 2u, 2u);
    put_nibbles(&image, 24u, 0x029bfu, 5u);
    put_nibbles(&image, 29u, 0x42u, 2u);
    image.used = 31u;
    assert(hpclang_rpl_object_span(&reader, 0u, image.used, &span) ==
           HPCLANG_RPL_SPAN_MALFORMED);

    /* Boundary parsing stays fail-closed on an unfrozen newest object. */
    put_nibbles(&image, 22u, 1u, 2u);
    put_nibbles(&image, 24u, 0x026d5u, 5u);
    image.used = 29u;
    assert(hpclang_rpl_object_span(&reader, 0u, image.used, &span) ==
           HPCLANG_RPL_SPAN_UNSUPPORTED);
}

static void test_unfrozen_inline_prologue_is_not_a_reference(void)
{
    struct image image = {0};
    struct hpclang_rpl_reader reader = reader_for(&image);
    uint32_t span = 0u;

    /* A known but still-unfrozen Aplet prologue is never a ROM reference. */
    put_nibbles(&image, 0u, 0x02d9du, 5u);
    put_nibbles(&image, 5u, 0x026d5u, 5u);
    put_nibbles(&image, 10u, 0x0312bu, 5u);
    image.used = 15u;

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

    put_nibbles(&image, 0u, 0x02beeu, 5u); /* reserved EXT3 */
    assert(hpclang_rpl_object_span(&reader, 0u, image.used, &span) ==
           HPCLANG_RPL_SPAN_UNSUPPORTED);
    put_nibbles(&image, 0u, 0x02c10u, 5u); /* reserved EXT4 */
    assert(hpclang_rpl_object_span(&reader, 0u, image.used, &span) ==
           HPCLANG_RPL_SPAN_UNSUPPORTED);
}

int main(void)
{
    test_address_overflow_is_bounded();
    test_fixed_objects();
    test_length_prefixed();
    test_hp49_extended_formats();
    test_library_crc_extent();
    test_long_numbers();
    test_tagged_and_symbolic_matrix();
    test_names();
    test_composite_with_reference_and_nested_object();
    test_nested_composite();
    test_directory_spans();
    test_unfrozen_inline_prologue_is_not_a_reference();
    test_unknown_and_unfrozen_objects();
    return 0;
}
