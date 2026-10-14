#include "rpl-reference.h"
#include <assert.h>
#include <string.h>

struct image
{
    uint8_t data[128];
    uint32_t size, fail;
};

static int read_nibble(void *context, uint32_t at, uint8_t *value)
{
    struct image *image = context;
    if (at >= image->size || at == image->fail)
        return 0;
    *value = image->data[at];
    return 1;
}

static void put(struct image *image, uint32_t at, uint32_t value, unsigned width)
{
    assert(at + width <= sizeof image->data);
    for (unsigned i = 0; i < width; ++i)
        image->data[at + i] = (value >> (4 * i)) & 15;
}

struct edges
{
    struct hpclang_rpl_reference items[8];
    unsigned count;
    int stop;
};

static int collect(void *context, const struct hpclang_rpl_reference *edge)
{
    struct edges *edges = context;
    assert(edges->count < 8);
    edges->items[edges->count++] = *edge;
    return edges->stop;
}

static void nested_array(void)
{
    struct image image = {.size = 45, .fail = UINT32_MAX};
    struct hpclang_rpl_reader reader = {read_nibble, &image};
    put(&image, 0, 0x2a74, 5);
    put(&image, 5, 0x29e8, 5);
    put(&image, 10, 30, 5);
    put(&image, 15, 0x2baa, 5);
    put(&image, 20, 1, 5);
    put(&image, 25, 1, 5);
    put(&image, 30, 0x4001c, 5);
    put(&image, 35, 0x23c21, 5);
    put(&image, 40, 0x312b, 5);
    struct image original = image;
    struct edges edges = {0};
    assert(hpclang_rpl_reference_visit(&reader, 0, 45, 0, collect, &edges) == 0);
    assert(edges.count == 1 && edges.items[0].kind == HPCLANG_RPL_REFERENCE_INLINE);
    struct hpclang_rpl_reference edge = edges.items[0];
    assert(edge.target == 5 && edge.extent == 35);
    edges.count = 0;
    assert(hpclang_rpl_reference_visit(&reader, edge.target, edge.extent, 0, collect, &edges) == 0);
    assert(edges.count == 1 && edges.items[0].kind == HPCLANG_RPL_REFERENCE_COMPACT);
    edge = edges.items[0];
    assert(edge.target == 30 && edge.extent == 10 && edge.auxiliary == 0x2baa);
    edges.count = 0;
    assert(hpclang_rpl_reference_visit(&reader, edge.target, edge.extent, edge.auxiliary, collect,
                                       &edges) == 0);
    assert(edges.count == 1 && edges.items[0].kind == HPCLANG_RPL_REFERENCE_ACPTR);
    assert(edges.items[0].field == 30 && edges.items[0].target == 0x4001c &&
           edges.items[0].auxiliary == 0x23c21);
    assert(memcmp(&image, &original, sizeof image) == 0);
    for (uint32_t at = 30; at < 40; ++at) {
        image.fail = at;
        edges.count = 0;
        assert(hpclang_rpl_reference_visit(&reader, 30, 10, 0x2baa, collect, &edges) ==
               HPCLANG_RPL_SPAN_UNAVAILABLE);
        assert(edges.count == 0);
    }
    image = original;
    for (uint32_t size = 0; size < 45; ++size) {
        edges.count = 0;
        assert(hpclang_rpl_reference_visit(&reader, 0, size, 0, collect, &edges) < 0);
    }
    edges = (struct edges){.stop = 73};
    assert(hpclang_rpl_reference_visit(&reader, 30, 10, 0x2baa, collect, &edges) == 73);
    assert(edges.count == 1);
    assert(hpclang_rpl_reference_visit(&reader, 4, 10, 0x2baa, collect, &edges) ==
           HPCLANG_RPL_SPAN_BOUNDS);
    assert(hpclang_rpl_reference_visit(&reader, 30, 10, 0x2a96, collect, &edges) ==
           HPCLANG_RPL_SPAN_UNSUPPORTED);
}

static void indices_and_opaque(void)
{
    struct image image = {.size = 20, .fail = UINT32_MAX};
    struct hpclang_rpl_reader reader = {read_nibble, &image};
    struct edges edges = {0};
    put(&image, 0, 0x2afc, 5);
    put(&image, 5, 1, 2);
    put(&image, 7, 'A', 2);
    put(&image, 9, 0x2e92, 5);
    put(&image, 14, 161, 3);
    put(&image, 17, 10, 3);
    assert(hpclang_rpl_reference_visit(&reader, 0, 20, 0, collect, &edges) == 0);
    assert(edges.count == 1 && edges.items[0].kind == HPCLANG_RPL_REFERENCE_INLINE);
    assert(edges.items[0].target == 9 && edges.items[0].extent == 11);
    edges.count = 0;
    assert(hpclang_rpl_reference_visit(&reader, 9, 11, 0, collect, &edges) == 0);
    assert(edges.items[0].kind == HPCLANG_RPL_REFERENCE_ROMP && edges.items[0].target == 161 &&
           edges.items[0].auxiliary == 10);
    put(&image, 0, 0x26ac, 5);
    put(&image, 5, 15, 3);
    put(&image, 8, 34, 4);
    edges.count = 0;
    assert(hpclang_rpl_reference_visit(&reader, 0, 12, 0, collect, &edges) == 0);
    assert(edges.items[0].kind == HPCLANG_RPL_REFERENCE_FLASHP && edges.items[0].target == 15 &&
           edges.items[0].auxiliary == 34);
    put(&image, 0, 0x2dcc, 5);
    put(&image, 5, 10, 5);
    edges.count = 0;
    assert(hpclang_rpl_reference_visit(&reader, 0, 15, 0, collect, &edges) == 0);
    assert(edges.count == 1 && edges.items[0].kind == HPCLANG_RPL_REFERENCE_OPAQUE &&
           edges.items[0].target == 0 && edges.items[0].extent == 15 &&
           edges.items[0].auxiliary == 0x2dcc);
    put(&image, 0, 0x2a2c, 5); // The same bytes in a string are not native references.
    edges.count = 0;
    assert(hpclang_rpl_reference_visit(&reader, 0, 15, 0, collect, &edges) == 0);
    assert(edges.count == 0);
    put(&image, 0, 0x2d9d, 5);
    put(&image, 5, 0x81234, 5);
    put(&image, 10, 0x312b, 5);
    assert(hpclang_rpl_reference_visit(&reader, 0, 15, 0, collect, &edges) == 0);
    assert(edges.count == 1 && edges.items[0].kind == HPCLANG_RPL_REFERENCE_DIRECT &&
           edges.items[0].target == 0x81234 && edges.items[0].extent == 0);
    put(&image, 0, 0x26d5, 5);
    edges.count = 0;
    assert(hpclang_rpl_reference_visit(&reader, 0, 20, 0, collect, &edges) ==
           HPCLANG_RPL_SPAN_UNSUPPORTED);
    assert(edges.count == 0);
    assert(hpclang_rpl_reference_visit(&reader, 0xfffff, 20, 0, collect, &edges) ==
           HPCLANG_RPL_SPAN_BOUNDS);
    assert(hpclang_rpl_reference_visit(NULL, 0, 20, 0, collect, &edges) ==
           HPCLANG_RPL_SPAN_UNAVAILABLE);
}

static void linked_aliases(void)
{
    struct image image = {.size = 45, .fail = UINT32_MAX};
    struct hpclang_rpl_reader reader = {read_nibble, &image};
    put(&image, 0, 0x2a0a, 5);
    put(&image, 5, 40, 5);
    put(&image, 10, 0x2baa, 5);
    put(&image, 15, 1, 5);
    put(&image, 20, 2, 5);
    put(&image, 25, 10, 5);
    put(&image, 30, 5, 5);
    put(&image, 35, 0x7327e, 5);
    put(&image, 40, 0x22f5a, 5);
    struct edges edges = {0};
    assert(hpclang_rpl_reference_visit(&reader, 0, 45, 0, collect, &edges) == 0);
    assert(edges.count == 2);
    for (unsigned i = 0; i < 2; ++i) {
        assert(edges.items[i].kind == HPCLANG_RPL_REFERENCE_COMPACT);
        assert(edges.items[i].target == 35 && edges.items[i].extent == 10 &&
               edges.items[i].auxiliary == 0x2baa);
    }
    edges.count = 0;
    put(&image, 30, 4, 5); // Link into the table is not an independent body.
    assert(hpclang_rpl_reference_visit(&reader, 0, 45, 0, collect, &edges) < 0);
    assert(edges.count == 1); // Earlier events must be discarded on error.
}

static void native_envelopes(void)
{
    static const char backup[] = "26b20130008005f42545a55425f48041620900003210119200f9e3";
    struct image image = {.size = sizeof backup - 1, .fail = UINT32_MAX};
    for (unsigned i = 0; i < image.size; ++i)
        image.data[i] = backup[i] <= '9' ? backup[i] - '0' : backup[i] - 'a' + 10;
    struct hpclang_rpl_reader reader = {read_nibble, &image};
    struct edges edges = {0};
    assert(hpclang_rpl_reference_visit(&reader, 0, image.size, 0, collect, &edges) == 0);
    assert(edges.count == 1 && edges.items[0].kind == HPCLANG_RPL_REFERENCE_INLINE &&
           edges.items[0].target == 30 && edges.items[0].extent == 14);
    image.data[image.size - 1] ^= 1;
    edges.count = 0;
    assert(hpclang_rpl_reference_visit(&reader, 0, image.size, 0, collect, &edges) ==
           HPCLANG_RPL_SPAN_MALFORMED);
    assert(edges.count == 0);

    image = (struct image){.size = 90, .fail = UINT32_MAX};
    put(&image, 0, 0x2b40, 5);
    put(&image, 5, 85, 5);
    put(&image, 14, 123, 3);
    put(&image, 27, 30, 5);
    put(&image, 32, 5, 5);
    put(&image, 37, 0x2d9d, 5);
    put(&image, 42, 0x312b, 5);
    put(&image, 47, 0x2a4e, 5);
    put(&image, 52, 5, 5);
    put(&image, 57, 0x2a4e, 5);
    put(&image, 62, 15, 5);
    put(&image, 67, (37u - 67u) & 0xfffff, 5);
    put(&image, 72, (47u - 72u) & 0xfffff, 5);
    uint32_t crc = 0;
    for (unsigned i = 5; i < 86; ++i)
        crc = (crc >> 4) ^ (((crc ^ image.data[i]) & 15) * 0x1081);
    put(&image, 86, crc, 4);
    assert(hpclang_rpl_reference_visit(&reader, 0, 90, 0, collect, &edges) == 0);
    assert(edges.count == 4);
    const uint32_t targets[] = {57, 37, 37, 47}, kinds[] = {3, 4, 5, 5};
    for (unsigned i = 0; i < 4; ++i)
        assert(edges.items[i].kind == HPCLANG_RPL_REFERENCE_DIRECT &&
               edges.items[i].target == targets[i] && edges.items[i].extent == 86 - targets[i] &&
               edges.items[i].auxiliary == kinds[i]);
    image.data[86] ^= 1;
    edges.count = 0;
    assert(hpclang_rpl_reference_visit(&reader, 0, 90, 0, collect, &edges) ==
           HPCLANG_RPL_SPAN_MALFORMED);
    assert(edges.count == 0);
}

int main(void)
{
    nested_array();
    indices_and_opaque();
    linked_aliases();
    native_envelopes();
    return 0;
}
