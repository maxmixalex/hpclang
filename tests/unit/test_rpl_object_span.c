#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "rpl-object-span.h"

enum
{
    TEST_RPL_USEROB = 0x00080711u
};

struct image
{
    uint8_t nibbles[512];
    uint32_t used;
    uint32_t home;
    int expose_home;
};

static void put_nibbles(struct image *image, uint32_t at, uint32_t value, uint32_t count)
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

    if (image->expose_home && address >= TEST_RPL_USEROB && address < TEST_RPL_USEROB + 5u) {
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

    assert(hpclang_rpl_object_span(&reader, UINT32_MAX - 2u, 5u, &span) == HPCLANG_RPL_SPAN_BOUNDS);
    assert(span == 0u);
}

static void test_fixed_objects(void)
{
    struct image image = {0};
    struct hpclang_rpl_reader reader = reader_for(&image);
    uint32_t span = 0u;

    put_nibbles(&image, 0u, 0x02933u, 5u);
    image.used = 21u;
    assert(hpclang_rpl_object_span(&reader, 0u, 21u, &span) == HPCLANG_RPL_SPAN_OK);
    assert(span == 21u);

    put_nibbles(&image, 0u, 0x02977u, 5u);
    image.used = 37u;
    assert(hpclang_rpl_object_span(&reader, 0u, 37u, &span) == HPCLANG_RPL_SPAN_OK);
    assert(span == 37u);

    put_nibbles(&image, 0u, 0x029bfu, 5u);
    image.used = 7u;
    assert(hpclang_rpl_object_span(&reader, 0u, 7u, &span) == HPCLANG_RPL_SPAN_OK);
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
    assert(hpclang_rpl_object_span(&reader, 0u, image.used, &span) == HPCLANG_RPL_SPAN_OK);
    assert(span == 14u);

    assert(hpclang_rpl_object_span(&reader, 0u, 13u, &span) == HPCLANG_RPL_SPAN_BOUNDS);

    put_nibbles(&image, 5u, 4u, 5u);
    assert(hpclang_rpl_object_span(&reader, 0u, image.used, &span) == HPCLANG_RPL_SPAN_MALFORMED);
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
    assert(hpclang_rpl_object_span(&reader, 0u, image.used, &span) == HPCLANG_RPL_SPAN_OK);
    assert(span == 12u);

    memset(&image, 0, sizeof(image));
    put_nibbles(&image, 0u, 0x026acu, 5u); /* Flash pointer */
    image.used = 12u;
    assert(hpclang_rpl_object_span(&reader, 0u, image.used, &span) == HPCLANG_RPL_SPAN_OK);
    assert(span == 12u);

    memset(&image, 0, sizeof(image));
    put_nibbles(&image, 0u, 0x02baau, 5u); /* Extended pointer */
    image.used = 15u;
    assert(hpclang_rpl_object_span(&reader, 0u, image.used, &span) == HPCLANG_RPL_SPAN_OK);
    assert(span == 15u);

    memset(&image, 0, sizeof(image));
    put_nibbles(&image, 0u, 0x026feu, 5u); /* Minifont */
    put_nibbles(&image, 5u, 8u, 5u);
    image.used = 13u;
    assert(hpclang_rpl_object_span(&reader, 0u, image.used, &span) == HPCLANG_RPL_SPAN_OK);
    assert(span == 13u);
}

static void test_library_crc_extent(void)
{
    struct image image = {0};
    struct hpclang_rpl_reader reader = reader_for(&image);
    uint32_t span = 0u;

    /*
     * The length includes the trailing CRC, excluding only the prologue.
     * Stock HP48gII's L275_48 transfer file is 2404 bytes including its
     * eight-byte transfer header: 4792 nibbles, with length field 4787.
     * In Port 0 the next backup starts exactly 4792 nibbles after the library.
     */
    put_nibbles(&image, 0u, 0x02b40u, 5u);
    put_nibbles(&image, 5u, 13u, 5u);
    image.used = 18u; /* 5 + 13, including CRC */
    assert(hpclang_rpl_object_span(&reader, 0u, image.used, &span) == HPCLANG_RPL_SPAN_OK);
    assert(span == 18u);
    assert(hpclang_rpl_object_span(&reader, 0u, 17u, &span) == HPCLANG_RPL_SPAN_BOUNDS);
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
    assert(hpclang_rpl_object_span(&reader, 0u, image.used, &span) == HPCLANG_RPL_SPAN_OK);
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
    assert(hpclang_rpl_object_span(&reader, 0u, image.used, &span) == HPCLANG_RPL_SPAN_OK);
    assert(span == at);

    put_nibbles(&image, 5u, 5u, 5u);
    assert(hpclang_rpl_object_span(&reader, 0u, image.used, &span) == HPCLANG_RPL_SPAN_MALFORMED);
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
    assert(hpclang_rpl_object_span(&reader, 0u, image.used, &span) == HPCLANG_RPL_SPAN_OK);
    assert(span == 18u);

    /* Symbolic matrix is a SEMI-terminated composite runstream. */
    memset(&image, 0, sizeof(image));
    put_nibbles(&image, 0u, 0x02686u, 5u);
    put_nibbles(&image, 5u, 0x12345u, 5u);
    put_nibbles(&image, 10u, 0x0312bu, 5u);
    image.used = 15u;
    assert(hpclang_rpl_object_span(&reader, 0u, image.used, &span) == HPCLANG_RPL_SPAN_OK);
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
    assert(hpclang_rpl_object_span(&reader, 0u, 15u, &span) == HPCLANG_RPL_SPAN_OK);
    assert(span == 15u);

    put_nibbles(&image, 0u, 0x02e6du, 5u);
    assert(hpclang_rpl_object_span(&reader, 0u, 14u, &span) == HPCLANG_RPL_SPAN_BOUNDS);
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

    assert(hpclang_rpl_object_span(&reader, 0u, image.used, &span) == HPCLANG_RPL_SPAN_OK);
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

    assert(hpclang_rpl_object_span(&reader, 0u, image.used, &span) == HPCLANG_RPL_SPAN_OK);
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
    assert(hpclang_rpl_object_span(&reader, 0u, image.used, &span) == HPCLANG_RPL_SPAN_OK);
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
    assert(hpclang_rpl_object_span(&reader, 0u, image.used, &span) == HPCLANG_RPL_SPAN_OK);
    assert(span == 31u);
    assert(hpclang_rpl_object_span(&reader, 0u, 30u, &span) == HPCLANG_RPL_SPAN_BOUNDS);

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
    assert(hpclang_rpl_object_span(&reader, 0u, image.used, &span) == HPCLANG_RPL_SPAN_OK);
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
    assert(hpclang_rpl_object_span(&reader, 0u, image.used, &span) == HPCLANG_RPL_SPAN_OK);
    assert(span == 37u);

    /* HOME inserts 13 nibbles per attached library before the 3-nibble header. */
    memset(&image, 0, sizeof(image));
    image.expose_home = 1;
    image.home = 0u;
    put_nibbles(&image, 0u, 0x02a96u, 5u);
    put_nibbles(&image, 5u, 2u, 3u);
    put_nibbles(&image, 34u, 0u, 5u);
    image.used = 39u;
    assert(hpclang_rpl_object_span(&reader, 0u, image.used, &span) == HPCLANG_RPL_SPAN_OK);
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
    assert(hpclang_rpl_object_span(&reader, 0u, image.used, &span) == HPCLANG_RPL_SPAN_MALFORMED);

    /* Boundary parsing stays fail-closed on an unfrozen newest object. */
    put_nibbles(&image, 22u, 1u, 2u);
    put_nibbles(&image, 24u, 0x026d5u, 5u);
    image.used = 29u;
    assert(hpclang_rpl_object_span(&reader, 0u, image.used, &span) == HPCLANG_RPL_SPAN_UNSUPPORTED);
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

    assert(hpclang_rpl_object_span(&reader, 0u, image.used, &span) == HPCLANG_RPL_SPAN_UNSUPPORTED);
}

static void test_unknown_and_unfrozen_objects(void)
{
    struct image image = {0};
    struct hpclang_rpl_reader reader = reader_for(&image);
    uint32_t span = 123u;

    put_nibbles(&image, 0u, 0x54321u, 5u);
    image.used = 5u;
    assert(hpclang_rpl_object_span(&reader, 0u, image.used, &span) == HPCLANG_RPL_SPAN_UNSUPPORTED);
    assert(span == 0u);

    put_nibbles(&image, 0u, 0x02beeu, 5u); /* reserved EXT3 */
    assert(hpclang_rpl_object_span(&reader, 0u, image.used, &span) == HPCLANG_RPL_SPAN_UNSUPPORTED);
    put_nibbles(&image, 0u, 0x02c10u, 5u); /* reserved EXT4 */
    assert(hpclang_rpl_object_span(&reader, 0u, image.used, &span) == HPCLANG_RPL_SPAN_UNSUPPORTED);
}

struct array_visit_state
{
    uint32_t count;
    uint32_t type;
    uint32_t next;
    uint32_t sizes[3];
    int stop;
};

static int array_visit(void *context, uint32_t index, uint32_t type, uint32_t body,
                       uint32_t nibbles)
{
    struct array_visit_state *state = context;
    assert(index == state->count);
    assert(type == state->type);
    assert(body == state->next);
    assert(index < 3 && nibbles == state->sizes[index]);
    state->next += nibbles;
    ++state->count;
    return state->stop;
}

static void test_array_bodies(void)
{
    struct image image = {0};
    struct hpclang_rpl_reader reader = reader_for(&image);
    /* Two compact strings: their size words include themselves, not a prologue. */
    put_nibbles(&image, 0, 0x029e8, 5);
    put_nibbles(&image, 5, 34, 5);
    put_nibbles(&image, 10, 0x02a2c, 5);
    put_nibbles(&image, 15, 1, 5);
    put_nibbles(&image, 20, 2, 5);
    put_nibbles(&image, 25, 5, 5);
    put_nibbles(&image, 30, 9, 5);
    put_nibbles(&image, 35, 0x4241, 4);
    struct image original = image;
    struct array_visit_state state = {0, 0x02a2c, 25, {5, 9}, 0};
    assert(hpclang_rpl_array_visit(&reader, 0, 39, array_visit, &state) == 0);
    assert(state.count == 2 && state.next == 39);
    assert(memcmp(&image, &original, sizeof(image)) == 0);
    for (uint32_t limit = 0; limit < 39; ++limit) {
        state = (struct array_visit_state){0, 0x02a2c, 25, {5, 9}, 0};
        assert(hpclang_rpl_array_visit(&reader, 0, limit, array_visit, &state) != 0);
        assert(state.count == 0);
    }
    state = (struct array_visit_state){0, 0x02a2c, 25, {5, 9}, 71};
    assert(hpclang_rpl_array_visit(&reader, 0, 39, array_visit, &state) == 71);
    assert(state.count == 1);
    /* A valid first body does not make a truncated second body safe. */
    put_nibbles(&image, 30, 10, 5);
    state = (struct array_visit_state){0, 0x02a2c, 25, {5, 9}, 0};
    assert(hpclang_rpl_array_visit(&reader, 0, 39, array_visit, &state) != 0);
    assert(state.count == 1);
    image = original;
    put_nibbles(&image, 20, 1, 5);
    state = (struct array_visit_state){0, 0x02a2c, 25, {5}, 0};
    assert(hpclang_rpl_array_visit(&reader, 0, 39, array_visit, &state) ==
           HPCLANG_RPL_SPAN_MALFORMED);
    /* A two-dimensional numeric array has fixed-size bodies. */
    memset(&image, 0, sizeof(image));
    put_nibbles(&image, 0, 0x029e8, 5);
    put_nibbles(&image, 5, 57, 5);
    put_nibbles(&image, 10, 0x02933, 5);
    put_nibbles(&image, 15, 2, 5);
    put_nibbles(&image, 20, 1, 5);
    put_nibbles(&image, 25, 2, 5);
    image.used = 62;
    state = (struct array_visit_state){0, 0x02933, 30, {16, 16}, 0};
    assert(hpclang_rpl_array_visit(&reader, 0, 62, array_visit, &state) == 0);
    assert(state.count == 2);
    put_nibbles(&image, 20, 0xfffff, 5);
    put_nibbles(&image, 25, 0xfffff, 5);
    assert(hpclang_rpl_array_visit(&reader, 0, 62, array_visit, &state) == HPCLANG_RPL_SPAN_BOUNDS);
    assert(hpclang_rpl_array_visit(NULL, 0, 62, array_visit, &state) ==
           HPCLANG_RPL_SPAN_UNAVAILABLE);
    assert(hpclang_rpl_array_visit(&reader, UINT32_MAX - 10, 62, array_visit, &state) ==
           HPCLANG_RPL_SPAN_BOUNDS);
    /* Banked-pointer bodies must remain visible to the reference walker. */
    memset(&image, 0, sizeof(image));
    put_nibbles(&image, 0, 0x029e8, 5);
    put_nibbles(&image, 5, 30, 5);
    put_nibbles(&image, 10, 0x02baa, 5);
    put_nibbles(&image, 15, 1, 5);
    put_nibbles(&image, 20, 1, 5);
    put_nibbles(&image, 25, 0x4001c, 5);
    put_nibbles(&image, 30, 0x23c21, 5);
    state = (struct array_visit_state){0, 0x02baa, 25, {10}, 0};
    assert(hpclang_rpl_array_visit(&reader, 0, 35, array_visit, &state) == 0);
    assert(state.count == 1);
    /* Missing header reads and unsupported element types fail closed. */
    for (uint32_t used = 0; used < 25; ++used) {
        image.used = used;
        state.count = 0;
        assert(hpclang_rpl_array_visit(&reader, 0, 35, array_visit, &state) != 0);
        assert(state.count == 0);
    }
    image.used = 35;
    put_nibbles(&image, 10, 0x54321, 5);
    assert(hpclang_rpl_array_visit(&reader, 0, 35, array_visit, &state) ==
           HPCLANG_RPL_SPAN_UNSUPPORTED);
    put_nibbles(&image, 10, 0x02a96, 5);
    assert(hpclang_rpl_array_visit(&reader, 0, 35, array_visit, &state) ==
           HPCLANG_RPL_SPAN_UNSUPPORTED);
    /* Empty arrays still require an exact declared extent. */
    put_nibbles(&image, 0, 0x029e8, 5);
    put_nibbles(&image, 5, 20, 5);
    put_nibbles(&image, 10, 0x02933, 5);
    put_nibbles(&image, 20, 0, 5);
    state.count = 0;
    assert(hpclang_rpl_array_visit(&reader, 0, 25, array_visit, &state) == 0);
    assert(state.count == 0);
}

struct linked_visit_state
{
    uint32_t count;
    uint32_t addresses[3];
};

static int linked_visit(void *context, uint32_t index, uint32_t type, uint32_t body,
                        uint32_t nibbles)
{
    struct linked_visit_state *state = context;
    assert(index == state->count && index < 3);
    assert(type == 0x02a2c && nibbles == 7);
    assert(body == state->addresses[index]);
    ++state->count;
    return 0;
}

static void test_linked_array_bodies(void)
{
    struct image image = {0};
    struct hpclang_rpl_reader reader = reader_for(&image);
    put_nibbles(&image, 0, 0x02a0a, 5);
    put_nibbles(&image, 5, 49, 5);
    put_nibbles(&image, 10, 0x02a2c, 5);
    put_nibbles(&image, 15, 1, 5);
    put_nibbles(&image, 20, 3, 5);
    /* Elements may share bodies and need not follow physical storage order. */
    put_nibbles(&image, 25, 22, 5);
    put_nibbles(&image, 30, 10, 5);
    put_nibbles(&image, 35, 12, 5);
    put_nibbles(&image, 40, 7, 5);
    put_nibbles(&image, 45, 0x41, 2);
    put_nibbles(&image, 47, 7, 5);
    put_nibbles(&image, 52, 0x42, 2);
    struct image original = image;
    struct linked_visit_state state = {0, {47, 40, 47}};
    assert(hpclang_rpl_array_visit(&reader, 0, 54, linked_visit, &state) == 0);
    assert(state.count == 3);
    assert(memcmp(&image, &original, sizeof(image)) == 0);
    const uint32_t invalid[] = {0, 14, 29, 30, 0xfffff};
    for (uint32_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
        image = original;
        put_nibbles(&image, 25, invalid[i], 5);
        state.count = 0;
        assert(hpclang_rpl_array_visit(&reader, 0, 54, linked_visit, &state) != 0);
        assert(state.count == 0);
    }
    image = original;
    put_nibbles(&image, 47, 8, 5);
    assert(hpclang_rpl_array_visit(&reader, 0, 54, linked_visit, &state) ==
           HPCLANG_RPL_SPAN_BOUNDS);
    image = original;
    put_nibbles(&image, 20, 7, 5);
    assert(hpclang_rpl_array_visit(&reader, 0, 54, linked_visit, &state) ==
           HPCLANG_RPL_SPAN_BOUNDS);
}

struct sequence_state
{
    uint32_t count;
    int stop;
};

static int sequence_visit(void *context, uint32_t field, uint32_t target, uint32_t span)
{
    struct sequence_state *state = context;
    const uint32_t fields[] = {5, 10, 17};
    const uint32_t targets[] = {0x81234, 10, 17};
    const uint32_t spans[] = {0, 7, 15};
    assert(state->count < 3);
    assert(field == fields[state->count]);
    assert(target == targets[state->count]);
    assert(span == spans[state->count]);
    ++state->count;
    return state->stop;
}

static void test_sequence_children(void)
{
    struct image image = {0};
    struct hpclang_rpl_reader reader = reader_for(&image);
    put_nibbles(&image, 0, 0x02a74, 5);
    put_nibbles(&image, 5, 0x81234, 5);
    put_nibbles(&image, 10, 0x029bf, 5);
    put_nibbles(&image, 15, 0x41, 2);
    put_nibbles(&image, 17, 0x02d9d, 5);
    put_nibbles(&image, 22, 0x82345, 5);
    put_nibbles(&image, 27, 0x0312b, 5);
    put_nibbles(&image, 32, 0x0312b, 5);
    struct image original = image;
    struct sequence_state state = {0};
    assert(hpclang_rpl_sequence_visit(&reader, 0, 37, sequence_visit, &state) == 0);
    assert(state.count == 3);
    assert(memcmp(&image, &original, sizeof(image)) == 0);
    for (uint32_t size = 0; size < 37; ++size) {
        state.count = 0;
        assert(hpclang_rpl_sequence_visit(&reader, 0, size, sequence_visit, &state) != 0);
        assert(state.count == 0);
    }
    state = (struct sequence_state){0, 73};
    assert(hpclang_rpl_sequence_visit(&reader, 0, 37, sequence_visit, &state) == 73);
    assert(state.count == 1);
    state = (struct sequence_state){0};
    put_nibbles(&image, 22, 0x026d5, 5);
    assert(hpclang_rpl_sequence_visit(&reader, 0, 37, sequence_visit, &state) ==
           HPCLANG_RPL_SPAN_UNSUPPORTED);
    assert(state.count == 0);
    image = original;
    put_nibbles(&image, 5, 0x0312b, 5);
    assert(hpclang_rpl_sequence_visit(&reader, 0, 37, sequence_visit, &state) == 0);
    assert(state.count == 0);
    assert(hpclang_rpl_sequence_visit(&reader, 0, 37, NULL, &state) ==
           HPCLANG_RPL_SPAN_UNAVAILABLE);
}

int main(void)
{
    test_sequence_children();
    test_linked_array_bodies();
    test_array_bodies();
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
