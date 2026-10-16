#include "rpl-reference.h"
#include "rpl-port.h"
#include <assert.h>
#include <string.h>

struct image
{
    uint8_t data[128];
    uint32_t size, fail;
    uint32_t home;
    int expose_home;
};

static int read_nibble(void *context, uint32_t at, uint8_t *value)
{
    struct image *image = context;
    if (at == image->fail)
        return 0;
    if (image->expose_home && at >= 0x80711 && at < 0x80716) {
        *value = (image->home >> (4 * (at - 0x80711))) & 15;
        return 1;
    }
    if (at >= image->size)
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

static void directories(void)
{
    struct image image = {.size = 87, .fail = UINT32_MAX, .home = 0, .expose_home = 1};
    struct hpclang_rpl_reader reader = {read_nibble, &image};
    put(&image, 0, 0x2a96, 5);
    put(&image, 5, 1, 3);
    put(&image, 8, 275, 3);
    put(&image, 11, 0x865d2, 5);
    put(&image, 16, 0, 5);
    put(&image, 21, 49, 5); // Newest null-name entry at 70.
    put(&image, 26, 0, 5);
    put(&image, 31, 1, 2);
    put(&image, 33, 'A', 2);
    put(&image, 35, 1, 2);
    put(&image, 37, 0x29bf, 5);
    put(&image, 42, 'B', 2);
    put(&image, 65, 34, 5); // Older item at 31.
    put(&image, 70, 0, 2);
    put(&image, 72, 0x2d9d, 5);
    put(&image, 77, 0x81234, 5);
    put(&image, 82, 0x312b, 5);
    struct image original = image;
    struct edges edges = {0};
    assert(hpclang_rpl_reference_visit(&reader, 0, image.size, 0, collect, &edges) == 0);
    assert(edges.count == 3);
    assert(edges.items[0].kind == HPCLANG_RPL_REFERENCE_ATTACHMENT && edges.items[0].field == 11 &&
           edges.items[0].target == 0x865d2 && edges.items[0].auxiliary == 275 &&
           edges.items[0].extent == 0);
    assert(edges.items[1].kind == HPCLANG_RPL_REFERENCE_INLINE && edges.items[1].target == 72 &&
           edges.items[1].extent == 15);
    assert(edges.items[2].kind == HPCLANG_RPL_REFERENCE_INLINE && edges.items[2].target == 37 &&
           edges.items[2].extent == 7);
    assert(memcmp(&image, &original, sizeof image) == 0);
    const uint32_t reads[] = {0, 5, 8, 11, 16, 21, 70, 72, 77, 82, 65, 31, 33, 35, 37, 0x80711};
    for (unsigned i = 0; i < sizeof reads / sizeof reads[0]; ++i) {
        image.fail = reads[i];
        edges.count = 0;
        assert(hpclang_rpl_reference_visit(&reader, 0, image.size, 0, collect, &edges) < 0);
    }
    image = original;
    edges = (struct edges){.stop = 79};
    assert(hpclang_rpl_reference_visit(&reader, 0, image.size, 0, collect, &edges) == 79);
    assert(edges.count == 1);
    edges = (struct edges){0};
    put(&image, 37, 0x2a2c, 5);
    put(&image, 42, 60, 5); // Older object overlaps the next entry's metadata.
    assert(hpclang_rpl_reference_visit(&reader, 0, image.size, 0, collect, &edges) ==
           HPCLANG_RPL_SPAN_BOUNDS);
    image = original;
    edges.count = 0;
    put(&image, 31, 255, 2); // A damaged name cannot read outside the directory.
    assert(hpclang_rpl_reference_visit(&reader, 0, image.size, 0, collect, &edges) ==
           HPCLANG_RPL_SPAN_BOUNDS);
    image = original;
    edges.count = 0;
    put(&image, 65, 44, 5); // A backlink into the chain header is rejected.
    assert(hpclang_rpl_reference_visit(&reader, 0, image.size, 0, collect, &edges) < 0);
    image = original;
    put(&image, 72, 0x2a96, 5);
    put(&image, 77, 0, 3);
    put(&image, 80, 0, 5);
    edges.count = 0;
    assert(hpclang_rpl_reference_visit(&reader, 72, 13, 0, collect, &edges) == 0);
    assert(edges.count == 0);
    put(&image, 77, 0x7ff, 3);
    assert(hpclang_rpl_reference_visit(&reader, 72, 13, 0, collect, &edges) == 0);
    assert(edges.count == 0);
    put(&image, 77, 1, 3);
    assert(hpclang_rpl_reference_visit(&reader, 72, 13, 0, collect, &edges) == 0);
    assert(edges.count == 1 && edges.items[0].kind == HPCLANG_RPL_REFERENCE_OPAQUE &&
           edges.items[0].target == 77 && edges.items[0].extent == 3);
}

static void relocation_patches(void)
{
    const struct hpclang_rpl_reference_patch sentinel = {71, 72};
    struct hpclang_rpl_reference_patch patch = sentinel;
    struct hpclang_rpl_reference edge = {HPCLANG_RPL_REFERENCE_DIRECT, 0x200, 0x110, 0, 0};
    assert(hpclang_rpl_reference_plan_shift(&edge, 0x100, 0x180, 16, &patch) == 1);
    assert(patch.field == 0x200 && patch.value == 0x120);
    assert(hpclang_rpl_reference_plan_shift(&edge, 0x100, 0x180, -16, &patch) == 1);
    assert(patch.value == 0x100);
    edge.kind = HPCLANG_RPL_REFERENCE_ATTACHMENT;
    edge.auxiliary = 275; /* Library ID is not a bank or relative-field flag. */
    assert(hpclang_rpl_reference_plan_shift(&edge, 0x100, 0x180, 16, &patch) == 1);
    assert(patch.value == 0x120);
    edge.kind = HPCLANG_RPL_REFERENCE_ACPTR;
    patch = sentinel;
    assert(hpclang_rpl_reference_plan_shift(&edge, 0x100, 0x180, 16, &patch) ==
           HPCLANG_RPL_SPAN_UNSUPPORTED);
    assert(!memcmp(&patch, &sentinel, sizeof patch));
    edge.auxiliary = 0;
    assert(hpclang_rpl_reference_plan_shift(&edge, 0x100, 0x180, 16, &patch) == 1);
    assert(patch.value == 0x120);
    edge = (struct hpclang_rpl_reference){HPCLANG_RPL_REFERENCE_DIRECT, 0x120, 0x100, 0, 1};
    patch = sentinel;
    assert(hpclang_rpl_reference_plan_shift(&edge, 0x100, 0x130, 16, &patch) == 0);
    assert(!memcmp(&patch, &sentinel, sizeof patch));
    assert(hpclang_rpl_reference_plan_shift(&edge, 0x100, 0x110, 16, &patch) == 1);
    assert(patch.field == 0x120 && patch.value == 0xffff0);
    assert(hpclang_rpl_reference_plan_shift(&edge, 0x120, 0x125, 16, &patch) == 1);
    assert(patch.value == 0xfffd0);
    patch = sentinel;
    assert(hpclang_rpl_reference_plan_shift(&edge, 0x122, 0x130, 16, &patch) ==
           HPCLANG_RPL_SPAN_BOUNDS);
    assert(hpclang_rpl_reference_plan_shift(&edge, 0x100, 0x123, 16, &patch) ==
           HPCLANG_RPL_SPAN_BOUNDS);
    assert(hpclang_rpl_reference_plan_shift(&edge, 0x100, 0x130, INT32_MIN, &patch) ==
           HPCLANG_RPL_SPAN_BOUNDS);
    assert(hpclang_rpl_reference_plan_shift(&edge, 0x100, 0x100000, 1, &patch) ==
           HPCLANG_RPL_SPAN_BOUNDS);
    assert(!memcmp(&patch, &sentinel, sizeof patch));
    edge.kind = HPCLANG_RPL_REFERENCE_OPAQUE;
    assert(hpclang_rpl_reference_plan_shift(&edge, 0x100, 0x130, 16, &patch) ==
           HPCLANG_RPL_SPAN_UNSUPPORTED);
    const enum hpclang_rpl_reference_kind unchanged[] = {
        HPCLANG_RPL_REFERENCE_INLINE, HPCLANG_RPL_REFERENCE_COMPACT, HPCLANG_RPL_REFERENCE_ROMP,
        HPCLANG_RPL_REFERENCE_FLASHP};
    for (unsigned i = 0; i < sizeof unchanged / sizeof unchanged[0]; ++i) {
        edge.kind = unchanged[i];
        assert(!hpclang_rpl_reference_plan_shift(&edge, 0x100, 0x130, 16, &patch));
        assert(!memcmp(&patch, &sentinel, sizeof patch));
    }
    assert(hpclang_rpl_reference_plan_shift(NULL, 0x100, 0x130, 16, &patch) < 0);
    assert(hpclang_rpl_reference_plan_shift(&edge, 0x100, 0x130, 16, NULL) < 0);
}

static int shifted_read(void *context, uint32_t address, uint8_t *value)
{
    if (address < 0x290cu)
        return 0;
    return read_nibble(context, address - 0x290cu, value);
}

static void execution_entries(void)
{
    struct image image = {.size = 16, .fail = UINT32_MAX};
    const struct hpclang_rpl_reader reader = {read_nibble, &image};
    struct hpclang_rpl_entry entry, saved;
    memset(&saved, 0x5a, sizeof saved);
    put(&image, 0, 5, 5);
    assert(!hpclang_rpl_entry_inspect(&reader, 0, 6, &entry));
    assert(entry.kind == HPCLANG_RPL_ENTRY_SELF_DISPATCH && entry.dispatch == 5 && !entry.span);
    for (uint32_t i = 0; i < 6; ++i) {
        image.fail = i;
        entry = saved;
        assert(hpclang_rpl_entry_inspect(&reader, 0, 6, &entry) == HPCLANG_RPL_SPAN_UNAVAILABLE);
        assert(!memcmp(&entry, &saved, sizeof entry));
    }
    image.fail = UINT32_MAX;
    for (uint32_t limit = 0; limit < 6; ++limit) {
        entry = saved;
        assert(hpclang_rpl_entry_inspect(&reader, 0, limit, &entry) == HPCLANG_RPL_SPAN_BOUNDS);
        assert(!memcmp(&entry, &saved, sizeof entry));
    }
    image.data[5] = 16;
    entry = saved;
    assert(hpclang_rpl_entry_inspect(&reader, 0, 6, &entry) == HPCLANG_RPL_SPAN_MALFORMED);
    assert(!memcmp(&entry, &saved, sizeof entry));
    image.data[5] = 0;
    put(&image, 0, 7, 5);
    assert(hpclang_rpl_entry_inspect(&reader, 0, 16, &entry) == HPCLANG_RPL_SPAN_UNSUPPORTED);
    assert(!memcmp(&entry, &saved, sizeof entry));
    /* At this address a known binary-integer prologue also equals self+5.
     * Object semantics must win, including truncation failures. */
    const struct hpclang_rpl_reader shifted = {shifted_read, &image};
    put(&image, 0, 0x2911u, 5);
    assert(!hpclang_rpl_entry_inspect(&shifted, 0x290cu, 10, &entry));
    assert(entry.kind == HPCLANG_RPL_ENTRY_OBJECT && entry.span == 10);
    entry = saved;
    assert(hpclang_rpl_entry_inspect(&shifted, 0x290cu, 6, &entry) == HPCLANG_RPL_SPAN_BOUNDS);
    assert(!memcmp(&entry, &saved, sizeof entry));
    assert(hpclang_rpl_entry_inspect(&reader, 0xffffcu, 5, &entry) == HPCLANG_RPL_SPAN_BOUNDS);
    assert(hpclang_rpl_entry_inspect(&reader, 0, UINT32_MAX, &entry) == HPCLANG_RPL_SPAN_BOUNDS);
    assert(hpclang_rpl_entry_inspect(NULL, 0, 6, &entry) == HPCLANG_RPL_SPAN_UNAVAILABLE);
}

static void dispatch_schemas(void)
{
    struct image image = {.size = 20, .fail = UINT32_MAX};
    struct hpclang_rpl_reader reader = {read_nibble, &image};
    struct hpclang_rpl_dispatch_descriptor descriptor = {80,
                                                         HPCLANG_RPL_DISPATCH_DUPLICATE_THEN_ENTRY};
    put(&image, 0, 80, 5);
    put(&image, 5, 90, 5);
    put(&image, 10, 0x2a2c, 5);
    struct image original = image;
    struct edges edges = {0};
    assert(!hpclang_rpl_dispatch_reference_visit(&reader, 0, 15, &descriptor, collect, &edges));
    assert(edges.count == 2);
    assert(edges.items[0].kind == HPCLANG_RPL_REFERENCE_EXECUTION && edges.items[0].field == 0 &&
           edges.items[0].target == 80 && !edges.items[0].extent);
    assert(edges.items[1].kind == HPCLANG_RPL_REFERENCE_EMBEDDED_ENTRY &&
           edges.items[1].field == 5 && edges.items[1].target == 5 && !edges.items[1].extent);
    struct hpclang_rpl_reference_patch patch = {123, 456}, saved = patch;
    assert(!hpclang_rpl_reference_plan_shift(&edges.items[1], 0, 15, 32, &patch));
    assert(!memcmp(&patch, &saved, sizeof patch));
    assert(hpclang_rpl_reference_plan_shift(&edges.items[0], 0, 100, 32, &patch) ==
           HPCLANG_RPL_SPAN_UNSUPPORTED);
    assert(!memcmp(&patch, &saved, sizeof patch));

    edges.count = 0;
    assert(hpclang_rpl_dispatch_reference_visit(&reader, 0, 5, &descriptor, collect, &edges) ==
           HPCLANG_RPL_SPAN_BOUNDS);
    image.fail = 5;
    assert(hpclang_rpl_dispatch_reference_visit(&reader, 0, 6, &descriptor, collect, &edges) ==
           HPCLANG_RPL_SPAN_UNAVAILABLE);
    assert(!edges.count);
    image.fail = UINT32_MAX;
    edges.stop = 37;
    assert(hpclang_rpl_dispatch_reference_visit(&reader, 0, 6, &descriptor, collect, &edges) == 37);
    assert(edges.count == 1);
    edges = (struct edges){0};
    descriptor = (struct hpclang_rpl_dispatch_descriptor){90, HPCLANG_RPL_DISPATCH_TYPE_PREDICATE};
    edges.count = 0;
    assert(!hpclang_rpl_dispatch_reference_visit(&reader, 5, 10, &descriptor, collect, &edges));
    /* The type prologue is a scalar, even if it numerically falls in a moved region. */
    assert(edges.count == 1 && edges.items[0].kind == HPCLANG_RPL_REFERENCE_EXECUTION &&
           edges.items[0].field == 5 && edges.items[0].target == 90);
    assert(!memcmp(&image, &original, sizeof image));
    for (uint32_t at = 5; at < 15; ++at) {
        image.fail = at;
        edges.count = 0;
        assert(hpclang_rpl_dispatch_reference_visit(&reader, 5, 10, &descriptor, collect, &edges) ==
               HPCLANG_RPL_SPAN_UNAVAILABLE);
        assert(!edges.count);
    }
    image.fail = UINT32_MAX;
    for (uint32_t size = 0; size < 10; ++size) {
        edges.count = 0;
        assert(hpclang_rpl_dispatch_reference_visit(&reader, 5, size, &descriptor, collect,
                                                    &edges) == HPCLANG_RPL_SPAN_BOUNDS);
        assert(!edges.count);
    }
    image.data[14] = 16;
    assert(hpclang_rpl_dispatch_reference_visit(&reader, 5, 10, &descriptor, collect, &edges) ==
           HPCLANG_RPL_SPAN_MALFORMED);
    image = original;
    edges.stop = 37;
    assert(hpclang_rpl_dispatch_reference_visit(&reader, 5, 10, &descriptor, collect, &edges) ==
           37);
    edges = (struct edges){0};
    descriptor.dispatch = 91;
    assert(hpclang_rpl_dispatch_reference_visit(&reader, 5, 10, &descriptor, collect, &edges) ==
           HPCLANG_RPL_SPAN_UNSUPPORTED);
    descriptor.dispatch = 0x2a2c;
    assert(hpclang_rpl_dispatch_reference_visit(&reader, 10, 10, &descriptor, collect, &edges) ==
           HPCLANG_RPL_SPAN_UNSUPPORTED);
    descriptor.dispatch = 90;
    descriptor.schema = (enum hpclang_rpl_dispatch_schema)99;
    assert(hpclang_rpl_dispatch_reference_visit(&reader, 5, 10, &descriptor, collect, &edges) ==
           HPCLANG_RPL_SPAN_UNSUPPORTED);
    assert(!edges.count);
    assert(hpclang_rpl_dispatch_reference_visit(&reader, 0xffffc, 10, &descriptor, collect,
                                                &edges) == HPCLANG_RPL_SPAN_BOUNDS);
    assert(hpclang_rpl_dispatch_reference_visit(&reader, 5, 10, NULL, collect, &edges) ==
           HPCLANG_RPL_SPAN_UNAVAILABLE);
}

static void qualified_relocation(void)
{
    struct hpclang_rpl_reference edge = {HPCLANG_RPL_REFERENCE_ACPTR, 0x100, 0x120, 0, 0x23456};
    struct hpclang_rpl_reference_mappings maps = {1, 8, 8};
    struct hpclang_rpl_reference_patch patch = {123, 456}, saved = patch;
    assert(hpclang_rpl_reference_plan_shift(&edge, 0x100, 0x180, -32, &patch) ==
           HPCLANG_RPL_SPAN_UNSUPPORTED);
    assert(hpclang_rpl_reference_plan_qualified_shift(&edge, &maps, 0x100, 0x180, -32, &patch) ==
           1);
    assert(patch.field == 0x100 && patch.value == 0x100);
    maps.target = 9; /* Same native address in another bank must not move. */
    patch = saved;
    assert(!hpclang_rpl_reference_plan_qualified_shift(&edge, &maps, 0x100, 0x180, -32, &patch));
    assert(!memcmp(&patch, &saved, sizeof patch));
    maps.field = 8;
    assert(!hpclang_rpl_reference_plan_qualified_shift(&edge, &maps, 0x100, 0x180, -32, &patch));
    assert(!memcmp(&patch, &saved, sizeof patch));
    /* A relative library field changes if either end moves, but not when both
     * move equally. The returned field always addresses the old source bytes. */
    edge.kind = HPCLANG_RPL_REFERENCE_DIRECT;
    edge.auxiliary = 1;
    assert(hpclang_rpl_reference_plan_qualified_shift(&edge, &maps, 0x100, 0x180, -32, &patch) ==
           1);
    assert(patch.field == 0x100 && patch.value == 0x40);
    maps.target = 8;
    patch = saved;
    assert(!hpclang_rpl_reference_plan_qualified_shift(&edge, &maps, 0x100, 0x180, -32, &patch));
    assert(!memcmp(&patch, &saved, sizeof patch));
    maps.field = 1;
    assert(hpclang_rpl_reference_plan_qualified_shift(&edge, &maps, 0x100, 0x180, -32, &patch) ==
           1);
    assert(patch.value == 0);
    /* An overlapping numeric field in a different mapping is not a torn field. */
    edge = (struct hpclang_rpl_reference){HPCLANG_RPL_REFERENCE_ACPTR, 0xfe, 0x120, 0, 0x23456};
    assert(hpclang_rpl_reference_plan_qualified_shift(&edge, &maps, 0x100, 0x180, -32, &patch) ==
           1);
    maps.field = 8;
    patch = saved;
    assert(hpclang_rpl_reference_plan_qualified_shift(&edge, &maps, 0x100, 0x180, -32, &patch) ==
           HPCLANG_RPL_SPAN_BOUNDS);
    assert(!memcmp(&patch, &saved, sizeof patch));
    maps.field = 1;
    assert(hpclang_rpl_reference_plan_qualified_shift(&edge, &maps, 0x100, 0x180, -0x101, &patch) ==
           HPCLANG_RPL_SPAN_BOUNDS);
    assert(hpclang_rpl_reference_plan_qualified_shift(&edge, NULL, 0x100, 0x180, -32, &patch) ==
           HPCLANG_RPL_SPAN_MALFORMED);
    edge.kind = HPCLANG_RPL_REFERENCE_EXECUTION;
    assert(hpclang_rpl_reference_plan_qualified_shift(&edge, &maps, 0x100, 0x180, -32, &patch) ==
           HPCLANG_RPL_SPAN_UNSUPPORTED);
    assert(!memcmp(&patch, &saved, sizeof patch));
}

struct mapped_images
{
    struct image banks[2];
    unsigned reads;
};

static int mapped_read(void *context, uint32_t mapping, uint32_t address, uint8_t *value)
{
    struct mapped_images *images = context;
    ++images->reads;
    return mapping < 2 && read_nibble(&images->banks[mapping], address, value);
}

static void batch_preflight(void)
{
    struct mapped_images images = {
        .banks = {{.size = 20, .fail = UINT32_MAX}, {.size = 20, .fail = UINT32_MAX}}};
    put(&images.banks[0], 5, 0x12345, 5);
    put(&images.banks[1], 5, 0x54321, 5);
    put(&images.banks[0], 10, 0x67890, 5);
    struct hpclang_rpl_reference_edit edits[] = {
        {0, 5, 0x12345, 0x12340}, {1, 5, 0x54321, 0x54320}, {0, 10, 0x67890, 0x67880}};
    struct hpclang_rpl_reference_reader reader = {mapped_read, &images};
    struct mapped_images original = images;
    assert(!hpclang_rpl_reference_edits_validate(NULL, NULL, 0));
    assert(!hpclang_rpl_reference_edits_validate(&reader, edits, 3));
    assert(images.reads == 15 && !memcmp(images.banks, original.banks, sizeof images.banks));
    /* Every read can fail, including the final nibble of the final edit. */
    for (unsigned i = 0; i < 3; ++i) {
        for (uint32_t j = 0; j < 5; ++j) {
            images = original;
            images.banks[edits[i].mapping].fail = edits[i].field + j;
            assert(hpclang_rpl_reference_edits_validate(&reader, edits, 3) ==
                   HPCLANG_RPL_SPAN_UNAVAILABLE);
            images.banks[edits[i].mapping].fail = UINT32_MAX;
            assert(!memcmp(images.banks, original.banks, sizeof images.banks));
        }
    }
    images = original;
    images.banks[0].data[14] ^= 1;
    assert(hpclang_rpl_reference_edits_validate(&reader, edits, 3) == HPCLANG_RPL_SPAN_MALFORMED);
    images.banks[0].data[14] = 16;
    assert(hpclang_rpl_reference_edits_validate(&reader, edits, 3) == HPCLANG_RPL_SPAN_MALFORMED);
    /* Check both overlap orders, exact duplicates and endpoint adjacency. */
    struct hpclang_rpl_reference_edit saved = edits[1];
    edits[1].mapping = 0;
    for (uint32_t field = 1; field < 10; ++field) {
        edits[1].field = field;
        images = original;
        assert(hpclang_rpl_reference_edits_validate(&reader, edits, 2) ==
               HPCLANG_RPL_SPAN_MALFORMED);
        assert(!images.reads);
    }
    edits[1] = saved;
    images = original;
    edits[2].field = 0xffffc;
    assert(hpclang_rpl_reference_edits_validate(&reader, edits, 3) == HPCLANG_RPL_SPAN_BOUNDS);
    assert(!images.reads); /* Even late metadata errors precede all reads. */
    edits[2].field = 10;
    edits[2].value = 0x100000;
    assert(hpclang_rpl_reference_edits_validate(&reader, edits, 3) == HPCLANG_RPL_SPAN_BOUNDS);
    edits[2].value = 0x67880;
    edits[2].expected = 0x100000;
    assert(hpclang_rpl_reference_edits_validate(&reader, edits, 3) == HPCLANG_RPL_SPAN_BOUNDS);
    assert(!images.reads);
    assert(hpclang_rpl_reference_edits_validate(&reader, NULL, 1) == HPCLANG_RPL_SPAN_UNAVAILABLE);
    assert(hpclang_rpl_reference_edits_validate(NULL, edits, 1) == HPCLANG_RPL_SPAN_UNAVAILABLE);
}

static void metadata_guards(void)
{
    struct mapped_images images = {
        .banks = {{.size = 40, .fail = UINT32_MAX}, {.size = 40, .fail = UINT32_MAX}}};
    struct hpclang_rpl_reference_reader reader = {mapped_read, &images};
    struct hpclang_rpl_reference_guard guards[] = {
        {0, 0, 2, 3}, {0, 3, 275, 3}, {0, 16, 7, 3}, {1, 3, 123, 3}};
    for (unsigned i = 0; i < 4; ++i)
        put(&images.banks[guards[i].mapping], guards[i].field, guards[i].expected, 3);
    assert(!hpclang_rpl_reference_guards_validate(NULL, NULL, 0));
    assert(!hpclang_rpl_reference_guards_validate(&reader, guards, 4));
    struct mapped_images saved = images;
    for (unsigned i = 0; i < 4; ++i) {
        for (unsigned j = 0; j < 3; ++j) {
            images = saved;
            images.banks[guards[i].mapping].data[guards[i].field + j] ^= 1;
            assert(hpclang_rpl_reference_guards_validate(&reader, guards, 4) ==
                   HPCLANG_RPL_SPAN_MALFORMED);
            images = saved;
            images.banks[guards[i].mapping].fail = guards[i].field + j;
            assert(hpclang_rpl_reference_guards_validate(&reader, guards, 4) ==
                   HPCLANG_RPL_SPAN_UNAVAILABLE);
        }
    }
    images = saved;
    images.reads = 0;
    guards[3].width = 0;
    assert(hpclang_rpl_reference_guards_validate(&reader, guards, 4) == HPCLANG_RPL_SPAN_MALFORMED);
    guards[3].width = 6;
    assert(hpclang_rpl_reference_guards_validate(&reader, guards, 4) == HPCLANG_RPL_SPAN_MALFORMED);
    guards[3].width = 3;
    guards[3].expected = 0x1000;
    assert(hpclang_rpl_reference_guards_validate(&reader, guards, 4) == HPCLANG_RPL_SPAN_BOUNDS);
    guards[3].expected = 123;
    guards[3].field = 0xffffe;
    assert(hpclang_rpl_reference_guards_validate(&reader, guards, 4) == HPCLANG_RPL_SPAN_BOUNDS);
    assert(!images.reads);
    guards[3] = guards[0]; /* Overlapping read-only dependencies are permitted. */
    assert(!hpclang_rpl_reference_guards_validate(&reader, guards, 4));
    images.banks[0].data[0] = 16;
    assert(hpclang_rpl_reference_guards_validate(&reader, guards, 4) == HPCLANG_RPL_SPAN_MALFORMED);
    assert(hpclang_rpl_reference_guards_validate(NULL, guards, 4) == HPCLANG_RPL_SPAN_UNAVAILABLE);
    assert(hpclang_rpl_reference_guards_validate(&reader, NULL, 4) == HPCLANG_RPL_SPAN_UNAVAILABLE);
    for (uint32_t width = 1; width <= 5; ++width) {
        struct hpclang_rpl_reference_guard guard = {0, 0, (1u << (4 * width)) - 1, width};
        put(&images.banks[0], 0, guard.expected, width);
        assert(!hpclang_rpl_reference_guards_validate(&reader, &guard, 1));
        ++guard.expected;
        assert(hpclang_rpl_reference_guards_validate(&reader, &guard, 1) ==
               HPCLANG_RPL_SPAN_BOUNDS);
    }
}

struct edit_memory
{
    uint8_t banks[2][32];
    unsigned reads, writes, fail_read, fail_write;
    int acknowledge_failure, silent_write;
};

static int edit_read(void *context, uint32_t mapping, uint32_t at, uint8_t *value)
{
    struct edit_memory *memory = context;
    assert(mapping < 2 && at < 32);
    if (++memory->reads == memory->fail_read)
        return 0;
    *value = memory->banks[mapping][at];
    return 1;
}

static int edit_write(void *context, uint32_t mapping, uint32_t at, uint8_t value)
{
    struct edit_memory *memory = context;
    assert(mapping < 2 && at >= 5 && at < 10 && value < 16);
    int failed = ++memory->writes == memory->fail_write;
    if ((!failed || memory->acknowledge_failure) && !memory->silent_write)
        memory->banks[mapping][at] = value;
    return !failed;
}

static void batch_replay(void)
{
    struct edit_memory initial = {0}, completed;
    memset(initial.banks, 7, sizeof initial.banks);
    struct hpclang_rpl_reference_edit edits[] = {{0, 5, 0x12345, 0xabcde},
                                                 {1, 5, 0x54321, 0xedcba}};
    for (unsigned i = 0; i < 2; ++i)
        for (unsigned j = 0; j < 5; ++j)
            initial.banks[i][5 + j] = (uint8_t)((edits[i].expected >> (4 * j)) & 15);
    completed = initial;
    for (unsigned i = 0; i < 2; ++i)
        for (unsigned j = 0; j < 5; ++j)
            completed.banks[i][5 + j] = (uint8_t)((edits[i].value >> (4 * j)) & 15);
    struct edit_memory memory = initial;
    struct hpclang_rpl_reference_writer writer = {{edit_read, &memory}, edit_write};
    assert(!hpclang_rpl_reference_edits_replay(NULL, NULL, 0));
    assert(!hpclang_rpl_reference_edits_replay(&writer, edits, 2));
    assert(!memcmp(memory.banks, completed.banks, sizeof memory.banks));
    unsigned reads = memory.reads, writes = memory.writes;
    assert(writes == 10);
    assert(!hpclang_rpl_reference_edits_replay(&writer, edits, 2));
    assert(memory.writes == writes);
    for (unsigned cut = 1; cut <= reads; ++cut) {
        memory = initial;
        memory.fail_read = cut;
        assert(hpclang_rpl_reference_edits_replay(&writer, edits, 2) ==
               HPCLANG_RPL_SPAN_UNAVAILABLE);
        if (cut <= 10)
            assert(!memory.writes);
        memory.fail_read = 0;
        assert(!hpclang_rpl_reference_edits_replay(&writer, edits, 2));
        assert(!memcmp(memory.banks, completed.banks, sizeof memory.banks));
    }
    for (unsigned cut = 1; cut <= writes; ++cut) {
        for (int committed = 0; committed <= 1; ++committed) {
            memory = initial;
            memory.fail_write = cut;
            memory.acknowledge_failure = committed;
            assert(hpclang_rpl_reference_edits_replay(&writer, edits, 2) ==
                   HPCLANG_RPL_SPAN_UNAVAILABLE);
            memory.fail_write = 0;
            assert(!hpclang_rpl_reference_edits_replay(&writer, edits, 2));
            assert(!memcmp(memory.banks, completed.banks, sizeof memory.banks));
        }
    }
    memory = initial;
    memory.banks[1][9] = 0; /* A late changed field prevents even the first write. */
    assert(hpclang_rpl_reference_edits_replay(&writer, edits, 2) == HPCLANG_RPL_SPAN_MALFORMED);
    assert(!memory.writes);
    memory = initial;
    edits[1].mapping = 0; /* Same-address edits in distinct banks above were valid. */
    assert(hpclang_rpl_reference_edits_replay(&writer, edits, 2) == HPCLANG_RPL_SPAN_MALFORMED);
    assert(!memory.reads && !memory.writes);
    edits[1].mapping = 1;
    memory.silent_write = 1;
    assert(hpclang_rpl_reference_edits_replay(&writer, edits, 2) == HPCLANG_RPL_SPAN_MALFORMED);
    assert(!memcmp(memory.banks, initial.banks, sizeof memory.banks));
}

struct rebind_memory
{
    struct image image;
    uint32_t field;
    unsigned writes, fail_write;
};

static int rebind_read(void *context, uint32_t mapping, uint32_t at, uint8_t *value)
{
    struct rebind_memory *memory = context;
    assert(!mapping);
    return read_nibble(&memory->image, at, value);
}

static int rebind_write(void *context, uint32_t mapping, uint32_t at, uint8_t value)
{
    struct rebind_memory *memory = context;
    assert(!mapping && at >= memory->field && at < memory->field + 5 && value < 16);
    memory->image.data[at] = value;
    return ++memory->writes != memory->fail_write;
}

static void rebuilt_catalog_replay(void)
{
    const struct hpclang_rpl_catalog_rebind identity = {275, 0x22f5a, 0x12345, 0xabcde};
    for (unsigned cut = 1; cut <= 5; ++cut) {
        struct rebind_memory memory = {
            .image = {.size = 128, .fail = UINT32_MAX}, .field = 11, .fail_write = cut};
        put(&memory.image, 5, 1, 3);
        put(&memory.image, 8, identity.number, 3);
        put(&memory.image, 11, identity.previous, 5);
        put(&memory.image, 16, identity.bank_switch, 5);
        struct hpclang_rpl_reader catalog = {read_nibble, &memory.image};
        struct hpclang_rpl_reference_writer writer = {{rebind_read, &memory}, rebind_write};
        uint32_t field = 0;
        assert(hpclang_rpl_catalog_rebind_field(&catalog, 5, 19, &identity, &field) == 1);
        struct hpclang_rpl_reference_edit edit = {0, field, identity.previous,
                                                  identity.replacement};
        assert(hpclang_rpl_reference_edits_replay(&writer, &edit, 1) ==
               HPCLANG_RPL_SPAN_UNAVAILABLE);
        /* Rebuild elsewhere with the partial value; discard the saved RAM edit. */
        memcpy(memory.image.data + 64, memory.image.data + 5, 19);
        memset(&edit, 0, sizeof edit);
        memory.field = 70;
        memory.fail_write = 0;
        struct image before = memory.image;
        assert(hpclang_rpl_catalog_rebind_field(&catalog, 64, 19, &identity, &field) == 1);
        assert(field == 70);
        edit =
            (struct hpclang_rpl_reference_edit){0, field, identity.previous, identity.replacement};
        assert(!hpclang_rpl_reference_edits_replay(&writer, &edit, 1));
        for (unsigned at = 0; at < memory.image.size; ++at) {
            if (at >= 70 && at < 75)
                assert(memory.image.data[at] == ((identity.replacement >> (4 * (at - 70))) & 15));
            else
                assert(memory.image.data[at] == before.data[at]);
        }
    }
}

struct catalog_resolver
{
    unsigned calls, fail;
};

static int catalog_mapping(void *context, const struct hpclang_rpl_library_binding *binding,
                           uint32_t *mapping)
{
    struct catalog_resolver *resolver = context;
    if (++resolver->calls == resolver->fail)
        return HPCLANG_RPL_SPAN_UNSUPPORTED;
    if (binding->bank_switch == 0x10)
        *mapping = 1;
    else if (binding->bank_switch == 0x20)
        *mapping = 2;
    else if (!binding->bank_switch)
        *mapping = 0;
    else
        return HPCLANG_RPL_SPAN_UNSUPPORTED;
    return 0;
}

static void catalog_relocation_plan(void)
{
    struct image image = {.size = 51, .fail = UINT32_MAX};
    struct hpclang_rpl_reader reader = {read_nibble, &image};
    put(&image, 0, 3, 3);
    for (unsigned i = 0; i < 3; ++i) {
        put(&image, 3 + 16 * i, i < 2 ? 7 : 8, 3); /* Duplicate ID, distinct bindings. */
        put(&image, 6 + 16 * i, 0x60020, 5);
        put(&image, 11 + 16 * i, i < 2 ? 0x10 + 0x10 * i : 0, 5);
        put(&image, 16 + 16 * i, i + 1, 3);
    }
    struct image original = image;
    struct hpclang_rpl_reference_edit edits[6];
    struct hpclang_rpl_reference_guard guards[7];
    struct hpclang_rpl_catalog_reference_plan plan = {edits, 6, 99, guards, 7, 99};
    const struct hpclang_rpl_catalog_move move = {0, 1, 0x60000, 0x60100, -16};
    struct catalog_resolver resolver = {0};
    assert(!hpclang_rpl_catalog_plan_relocation(&reader, 0, image.size, &move, catalog_mapping,
                                                &resolver, &plan));
    assert(plan.edit_count == 6 && plan.guard_count == 7 && resolver.calls == 3);
    assert(guards[0].field == 0 && guards[0].expected == 3 && guards[0].width == 3);
    for (unsigned i = 0; i < 3; ++i) {
        assert(edits[2 * i].mapping == 0 && edits[2 * i].field == 6 + 16 * i);
        assert(edits[2 * i].expected == 0x60020);
        assert(edits[2 * i].value == (i ? 0x60020 : 0x60010));
        assert(edits[2 * i + 1].field == 11 + 16 * i);
        assert(edits[2 * i + 1].expected == (i < 2 ? 0x10 + 0x10 * i : 0));
        assert(edits[2 * i + 1].value == edits[2 * i + 1].expected);
        assert(guards[2 * i + 1].field == 3 + 16 * i);
        assert(guards[2 * i + 1].expected == (i < 2 ? 7 : 8));
        assert(guards[2 * i + 2].field == 16 + 16 * i);
        assert(guards[2 * i + 2].expected == i + 1);
    }
    for (unsigned at = 0; at < image.size; ++at) {
        image.fail = at;
        assert(hpclang_rpl_catalog_plan_relocation(&reader, 0, image.size, &move, catalog_mapping,
                                                   &resolver,
                                                   &plan) == HPCLANG_RPL_SPAN_UNAVAILABLE);
        assert(!plan.edit_count && !plan.guard_count);
    }
    image = original;
    resolver = (struct catalog_resolver){0, 2};
    assert(hpclang_rpl_catalog_plan_relocation(&reader, 0, image.size, &move, catalog_mapping,
                                               &resolver, &plan) == HPCLANG_RPL_SPAN_UNSUPPORTED);
    assert(!plan.edit_count && !plan.guard_count);
    resolver = (struct catalog_resolver){0};
    plan.edit_capacity = 5;
    assert(hpclang_rpl_catalog_plan_relocation(&reader, 0, image.size, &move, catalog_mapping,
                                               &resolver, &plan) == HPCLANG_RPL_SPAN_BOUNDS);
    assert(!resolver.calls && !plan.edit_count && !plan.guard_count);
    plan.edit_capacity = 6;
    plan.guard_capacity = 6;
    assert(hpclang_rpl_catalog_plan_relocation(&reader, 0, image.size, &move, catalog_mapping,
                                               &resolver, &plan) == HPCLANG_RPL_SPAN_BOUNDS);
    assert(!resolver.calls && !plan.edit_count && !plan.guard_count);
    plan.guard_capacity = 7;
    image.data[20] = 16;
    assert(hpclang_rpl_catalog_plan_relocation(&reader, 0, image.size, &move, catalog_mapping,
                                               &resolver, &plan) == HPCLANG_RPL_SPAN_MALFORMED);
    assert(!plan.edit_count && !plan.guard_count);
    image.data[20] = original.data[20];
    assert(!memcmp(&image, &original, sizeof image)); /* No native mutation. */
    put(&image, 0, 0, 3);
    plan.edits = NULL;
    plan.edit_capacity = 0;
    assert(!hpclang_rpl_catalog_plan_relocation(&reader, 0, 3, &move, NULL, NULL, &plan));
    assert(!plan.edit_count && plan.guard_count == 1 && !guards[0].expected);
}

int main(void)
{
    catalog_relocation_plan();
    rebuilt_catalog_replay();
    batch_replay();
    metadata_guards();
    batch_preflight();
    qualified_relocation();
    dispatch_schemas();
    execution_entries();
    relocation_patches();
    nested_array();
    indices_and_opaque();
    linked_aliases();
    native_envelopes();
    directories();
    return 0;
}
