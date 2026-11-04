#include "rpl-reference.h"
#include "rpl-code.h"
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
    unsigned stop_at;
};

static int collect(void *context, const struct hpclang_rpl_reference *edge)
{
    struct edges *edges = context;
    assert(edges->count < 8);
    edges->items[edges->count++] = *edge;
    return !edges->stop_at || edges->count == edges->stop_at ? edges->stop : 0;
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
    assert(edges.count == 2 && edges.items[0].kind == HPCLANG_RPL_REFERENCE_ACPTR);
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

    struct hpclang_rpl_reference_node root = {0, 45, 0}, workspace[1];
    edges = (struct edges){0};
    assert(!hpclang_rpl_reference_visit_nested(&reader, &root, workspace, 1, 3, collect, &edges));
    assert(edges.count == 4 && edges.items[0].kind == HPCLANG_RPL_REFERENCE_INLINE &&
           edges.items[1].kind == HPCLANG_RPL_REFERENCE_COMPACT &&
           edges.items[2].kind == HPCLANG_RPL_REFERENCE_ACPTR);
    assert(edges.items[2].field == 30 && edges.items[2].target == 0x4001c &&
           edges.items[2].auxiliary == 0x23c21);
    assert(edges.items[3].kind == HPCLANG_RPL_REFERENCE_EXECUTION && edges.items[3].field == 35 &&
           edges.items[3].target == 0x23c21);
    assert(!memcmp(&image, &original, sizeof image));
    edges = (struct edges){0};
    assert(hpclang_rpl_reference_visit_nested(&reader, &root, workspace, 1, 2, collect, &edges) ==
           HPCLANG_RPL_SPAN_BOUNDS);
    edges = (struct edges){.stop = 73};
    assert(hpclang_rpl_reference_visit_nested(&reader, &root, workspace, 1, 3, collect, &edges) ==
           73);
    assert(edges.count == 1);
    for (uint32_t at = 30; at < 40; ++at) {
        image.fail = at;
        edges = (struct edges){0};
        assert(hpclang_rpl_reference_visit_nested(&reader, &root, workspace, 1, 3, collect,
                                                  &edges) == HPCLANG_RPL_SPAN_UNAVAILABLE);
    }
}

static void acptr_switch_execution(void)
{
    struct image image = {.size = 15, .fail = UINT32_MAX};
    struct hpclang_rpl_reader reader = {read_nibble, &image};
    put(&image, 0, 0x2baa, 5);
    put(&image, 5, 0x4001c, 5);
    put(&image, 10, 0x22f5a, 5);
    struct image original = image;
    for (unsigned compact = 0; compact < 2; ++compact) {
        struct edges edges = {0};
        uint32_t start = compact ? 5 : 0;
        uint32_t prologue = compact ? 0x2baa : 0;
        assert(!hpclang_rpl_reference_visit(&reader, start, 15 - start, prologue, collect, &edges));
        assert(edges.count == 2 && edges.items[0].kind == HPCLANG_RPL_REFERENCE_ACPTR);
        const struct hpclang_rpl_reference *execution = &edges.items[1];
        assert(execution->kind == HPCLANG_RPL_REFERENCE_EXECUTION && execution->field == 10 &&
               execution->target == 0x22f5a && !execution->extent && !execution->auxiliary);
        struct hpclang_rpl_reference_mappings mappings = {1, 2, 2};
        struct hpclang_rpl_reference_patch patch = {123, 456};
        assert(hpclang_rpl_reference_plan_qualified_shift(execution, &mappings, 0x22000, 0x23000,
                                                          16,
                                                          &patch) == HPCLANG_RPL_SPAN_UNSUPPORTED);
        assert(patch.field == 123 && patch.value == 456);
        assert(hpclang_rpl_reference_plan_execution_shift(execution, &mappings, 0x22000, 0x23000,
                                                          16, &patch) == 1);
        assert(patch.field == 10 && patch.value == 0x22f6a);
        edges = (struct edges){.stop = 73, .stop_at = 2};
        assert(hpclang_rpl_reference_visit(&reader, start, 15 - start, prologue, collect, &edges) ==
               73);
        assert(edges.count == 2);
        assert(!memcmp(&image, &original, sizeof image));
    }
    put(&image, 10, 0, 5);
    struct edges edges = {0};
    assert(!hpclang_rpl_reference_visit(&reader, 0, 15, 0, collect, &edges));
    assert(edges.count == 1 && edges.items[0].kind == HPCLANG_RPL_REFERENCE_ACPTR &&
           !edges.items[0].auxiliary);
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
    struct hpclang_rpl_reference_node root = {0, 45, 0};
    struct hpclang_rpl_reference_node workspace[3] = {{0}, {0}, {123, 456, 789}};
    edges.count = 0;
    assert(!hpclang_rpl_reference_visit_nested(&reader, &root, workspace, 2, 3, collect, &edges));
    assert(edges.count == 6 && edges.items[2].kind == HPCLANG_RPL_REFERENCE_ACPTR &&
           edges.items[4].kind == HPCLANG_RPL_REFERENCE_ACPTR);
    assert(edges.items[2].target == 0x7327e && edges.items[4].target == 0x7327e);
    for (unsigned i = 3; i < 6; i += 2)
        assert(edges.items[i].kind == HPCLANG_RPL_REFERENCE_EXECUTION &&
               edges.items[i].field == 40 && edges.items[i].target == 0x22f5a);
    assert(workspace[2].address == 123 && workspace[2].limit == 456 &&
           workspace[2].body_prologue == 789);
    edges.count = 0;
    assert(hpclang_rpl_reference_visit_nested(&reader, &root, workspace, 1, 3, collect, &edges) ==
           HPCLANG_RPL_SPAN_BOUNDS);
    edges.count = 0;
    assert(hpclang_rpl_reference_visit_nested(&reader, &root, workspace, 2, 2, collect, &edges) ==
           HPCLANG_RPL_SPAN_BOUNDS);
    edges.count = 0;
    put(&image, 30, 4, 5); // Link into the table is not an independent body.
    assert(hpclang_rpl_reference_visit(&reader, 0, 45, 0, collect, &edges) < 0);
    assert(edges.count == 1); // Earlier events must be discarded on error.
}

static void nested_unresolved(void)
{
    struct image image = {.size = 30, .fail = UINT32_MAX};
    struct hpclang_rpl_reader reader = {read_nibble, &image};
    put(&image, 0, 0x2a74, 5);
    put(&image, 5, 0x2dcc, 5); // Opaque code remains visible inside the list.
    put(&image, 10, 10, 5);
    put(&image, 15, 0x81234, 5);
    put(&image, 20, 0x8abcd, 5); // External reference must not be dereferenced.
    put(&image, 25, 0x312b, 5);
    struct hpclang_rpl_reference_node root = {0, 30, 0}, workspace[1];
    struct edges edges = {0};
    assert(!hpclang_rpl_reference_visit_nested(&reader, &root, workspace, 1, 2, collect, &edges));
    assert(edges.count == 3 && edges.items[0].kind == HPCLANG_RPL_REFERENCE_INLINE &&
           edges.items[1].kind == HPCLANG_RPL_REFERENCE_DIRECT &&
           edges.items[2].kind == HPCLANG_RPL_REFERENCE_OPAQUE);
    assert(edges.items[1].target == 0x8abcd && edges.items[2].target == 5 &&
           edges.items[2].extent == 15);
    assert(hpclang_rpl_reference_visit_nested(&reader, &root, workspace, 0, 2, collect, &edges) ==
           HPCLANG_RPL_SPAN_BOUNDS);
    assert(hpclang_rpl_reference_visit_nested(&reader, &root, workspace, 1, 0, collect, &edges) ==
           HPCLANG_RPL_SPAN_BOUNDS);
    assert(hpclang_rpl_reference_visit_nested(&reader, NULL, workspace, 1, 2, collect, &edges) ==
           HPCLANG_RPL_SPAN_UNAVAILABLE);
    assert(hpclang_rpl_reference_visit_nested(&reader, &root, NULL, 1, 2, collect, &edges) ==
           HPCLANG_RPL_SPAN_UNAVAILABLE);
    assert(hpclang_rpl_reference_visit_nested(NULL, &root, workspace, 1, 2, collect, &edges) ==
           HPCLANG_RPL_SPAN_UNAVAILABLE);
    assert(hpclang_rpl_reference_visit_nested(&reader, &root, workspace, 1, 2, NULL, &edges) ==
           HPCLANG_RPL_SPAN_UNAVAILABLE);
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

static void offset_relocation(void)
{
    const struct hpclang_rpl_reference_patch unchanged = {0xabcde, 0x12345};
    struct hpclang_rpl_reference_patch patch = unchanged;
    struct hpclang_rpl_reference_mappings maps = {2, 1, 1};
    struct hpclang_rpl_reference edge = {HPCLANG_RPL_REFERENCE_ACPTR, 0x80000, 0x70011, 0, 123};
    /* Bank 0's 70011 aliases bank 1's 40011; keep the original encoding. */
    assert(hpclang_rpl_reference_plan_offset_shift(&edge, &maps, 0, -0x30000, 0x40000, 0x40100, 16,
                                                   &patch) == 1);
    assert(patch.field == edge.field && patch.value == 0x70021);
    /* An equal-looking address in a proven-disjoint mapping does not move. */
    maps.target = 3;
    patch = unchanged;
    assert(!hpclang_rpl_reference_plan_offset_shift(&edge, &maps, 0, -0x30000, 0x40000, 0x40100, 16,
                                                    &patch));
    assert(!memcmp(&patch, &unchanged, sizeof patch));
    /* Independently translated relative endpoints; compare actual encoded
     * subtraction, including cases where one or both endpoints move. */
    for (unsigned field_moves = 0; field_moves < 2; ++field_moves) {
        for (unsigned target_moves = 0; target_moves < 2; ++target_moves) {
            for (int delta = -32; delta <= 32; delta += 64) {
                maps = (struct hpclang_rpl_reference_mappings){1, 1, 1};
                edge = (struct hpclang_rpl_reference){HPCLANG_RPL_REFERENCE_DIRECT, 0x70000,
                                                      0x60000, 0, 1};
                int32_t field_offset = field_moves ? -0x2fff0 : -0x30020;
                int32_t target_offset = target_moves ? -0x1ffe0 : -0x1ff00;
                uint32_t before = (edge.target - edge.field) & 0xfffff;
                uint32_t expected =
                    (uint32_t)((int64_t)edge.target + (int64_t)target_moves * delta - edge.field -
                               (int64_t)field_moves * delta) &
                    0xfffff;
                patch = unchanged;
                int result = hpclang_rpl_reference_plan_offset_shift(
                    &edge, &maps, field_offset, target_offset, 0x40000, 0x40100, delta, &patch);
                assert(result == (expected != before));
                if (result)
                    assert(patch.field == edge.field && patch.value == expected);
                else
                    assert(!memcmp(&patch, &unchanged, sizeof patch));
            }
        }
    }
    edge = (struct hpclang_rpl_reference){HPCLANG_RPL_REFERENCE_DIRECT, 0x70000, 0x60000, 0, 0};
    /* Every split through the five-nibble field is rejected in translated coordinates. */
    for (int split = 1; split < 5; ++split) {
        patch = unchanged;
        assert(hpclang_rpl_reference_plan_offset_shift(&edge, &maps, -0x30000 - split, 0, 0x40000,
                                                       0x40100, 16,
                                                       &patch) == HPCLANG_RPL_SPAN_BOUNDS);
        assert(hpclang_rpl_reference_plan_offset_shift(&edge, &maps, -0x2ff00 - split, 0, 0x40000,
                                                       0x40100, 16,
                                                       &patch) == HPCLANG_RPL_SPAN_BOUNDS);
        assert(!memcmp(&patch, &unchanged, sizeof patch));
    }
    /* Normalized destinations fit, but native encoding may still overflow. */
    for (unsigned endpoint = 0; endpoint < 2; ++endpoint) {
        for (unsigned high = 0; high < 2; ++high) {
            edge.field = endpoint ? 0x80000 : (high ? 0xffffb : 0);
            edge.target = endpoint ? (high ? 0xfffff : 0) : 0x80000;
            int32_t field_offset = endpoint ? 0 : 0x40010 - (int32_t)edge.field;
            int32_t target_offset = endpoint ? 0x40010 - (int32_t)edge.target : 0;
            patch = unchanged;
            assert(hpclang_rpl_reference_plan_offset_shift(
                       &edge, &maps, field_offset, target_offset, 0x40000, 0x40100, high ? 16 : -32,
                       &patch) == HPCLANG_RPL_SPAN_BOUNDS);
            assert(!memcmp(&patch, &unchanged, sizeof patch));
        }
    }
    edge = (struct hpclang_rpl_reference){HPCLANG_RPL_REFERENCE_DIRECT, 0x70000, 0x60000, 0, 0};
    assert(!hpclang_rpl_reference_plan_offset_shift(&edge, &maps, INT32_MIN, INT32_MAX, 0x40000,
                                                    0x40100, 16, &patch));
    edge.kind = HPCLANG_RPL_REFERENCE_EXECUTION;
    assert(hpclang_rpl_reference_plan_offset_shift(&edge, &maps, 0, 0, 0x40000, 0x40100, 16,
                                                   &patch) == HPCLANG_RPL_SPAN_UNSUPPORTED);
    assert(hpclang_rpl_reference_plan_offset_shift(&edge, NULL, 0, 0, 0x40000, 0x40100, 16,
                                                   &patch) == HPCLANG_RPL_SPAN_MALFORMED);
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
        {0, 5, 0x12345, 0x12340, 5}, {1, 5, 0x54321, 0x54320, 5}, {0, 10, 0x67890, 0x67880, 5}};
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

struct alias_edit_memory
{
    uint8_t data[32];
    int32_t bias;
    unsigned reads, writes, locations, fail_location;
};

static int alias_field(void *context, uint32_t mapping, uint32_t field, uint32_t width,
                       uint32_t *physical)
{
    struct alias_edit_memory *memory = context;
    assert(mapping < 2 && width && width <= 5);
    if (++memory->locations == memory->fail_location)
        return 73;
    *physical = (uint32_t)((int64_t)field + (mapping ? memory->bias : 0));
    return 0;
}

static int alias_read(void *context, uint32_t mapping, uint32_t field, uint8_t *value)
{
    struct alias_edit_memory *memory = context;
    int64_t at = (int64_t)field + (mapping ? memory->bias : 0);
    assert(at >= 0 && at < 32);
    ++memory->reads;
    *value = memory->data[at];
    return 1;
}

static int alias_write(void *context, uint32_t mapping, uint32_t field, uint8_t value)
{
    struct alias_edit_memory *memory = context;
    int64_t at = (int64_t)field + (mapping ? memory->bias : 0);
    assert(at >= 0 && at < 32);
    ++memory->writes;
    memory->data[at] = value;
    return 1;
}

static int repeated_page_field(void *context, uint32_t mapping, uint32_t field, uint32_t width,
                               uint32_t *physical)
{
    (void)context;
    (void)mapping;
    assert(width == 1);
    *physical = field & 7;
    return 0;
}

static void physical_alias_replay(void)
{
    assert(!hpclang_rpl_reference_edits_replay_mapped(NULL, NULL, 0, NULL, NULL));
    const struct hpclang_rpl_reference_edit repeated[] = {{0, 5, 0, 1, 1}, {0, 13, 0, 1, 1}};
    assert(hpclang_rpl_reference_edits_check_aliases(repeated, 2, repeated_page_field, NULL) ==
           HPCLANG_RPL_SPAN_MALFORMED);
    for (uint32_t width = 1; width <= 5; ++width) {
        for (int32_t bias = -8; bias <= 2; ++bias) {
            struct alias_edit_memory memory = {.bias = bias};
            const struct hpclang_rpl_reference_writer writer = {{alias_read, &memory}, alias_write};
            const struct hpclang_rpl_reference_edit edits[] = {{0, 5, 0, 1, 5},
                                                               {1, 9, 0, 1, width}};
            int overlaps = 9 + bias < 10 && 9 + bias + (int32_t)width > 5;
            int result =
                hpclang_rpl_reference_edits_replay_mapped(&writer, edits, 2, alias_field, &memory);
            if (overlaps) {
                assert(result == HPCLANG_RPL_SPAN_MALFORMED);
                assert(!memory.reads && !memory.writes);
                for (unsigned i = 0; i < sizeof memory.data; ++i)
                    assert(!memory.data[i]);
            } else {
                assert(!result && memory.writes == 2);
                /* Replay accepts replacement bytes from an earlier attempt. */
                assert(!hpclang_rpl_reference_edits_replay_mapped(&writer, edits, 2, alias_field,
                                                                  &memory));
                assert(memory.writes == 2);
            }
        }
    }
    struct alias_edit_memory memory = {0};
    const struct hpclang_rpl_reference_writer writer = {{alias_read, &memory}, alias_write};
    struct hpclang_rpl_reference_edit edits[] = {{0, 5, 0, 1, 5}, {1, 15, 0, 1, 5}};
    for (unsigned fail = 1; fail <= 3; ++fail) {
        memory = (struct alias_edit_memory){.fail_location = fail};
        assert(hpclang_rpl_reference_edits_replay_mapped(&writer, edits, 2, alias_field, &memory) ==
               73);
        assert(!memory.reads && !memory.writes);
    }
    memory = (struct alias_edit_memory){0};
    edits[1].width = 6;
    assert(hpclang_rpl_reference_edits_replay_mapped(&writer, edits, 2, alias_field, &memory) ==
           HPCLANG_RPL_SPAN_MALFORMED);
    assert(!memory.locations && !memory.reads && !memory.writes);
    edits[1].width = 5;
    memory.bias = -16; /* Resolved start UINT32_MAX cannot contain five nibbles. */
    assert(hpclang_rpl_reference_edits_replay_mapped(&writer, edits, 2, alias_field, &memory) ==
           HPCLANG_RPL_SPAN_BOUNDS);
    assert(!memory.reads && !memory.writes);
    edits[1].width = 1;
    assert(!hpclang_rpl_reference_edits_check_aliases(edits, 2, alias_field, &memory));
    assert(hpclang_rpl_reference_edits_check_aliases(edits, 2, NULL, NULL) ==
           HPCLANG_RPL_SPAN_UNAVAILABLE);
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

static int distinct_ram_field(void *context, uint32_t mapping, uint32_t field, uint32_t width,
                              uint32_t *physical)
{
    (void)context;
    assert(mapping < 2 && field + width <= 32);
    *physical = mapping * 32 + field;
    return 0;
}

static int mapped_batch_replay(const struct hpclang_rpl_reference_writer *writer,
                               const struct hpclang_rpl_reference_edit *edits, uint32_t count)
{
    return hpclang_rpl_reference_edits_replay_mapped(writer, edits, count, distinct_ram_field,
                                                     NULL);
}

static void batch_replay(unsigned first_width, unsigned second_width,
                         int (*replay)(const struct hpclang_rpl_reference_writer *,
                                       const struct hpclang_rpl_reference_edit *, uint32_t))
{
    struct edit_memory initial = {0}, completed;
    memset(initial.banks, 7, sizeof initial.banks);
    struct hpclang_rpl_reference_edit edits[] = {{0, 5, 0x12345, 0xabcde, 5},
                                                 {1, 5, 0x54321, 0xedcba, 5}};
    edits[0].width = first_width;
    edits[1].width = second_width;
    for (unsigned i = 0; i < 2; ++i) {
        uint32_t mask = (1u << (4 * edits[i].width)) - 1;
        edits[i].expected &= mask;
        edits[i].value &= mask;
    }
    for (unsigned i = 0; i < 2; ++i)
        for (unsigned j = 0; j < edits[i].width; ++j)
            initial.banks[i][5 + j] = (uint8_t)((edits[i].expected >> (4 * j)) & 15);
    completed = initial;
    for (unsigned i = 0; i < 2; ++i)
        for (unsigned j = 0; j < edits[i].width; ++j)
            completed.banks[i][5 + j] = (uint8_t)((edits[i].value >> (4 * j)) & 15);
    struct edit_memory memory = initial;
    struct hpclang_rpl_reference_writer writer = {{edit_read, &memory}, edit_write};
    assert(!hpclang_rpl_reference_edits_validate(&writer.reader, edits, 2));
    memory = initial;
    assert(!replay(NULL, NULL, 0));
    assert(!replay(&writer, edits, 2));
    assert(!memcmp(memory.banks, completed.banks, sizeof memory.banks));
    unsigned reads = memory.reads, writes = memory.writes;
    assert(writes == first_width + second_width);
    assert(!replay(&writer, edits, 2));
    assert(memory.writes == writes);
    for (unsigned cut = 1; cut <= reads; ++cut) {
        memory = initial;
        memory.fail_read = cut;
        assert(replay(&writer, edits, 2) == HPCLANG_RPL_SPAN_UNAVAILABLE);
        if (cut <= first_width + second_width)
            assert(!memory.writes);
        memory.fail_read = 0;
        assert(!replay(&writer, edits, 2));
        assert(!memcmp(memory.banks, completed.banks, sizeof memory.banks));
    }
    for (unsigned cut = 1; cut <= writes; ++cut) {
        for (int committed = 0; committed <= 1; ++committed) {
            memory = initial;
            memory.fail_write = cut;
            memory.acknowledge_failure = committed;
            assert(replay(&writer, edits, 2) == HPCLANG_RPL_SPAN_UNAVAILABLE);
            memory.fail_write = 0;
            assert(!replay(&writer, edits, 2));
            assert(!memcmp(memory.banks, completed.banks, sizeof memory.banks));
        }
    }
    memory = initial;
    memory.banks[1][4 + second_width] = 0; /* Late corruption prevents all writes. */
    assert(replay(&writer, edits, 2) == HPCLANG_RPL_SPAN_MALFORMED);
    assert(!memory.writes);
    memory = initial;
    edits[1].mapping = 0; /* Same-address edits in distinct banks above were valid. */
    assert(replay(&writer, edits, 2) == HPCLANG_RPL_SPAN_MALFORMED);
    assert(!memory.reads && !memory.writes);
    edits[1].mapping = 1;
    memory.silent_write = 1;
    assert(replay(&writer, edits, 2) == HPCLANG_RPL_SPAN_MALFORMED);
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
        struct hpclang_rpl_reference_edit edit = {0, field, identity.previous, identity.replacement,
                                                  5};
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
        edit = (struct hpclang_rpl_reference_edit){0, field, identity.previous,
                                                   identity.replacement, 5};
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
                           struct hpclang_rpl_catalog_mappings *mappings)
{
    struct catalog_resolver *resolver = context;
    if (++resolver->calls == resolver->fail)
        return HPCLANG_RPL_SPAN_UNSUPPORTED;
    if (binding->bank_switch == 0x10)
        mappings->payload = 1;
    else if (binding->bank_switch == 0x20)
        mappings->payload = 2;
    else if (!binding->bank_switch)
        mappings->payload = 0;
    else
        return HPCLANG_RPL_SPAN_UNSUPPORTED;
    mappings->bank_switch = 3;
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

static int catalog_independent_mappings(void *context,
                                        const struct hpclang_rpl_library_binding *binding,
                                        struct hpclang_rpl_catalog_mappings *mappings)
{
    (void)binding;
    *mappings = *(const struct hpclang_rpl_catalog_mappings *)context;
    return 0;
}

static void execution_address_planning(void)
{
    const struct hpclang_rpl_reference_patch unchanged = {123, 456};
    for (unsigned source = 0; source < 2; ++source) {
        for (unsigned target = 0; target < 2; ++target) {
            struct hpclang_rpl_reference edge = {HPCLANG_RPL_REFERENCE_EXECUTION, 0x100, 0x160, 0,
                                                 0};
            const struct hpclang_rpl_reference_mappings maps = {source, target, 1};
            for (int delta = -16; delta <= 16; delta += 32) {
                struct hpclang_rpl_reference_patch patch = unchanged;
                assert(hpclang_rpl_reference_plan_qualified_shift(&edge, &maps, 0x100, 0x200, delta,
                                                                  &patch) ==
                       HPCLANG_RPL_SPAN_UNSUPPORTED);
                assert(!memcmp(&patch, &unchanged, sizeof patch));
                assert(hpclang_rpl_reference_plan_execution_shift(&edge, &maps, 0x100, 0x200, delta,
                                                                  &patch) == (int)target);
                if (target)
                    assert(patch.field == 0x100 && patch.value == (uint32_t)(0x160 + delta));
                else
                    assert(!memcmp(&patch, &unchanged, sizeof patch));
            }
        }
    }
    struct hpclang_rpl_reference edge = {HPCLANG_RPL_REFERENCE_EXECUTION, 0x100, 1, 0, 0};
    struct hpclang_rpl_reference_mappings maps = {1, 1, 1};
    struct hpclang_rpl_reference_patch patch = unchanged;
    assert(hpclang_rpl_reference_plan_execution_shift(&edge, &maps, 1, 2, -1, &patch) ==
           HPCLANG_RPL_SPAN_UNSUPPORTED);
    assert(!memcmp(&patch, &unchanged, sizeof patch));
    for (unsigned invalid = 0; invalid < 4; ++invalid) {
        struct hpclang_rpl_reference bad = edge;
        if (invalid == 0)
            bad.kind = HPCLANG_RPL_REFERENCE_OPAQUE;
        if (invalid == 1)
            bad.target = 0;
        if (invalid == 2)
            bad.extent = 1;
        if (invalid == 3)
            bad.auxiliary = 1;
        assert(hpclang_rpl_reference_plan_execution_shift(&bad, &maps, 1, 2, 1, &patch) ==
               HPCLANG_RPL_SPAN_MALFORMED);
        assert(!memcmp(&patch, &unchanged, sizeof patch));
    }
    assert(hpclang_rpl_reference_plan_execution_shift(NULL, &maps, 1, 2, 1, &patch) ==
           HPCLANG_RPL_SPAN_MALFORMED);
    assert(hpclang_rpl_reference_plan_execution_shift(&edge, NULL, 1, 2, 1, &patch) ==
           HPCLANG_RPL_SPAN_MALFORMED);
    assert(hpclang_rpl_reference_plan_execution_shift(&edge, &maps, 1, 2, 1, NULL) ==
           HPCLANG_RPL_SPAN_MALFORMED);
    edge.target = 0x160;
    assert(hpclang_rpl_reference_plan_execution_shift(&edge, &maps, 0x103, 0x200, 16, &patch) ==
           HPCLANG_RPL_SPAN_BOUNDS);
    assert(!memcmp(&patch, &unchanged, sizeof patch));
}

static int catalog_alias_locations(void *context, const struct hpclang_rpl_library_binding *binding,
                                   struct hpclang_rpl_catalog_locations *locations)
{
    (void)binding;
    *locations = *(const struct hpclang_rpl_catalog_locations *)context;
    return 0;
}

static void catalog_offset_plans(void)
{
    struct mapped_images images = {.banks = {{.size = 19, .fail = UINT32_MAX}}};
    struct image *image = &images.banks[0];
    put(image, 0, 1, 3);
    put(image, 3, 275, 3);
    put(image, 6, 0x70020, 5);
    put(image, 11, 0x60030, 5);
    put(image, 16, 7, 3);
    const struct hpclang_rpl_reader reader = {read_nibble, image};
    const struct hpclang_rpl_reference_reader mapped = {mapped_read, &images};
    struct hpclang_rpl_reference_edit edits[2];
    struct hpclang_rpl_reference_guard guards[3];
    struct hpclang_rpl_catalog_reference_plan plan = {edits, 2, 0, guards, 3, 0};
    struct hpclang_rpl_catalog_locations locations = {{1, 1}, -0x30000, -0x20000};
    struct hpclang_rpl_catalog_move move = {0, 1, 0x40000, 0x40100, 16};
    assert(!hpclang_rpl_catalog_plan_relocation_offsets(
        &reader, 0, 19, &move, 0, catalog_alias_locations, &locations, &plan));
    assert(plan.edit_count == 2 && plan.guard_count == 3);
    assert(edits[0].field == 6 && edits[0].expected == 0x70020 && edits[0].value == 0x70030);
    assert(edits[1].field == 11 && edits[1].expected == 0x60030 && edits[1].value == 0x60040);
    assert(!hpclang_rpl_reference_edits_validate(&mapped, edits, 2));
    assert(!hpclang_rpl_reference_guards_validate(&mapped, guards, 3));
    for (unsigned nibble = 0; nibble < 19; ++nibble) {
        image->data[nibble] ^= 1;
        assert(hpclang_rpl_reference_edits_validate(&mapped, edits, 2) ||
               hpclang_rpl_reference_guards_validate(&mapped, guards, 3));
        image->data[nibble] ^= 1;
    }
    /* Partial translated field, missing resolver and changed function sentinel
     * invalidate all output counts, including already prepared payload edits. */
    move.catalog_mapping = 1;
    assert(hpclang_rpl_catalog_plan_relocation_offsets(&reader, 0, 19, &move, 0x3fff8,
                                                       catalog_alias_locations, &locations,
                                                       &plan) == HPCLANG_RPL_SPAN_BOUNDS);
    assert(!plan.edit_count && !plan.guard_count);
    move.catalog_mapping = 0;
    assert(hpclang_rpl_catalog_plan_relocation_offsets(&reader, 0, 19, &move, 0, NULL, &locations,
                                                       &plan) == HPCLANG_RPL_SPAN_MALFORMED);
    assert(!plan.edit_count && !plan.guard_count);
    put(image, 11, 16, 5);
    locations.bank_switch_offset = 0x40000;
    move.delta = -16;
    assert(hpclang_rpl_catalog_plan_relocation_offsets(&reader, 0, 19, &move, 0,
                                                       catalog_alias_locations, &locations,
                                                       &plan) == HPCLANG_RPL_SPAN_UNSUPPORTED);
    assert(!plan.edit_count && !plan.guard_count);
    put(image, 11, 0, 5);
    assert(!hpclang_rpl_catalog_plan_relocation_offsets(
        &reader, 0, 19, &move, 0, catalog_alias_locations, &locations, &plan));
    assert(!edits[1].expected && !edits[1].value);
}

static void catalog_executable_switches(void)
{
    struct image image = {.size = 19, .fail = UINT32_MAX};
    struct hpclang_rpl_reader reader = {read_nibble, &image};
    put(&image, 0, 1, 3);
    put(&image, 3, 275, 3);
    put(&image, 6, 0x60020, 5);
    put(&image, 11, 0x60020, 5);
    put(&image, 16, 7, 3);
    struct image before = image;
    struct hpclang_rpl_reference_edit edits[2];
    struct hpclang_rpl_reference_guard guards[3];
    struct hpclang_rpl_catalog_reference_plan plan = {edits, 2, 0, guards, 3, 0};
    for (unsigned payload = 0; payload < 2; ++payload) {
        for (unsigned function = 0; function < 2; ++function) {
            struct hpclang_rpl_catalog_mappings mappings = {payload, function};
            for (int delta = -16; delta <= 16; delta += 32) {
                const struct hpclang_rpl_catalog_move move = {0, 1, 0x60000, 0x60100, delta};
                assert(!hpclang_rpl_catalog_plan_relocation(
                    &reader, 0, image.size, &move, catalog_independent_mappings, &mappings, &plan));
                assert(plan.edit_count == 2 && plan.guard_count == 3);
                assert(edits[0].mapping == 0 && edits[0].field == 6 && edits[0].width == 5);
                assert(edits[1].mapping == 0 && edits[1].field == 11 && edits[1].width == 5);
                assert(edits[0].expected == 0x60020 && edits[1].expected == 0x60020);
                assert(edits[0].value == (uint32_t)(0x60020 + (payload ? delta : 0)));
                assert(edits[1].value == (uint32_t)(0x60020 + (function ? delta : 0)));
                assert(!memcmp(&image, &before, sizeof image));
            }
        }
    }
    /* A zero switch means no executable target, even when zero is in the move.
     * The real payload pointer still relocates under its independently resolved
     * mapping. Both remain protected by explicit expected-value edits. */
    put(&image, 6, 1, 5);
    put(&image, 11, 0, 5);
    struct hpclang_rpl_catalog_mappings mappings = {1, 1};
    const struct hpclang_rpl_catalog_move move = {0, 1, 0, 2, 16};
    assert(!hpclang_rpl_catalog_plan_relocation(&reader, 0, image.size, &move,
                                                catalog_independent_mappings, &mappings, &plan));
    assert(edits[0].expected == 1 && edits[0].value == 17);
    assert(!edits[1].expected && !edits[1].value);
    put(&image, 11, 16, 5);
    const struct hpclang_rpl_catalog_move to_zero = {0, 1, 16, 32, -16};
    assert(hpclang_rpl_catalog_plan_relocation(&reader, 0, image.size, &to_zero,
                                               catalog_independent_mappings, &mappings,
                                               &plan) == HPCLANG_RPL_SPAN_UNSUPPORTED);
    assert(!plan.edit_count && !plan.guard_count);
}

static void compact_guard_observations(void)
{
    struct hpclang_rpl_reference_guard storage[6];
    struct hpclang_rpl_reference_guard_builder builder;
    /* Every order of five contiguous observations must preserve the complete
     * nibble sequence. Include duplicates while the builder is populated. */
    for (unsigned permutation = 0; permutation < 120; ++permutation) {
        assert(!hpclang_rpl_reference_guards_begin(&builder, storage, 5));
        unsigned remaining[] = {0, 1, 2, 3, 4}, code = permutation;
        uint32_t expected = 0;
        for (unsigned i = 0; i < 5; ++i)
            expected |= ((i * 7 + 3) & 15u) << (4 * i);
        for (unsigned left = 5; left; --left) {
            unsigned chosen = code % left, at = remaining[chosen];
            code /= left;
            uint8_t value = (uint8_t)((at * 7 + 3) & 15);
            assert(!hpclang_rpl_reference_guards_observe(&builder, 9, 100 + at, value));
            unsigned count = builder.count;
            assert(!hpclang_rpl_reference_guards_observe(&builder, 9, 100 + at, value));
            assert(builder.count == count);
            for (unsigned i = chosen; i + 1 < left; ++i)
                remaining[i] = remaining[i + 1];
        }
        assert(builder.count == 1 && !builder.error);
        assert(storage[0].mapping == 9 && storage[0].field == 100 && storage[0].width == 5);
        assert(storage[0].expected == expected);
    }
    assert(!hpclang_rpl_reference_guards_begin(&builder, storage, 2));
    storage[2] = (struct hpclang_rpl_reference_guard){123, 456, 789, 1};
    assert(!hpclang_rpl_reference_guards_observe(&builder, 0, 2, 2));
    assert(!hpclang_rpl_reference_guards_observe(&builder, 0, 0, 0));
    assert(builder.count == 2 && storage[0].width == 1 && storage[1].width == 1);
    assert(!hpclang_rpl_reference_guards_observe(&builder, 0, 1, 1));
    assert(builder.count == 1 && storage[0].expected == 0x210 && storage[0].width == 3);
    assert(storage[2].mapping == 123 && storage[2].field == 456 && storage[2].expected == 789);
    assert(!hpclang_rpl_reference_guards_observe(&builder, UINT32_MAX, 0, 15));
    assert(builder.count == 2 && storage[1].mapping == UINT32_MAX && storage[1].expected == 15);
    assert(hpclang_rpl_reference_guards_observe(&builder, 0, 1, 2) == HPCLANG_RPL_SPAN_MALFORMED);
    assert(!builder.count && builder.error == HPCLANG_RPL_SPAN_MALFORMED);
    assert(hpclang_rpl_reference_guards_observe(&builder, 0, 1, 1) == builder.error);
    assert(!hpclang_rpl_reference_guards_begin(&builder, storage, 1));
    assert(!hpclang_rpl_reference_guards_observe(&builder, 0, 0xfffff, 15));
    assert(hpclang_rpl_reference_guards_observe(&builder, 0, 0, 1) == HPCLANG_RPL_SPAN_BOUNDS);
    assert(!builder.count && builder.error == HPCLANG_RPL_SPAN_BOUNDS);
    assert(!hpclang_rpl_reference_guards_begin(&builder, storage, 1));
    assert(hpclang_rpl_reference_guards_observe(&builder, 0, 0x100000, 0) ==
           HPCLANG_RPL_SPAN_BOUNDS);
    assert(!hpclang_rpl_reference_guards_begin(&builder, storage, 1));
    assert(hpclang_rpl_reference_guards_observe(&builder, 0, 0, 16) == HPCLANG_RPL_SPAN_BOUNDS);
    assert(!hpclang_rpl_reference_guards_begin(&builder, NULL, 0));
    assert(hpclang_rpl_reference_guards_observe(&builder, 0, 0, 0) == HPCLANG_RPL_SPAN_BOUNDS);
    assert(hpclang_rpl_reference_guards_begin(&builder, NULL, 1) == HPCLANG_RPL_SPAN_MALFORMED);
    assert(hpclang_rpl_reference_guards_begin(NULL, storage, 1) == HPCLANG_RPL_SPAN_MALFORMED);
    assert(hpclang_rpl_reference_guards_observe(NULL, 0, 0, 0) == HPCLANG_RPL_SPAN_MALFORMED);
}

static void merge_reference_edits(void)
{
    const struct hpclang_rpl_reference_edit inputs[] = {
        {0, 5, 0x12345, 0x12355, 5}, {0, 10, 0x23456, 0x23456, 5}, {UINT32_MAX, 5, 3, 4, 5}};
    struct hpclang_rpl_reference_edit storage[4];
    struct hpclang_rpl_reference_edit_builder builder;
    for (unsigned a = 0; a < 3; ++a)
        for (unsigned b = 0; b < 3; ++b) {
            if (a == b)
                continue;
            unsigned order[] = {a, b, 3 - a - b};
            storage[3] = (struct hpclang_rpl_reference_edit){123, 456, 789, 1011, 5};
            assert(!hpclang_rpl_reference_edits_begin(&builder, storage, 3));
            for (unsigned i = 0; i < 3; ++i) {
                assert(!hpclang_rpl_reference_edits_add(&builder, &inputs[order[i]]));
                assert(!hpclang_rpl_reference_edits_add(&builder, &inputs[order[i]]));
                assert(builder.count == i + 1 && !builder.error);
            }
            assert(!memcmp(storage, inputs, sizeof inputs));
            assert(storage[3].mapping == 123 && storage[3].field == 456 &&
                   storage[3].expected == 789 && storage[3].value == 1011);
            assert(hpclang_rpl_reference_edits_validate(NULL, storage, builder.count) ==
                   HPCLANG_RPL_SPAN_UNAVAILABLE);
        }
    for (int offset = -4; offset <= 4; ++offset) {
        if (!offset)
            continue;
        struct hpclang_rpl_reference_edit overlap = {0, (uint32_t)(10 + offset), 1, 2, 5};
        assert(!hpclang_rpl_reference_edits_begin(&builder, storage, 3));
        assert(!hpclang_rpl_reference_edits_add(&builder, &inputs[1]));
        assert(hpclang_rpl_reference_edits_add(&builder, &overlap) == HPCLANG_RPL_SPAN_MALFORMED);
        assert(!builder.count && builder.error == HPCLANG_RPL_SPAN_MALFORMED);
        assert(hpclang_rpl_reference_edits_add(&builder, &inputs[2]) == builder.error);
    }
    for (unsigned field = 0; field < 2; ++field) {
        struct hpclang_rpl_reference_edit conflict = inputs[0];
        if (field)
            conflict.value ^= 1;
        else
            conflict.expected ^= 1;
        assert(!hpclang_rpl_reference_edits_begin(&builder, storage, 3));
        assert(!hpclang_rpl_reference_edits_add(&builder, &inputs[0]));
        assert(hpclang_rpl_reference_edits_add(&builder, &conflict) == HPCLANG_RPL_SPAN_MALFORMED);
        assert(!builder.count);
    }
    const struct hpclang_rpl_reference_edit invalid[] = {
        {0, 0xffffc, 0, 0, 5}, {0, 0, 0x100000, 0, 5}, {0, 0, 0, 0x100000, 5}};
    for (unsigned i = 0; i < 3; ++i) {
        assert(!hpclang_rpl_reference_edits_begin(&builder, storage, 3));
        assert(hpclang_rpl_reference_edits_add(&builder, &invalid[i]) == HPCLANG_RPL_SPAN_BOUNDS);
        assert(!builder.count && builder.error == HPCLANG_RPL_SPAN_BOUNDS);
    }
    assert(!hpclang_rpl_reference_edits_begin(&builder, storage, 1));
    struct hpclang_rpl_reference_edit last = {0, 0xffffb, 0xfffff, 0xfffff, 5};
    assert(!hpclang_rpl_reference_edits_add(&builder, &last));
    assert(!hpclang_rpl_reference_edits_add(&builder, &last));
    assert(hpclang_rpl_reference_edits_add(&builder, &inputs[0]) == HPCLANG_RPL_SPAN_BOUNDS);
    assert(!builder.count);
    assert(!hpclang_rpl_reference_edits_begin(&builder, NULL, 0));
    assert(hpclang_rpl_reference_edits_add(&builder, &inputs[0]) == HPCLANG_RPL_SPAN_BOUNDS);
    assert(hpclang_rpl_reference_edits_begin(&builder, NULL, 1) == HPCLANG_RPL_SPAN_MALFORMED);
    assert(hpclang_rpl_reference_edits_begin(NULL, storage, 3) == HPCLANG_RPL_SPAN_MALFORMED);
    assert(!hpclang_rpl_reference_edits_begin(&builder, storage, 3));
    assert(hpclang_rpl_reference_edits_add(&builder, NULL) == HPCLANG_RPL_SPAN_MALFORMED);
    assert(hpclang_rpl_reference_edits_add(NULL, &inputs[0]) == HPCLANG_RPL_SPAN_MALFORMED);
}

static void cached_command_program(void)
{
    struct image cache = {.size = 128, .fail = UINT32_MAX};
    struct image rom = {.size = 128, .fail = UINT32_MAX};
    struct hpclang_rpl_reader reader = {read_nibble, &cache};
    struct hpclang_rpl_command_program source = {{read_nibble, &rom}, 0, 15, 161, 17};
    const struct hpclang_rpl_cached_program sentinel = {1, 2, 3, 4};
    struct hpclang_rpl_cached_program view = sentinel;
    put(&rom, 0, 0x2d9d, 5);
    put(&rom, 5, 0x395ac, 5);
    put(&rom, 10, 0x312b, 5);
    put(&cache, 32, 0x2e92, 5);
    put(&cache, 37, 161, 3);
    put(&cache, 40, 17, 3);
    memcpy(cache.data + 43, rom.data, 15);
    assert(!hpclang_rpl_cached_program_match(&reader, 32, 26, &source, &view));
    assert(view.number == 161 && view.command == 17 && view.address == 43 && view.span == 15);
    view = sentinel;
    for (unsigned i = 32; i < 58; ++i) {
        cache.fail = i;
        assert(hpclang_rpl_cached_program_match(&reader, 32, 26, &source, &view) ==
               HPCLANG_RPL_SPAN_UNAVAILABLE);
        assert(!memcmp(&view, &sentinel, sizeof view));
    }
    cache.fail = UINT32_MAX;
    for (unsigned i = 0; i < 15; ++i) {
        rom.fail = i;
        assert(hpclang_rpl_cached_program_match(&reader, 32, 26, &source, &view) != 0);
        assert(!memcmp(&view, &sentinel, sizeof view));
    }
    rom.fail = UINT32_MAX;
    for (unsigned i = 43; i < 58; ++i) {
        cache.data[i] ^= 1;
        assert(hpclang_rpl_cached_program_match(&reader, 32, 26, &source, &view) ==
               HPCLANG_RPL_SPAN_MALFORMED);
        cache.data[i] ^= 1;
        assert(!memcmp(&view, &sentinel, sizeof view));
    }
    for (unsigned i = 37; i < 43; ++i) {
        cache.data[i] ^= 1;
        assert(hpclang_rpl_cached_program_match(&reader, 32, 26, &source, &view) ==
               HPCLANG_RPL_SPAN_MALFORMED);
        cache.data[i] ^= 1;
    }
    cache.data[43] = 16;
    assert(hpclang_rpl_cached_program_match(&reader, 32, 26, &source, &view) ==
           HPCLANG_RPL_SPAN_MALFORMED);
    cache.data[43] = rom.data[0];
    put(&cache, 32, 0x2d9d, 5);
    assert(hpclang_rpl_cached_program_match(&reader, 32, 26, &source, &view) ==
           HPCLANG_RPL_SPAN_UNSUPPORTED);
    put(&cache, 32, 0x2e92, 5);
    put(&rom, 0, 0x2a74, 5);
    assert(hpclang_rpl_cached_program_match(&reader, 32, 26, &source, &view) ==
           HPCLANG_RPL_SPAN_UNSUPPORTED);
    put(&rom, 0, 0x2d9d, 5);
    assert(hpclang_rpl_cached_program_match(&reader, 32, 25, &source, &view) != 0);
    assert(hpclang_rpl_cached_program_match(&reader, 32, 27, &source, &view) != 0);
    assert(hpclang_rpl_cached_program_match(&reader, 0xffff0, 26, &source, &view) ==
           HPCLANG_RPL_SPAN_BOUNDS);
    source.limit = 14;
    assert(hpclang_rpl_cached_program_match(&reader, 32, 26, &source, &view) != 0);
    source.limit = 15;
    source.command = 0x1000;
    assert(hpclang_rpl_cached_program_match(&reader, 32, 26, &source, &view) ==
           HPCLANG_RPL_SPAN_MALFORMED);
    source.command = 17;
    assert(hpclang_rpl_cached_program_match(NULL, 32, 26, &source, &view) ==
           HPCLANG_RPL_SPAN_UNAVAILABLE);
    assert(hpclang_rpl_cached_program_match(&reader, 32, 26, NULL, &view) ==
           HPCLANG_RPL_SPAN_UNAVAILABLE);
    assert(hpclang_rpl_cached_program_match(&reader, 32, 26, &source, NULL) ==
           HPCLANG_RPL_SPAN_UNAVAILABLE);
    assert(!memcmp(&view, &sentinel, sizeof view));
}

static void code_operand_replay(void)
{
    struct rebind_memory memory = {.image = {.size = 128, .fail = UINT32_MAX}, .field = 6};
    memset(memory.image.data, 7, sizeof memory.image.data);
    put(&memory.image, 5, 0x00b7, 4); /* GOSUB: end 9 + displacement 11 = 20. */
    struct image initial = memory.image;
    struct hpclang_rpl_reader reader = {read_nibble, &memory.image};
    struct hpclang_rpl_reference_mappings maps = {0, 0, 0};
    struct hpclang_rpl_code_patch patch;
    assert(hpclang_rpl_code_branch_plan_shift(&reader, 5, 4, &maps, 20, 21, 16, &patch) == 1);
    struct hpclang_rpl_reference_edit edit = {0, patch.field, patch.expected, patch.value,
                                              patch.width};
    struct hpclang_rpl_reference_writer writer = {{rebind_read, &memory}, rebind_write};
    struct hpclang_rpl_reference_guard guard = {0, 5, 0x00b7, 4};
    assert(!hpclang_rpl_reference_guards_validate(&writer.reader, &guard, 1));
    assert(!hpclang_rpl_reference_edits_validate(&writer.reader, &edit, 1));
    memory.fail_write = 1;
    assert(hpclang_rpl_reference_edits_replay(&writer, &edit, 1) == HPCLANG_RPL_SPAN_UNAVAILABLE);
    memory.fail_write = 0;
    assert(!hpclang_rpl_reference_edits_replay(&writer, &edit, 1));
    uint32_t displacement =
        memory.image.data[6] | (memory.image.data[7] << 4) | (memory.image.data[8] << 8);
    assert(9 + displacement == 36);
    for (unsigned at = 0; at < 128; ++at)
        if (at < 6 || at >= 9)
            assert(memory.image.data[at] == initial.data[at]);
    memory.image = initial;
    memory.image.data[5] = 6; /* Same operand, different base/return semantics. */
    memory.writes = 0;
    assert(hpclang_rpl_reference_guards_validate(&writer.reader, &guard, 1) ==
           HPCLANG_RPL_SPAN_MALFORMED);
    assert(!memory.writes);
}

static void mixed_width_metadata(void)
{
    struct hpclang_rpl_reference_edit storage[2];
    struct hpclang_rpl_reference_edit_builder builder;
    for (unsigned a = 1; a <= 5; ++a)
        for (unsigned b = 1; b <= 5; ++b)
            for (unsigned at = 15; at <= 25; ++at)
                for (unsigned reverse = 0; reverse < 2; ++reverse) {
                    struct hpclang_rpl_reference_edit pair[] = {{0, 20, 1, 2, a}, {0, at, 1, 2, b}};
                    int overlap = at < 20 + a && 20 < at + b;
                    int duplicate = at == 20 && a == b;
                    assert(!hpclang_rpl_reference_edits_begin(&builder, storage, 2));
                    assert(!hpclang_rpl_reference_edits_add(&builder, &pair[reverse]));
                    int result = hpclang_rpl_reference_edits_add(&builder, &pair[!reverse]);
                    assert(result == (overlap && !duplicate ? HPCLANG_RPL_SPAN_MALFORMED : 0));
                    assert(builder.count == (overlap && !duplicate ? 0u : duplicate ? 1u : 2u));
                }
    struct hpclang_rpl_reference_edit edit = {0, 0xfffff, 15, 15, 1};
    assert(!hpclang_rpl_reference_edits_begin(&builder, storage, 2));
    assert(!hpclang_rpl_reference_edits_add(&builder, &edit));
    edit.width = 2;
    assert(hpclang_rpl_reference_edits_add(&builder, &edit) == HPCLANG_RPL_SPAN_BOUNDS);
    for (unsigned width = 0; width <= 6; width += 6) {
        edit.width = width;
        assert(!hpclang_rpl_reference_edits_begin(&builder, storage, 2));
        assert(hpclang_rpl_reference_edits_add(&builder, &edit) == HPCLANG_RPL_SPAN_MALFORMED);
    }
    edit = (struct hpclang_rpl_reference_edit){0, 0, 16, 0, 1};
    assert(!hpclang_rpl_reference_edits_begin(&builder, storage, 2));
    assert(hpclang_rpl_reference_edits_add(&builder, &edit) == HPCLANG_RPL_SPAN_BOUNDS);
}

struct guarded_edges
{
    struct edges edges;
    struct hpclang_rpl_reference_guard_builder builder;
    struct hpclang_rpl_reference_guard guards[32];
    uint8_t seen[128];
    uint32_t observations, stop_at;
};

static int guarded_edge(void *context, const struct hpclang_rpl_reference *edge)
{
    struct guarded_edges *state = context;
    return collect(&state->edges, edge);
}

static int observe_reference(void *context, uint32_t address, uint8_t value)
{
    struct guarded_edges *state = context;
    assert(address < sizeof state->seen && value < 16);
    if (++state->observations == state->stop_at)
        return 73;
    state->seen[address] = 1;
    return hpclang_rpl_reference_guards_observe(&state->builder, 7, address, value);
}

static int guarded_mapped_read(void *context, uint32_t mapping, uint32_t address, uint8_t *value)
{
    return mapping == 7 && read_nibble(context, address, value);
}

static void begin_guarded_edges(struct guarded_edges *state)
{
    memset(state, 0, sizeof *state);
    assert(!hpclang_rpl_reference_guards_begin(&state->builder, state->guards, 32));
}

static void guarded_dispatch_references(void)
{
    for (unsigned schema = 0; schema < 2; ++schema) {
        struct image image = {.size = 20, .fail = UINT32_MAX};
        put(&image, 0, 80, 5);
        put(&image, 5, 0x81234, 5); /* Scalar type bits must not become a pointer. */
        const struct image original = image;
        const struct hpclang_rpl_reader reader = {read_nibble, &image};
        const struct hpclang_rpl_reference_reader mapped = {guarded_mapped_read, &image};
        const struct hpclang_rpl_dispatch_descriptor descriptor = {80, schema};
        const unsigned consumed = schema ? 10 : 6;
        struct guarded_edges state;
        begin_guarded_edges(&state);
        assert(!hpclang_rpl_dispatch_reference_visit_guarded(
            &reader, 0, image.size, &descriptor, guarded_edge, observe_reference, &state));
        assert(state.observations == consumed && state.edges.count == (schema ? 1u : 2u));
        assert(state.edges.items[0].kind == HPCLANG_RPL_REFERENCE_EXECUTION);
        assert(state.edges.items[0].target == 80);
        if (!schema)
            assert(state.edges.items[1].kind == HPCLANG_RPL_REFERENCE_EMBEDDED_ENTRY);
        assert(!memcmp(&image, &original, sizeof image));
        assert(!hpclang_rpl_reference_guards_validate(&mapped, state.guards, state.builder.count));
        for (unsigned i = 0; i < image.size; ++i) {
            assert(state.seen[i] == (i < consumed));
            image.data[i] ^= 1;
            int result =
                hpclang_rpl_reference_guards_validate(&mapped, state.guards, state.builder.count);
            assert(result == (i < consumed ? HPCLANG_RPL_SPAN_MALFORMED : 0));
            image.data[i] ^= 1;
        }
        for (unsigned i = 0; i < consumed; ++i) {
            begin_guarded_edges(&state);
            state.stop_at = i + 1;
            assert(hpclang_rpl_dispatch_reference_visit_guarded(&reader, 0, image.size, &descriptor,
                                                                guarded_edge, observe_reference,
                                                                &state) == 73);
            assert(state.observations == i + 1 && !state.edges.count);
            begin_guarded_edges(&state);
            image.fail = i;
            assert(hpclang_rpl_dispatch_reference_visit_guarded(
                       &reader, 0, image.size, &descriptor, guarded_edge, observe_reference,
                       &state) == HPCLANG_RPL_SPAN_UNAVAILABLE);
            assert(!state.seen[i] && !state.edges.count);
            image.fail = UINT32_MAX;
            begin_guarded_edges(&state);
            image.data[i] = 16;
            assert(hpclang_rpl_dispatch_reference_visit_guarded(
                &reader, 0, image.size, &descriptor, guarded_edge, observe_reference, &state));
            assert(!state.seen[i] && !state.edges.count);
            image = original;
        }
        begin_guarded_edges(&state);
        state.edges.stop = 81;
        assert(hpclang_rpl_dispatch_reference_visit_guarded(&reader, 0, image.size, &descriptor,
                                                            guarded_edge, observe_reference,
                                                            &state) == 81);
        assert(state.edges.count == 1);
        begin_guarded_edges(&state);
        assert(hpclang_rpl_dispatch_reference_visit_guarded(&reader, 0, image.size, &descriptor,
                                                            guarded_edge, NULL, &state) ==
               HPCLANG_RPL_SPAN_UNAVAILABLE);
        assert(!state.observations && !state.edges.count);
    }
}

static void identified_dispatch_references(void)
{
    struct image image = {.size = 128, .fail = UINT32_MAX};
    put(&image, 0, 80, 5);
    put(&image, 5, 0x81234, 5);
    uint8_t expected[] = {1, 3, 2, 1, 6, 4, 0, 3};
    memcpy(image.data + 80, expected, 5);
    memcpy(image.data + 100, expected + 5, 3);
    struct hpclang_rpl_dispatch_identity_range ranges[] = {{80, 5, expected},
                                                           {100, 3, expected + 5}};
    const struct hpclang_rpl_reader reader = {read_nibble, &image};
    const struct hpclang_rpl_reference_reader mapped = {guarded_mapped_read, &image};
    for (unsigned schema = 0; schema < 2; ++schema) {
        const struct hpclang_rpl_dispatch_descriptor descriptor = {80, schema};
        struct guarded_edges state;
        begin_guarded_edges(&state);
        assert(!hpclang_rpl_dispatch_reference_visit_identified(
            &reader, 0, 10, &descriptor, ranges, 2, guarded_edge, observe_reference, &state));
        assert(state.edges.count == (schema ? 1u : 2u));
        assert(state.observations == 8 + (schema ? 10u : 6u));
        assert(!hpclang_rpl_reference_guards_validate(&mapped, state.guards, state.builder.count));
        for (unsigned i = 0; i < 8; ++i) {
            unsigned at = i < 5 ? 80 + i : 100 + i - 5;
            image.data[at] ^= 1;
            assert(
                hpclang_rpl_reference_guards_validate(&mapped, state.guards, state.builder.count));
            image.data[at] ^= 1;
        }
        for (unsigned i = 0; i < 8; ++i) {
            unsigned at = i < 5 ? 80 + i : 100 + i - 5;
            for (unsigned failure = 0; failure < 4; ++failure) {
                begin_guarded_edges(&state);
                if (failure == 0)
                    image.data[at] ^= 1;
                else if (failure == 1)
                    image.fail = at;
                else if (failure == 2)
                    image.data[at] = 16;
                else
                    state.stop_at = i + 1;
                int result = hpclang_rpl_dispatch_reference_visit_identified(
                    &reader, 0, 10, &descriptor, ranges, 2, guarded_edge, observe_reference,
                    &state);
                assert(result == (failure == 0   ? HPCLANG_RPL_SPAN_UNSUPPORTED
                                  : failure == 1 ? HPCLANG_RPL_SPAN_UNAVAILABLE
                                  : failure == 2 ? HPCLANG_RPL_SPAN_MALFORMED
                                                 : 73));
                assert(!state.edges.count);
                image.data[at] = expected[i];
                image.fail = UINT32_MAX;
            }
        }
        for (unsigned invalid = 0; invalid < 5; ++invalid) {
            struct hpclang_rpl_dispatch_identity_range bad = ranges[0];
            if (invalid == 0)
                bad.length = 0;
            if (invalid == 1)
                bad.address = 0xffffe;
            if (invalid == 2)
                bad.nibbles = NULL;
            if (invalid == 3)
                bad.address = 81;
            if (invalid == 4)
                expected[0] = 16;
            begin_guarded_edges(&state);
            assert(hpclang_rpl_dispatch_reference_visit_identified(
                &reader, 0, 10, &descriptor, &bad, 1, guarded_edge, observe_reference, &state));
            assert(!state.edges.count && !state.observations);
            expected[0] = 1;
        }
    }
}

static void guarded_nested_references(void)
{
    struct image image = {.size = 45, .fail = UINT32_MAX};
    put(&image, 0, 0x2a74, 5);
    put(&image, 5, 0x29e8, 5);
    put(&image, 10, 30, 5);
    put(&image, 15, 0x2baa, 5);
    put(&image, 20, 1, 5);
    put(&image, 25, 1, 5);
    put(&image, 30, 0x4001c, 5);
    put(&image, 35, 0x23c21, 5);
    put(&image, 40, 0x312b, 5);
    const struct image original = image;
    const struct hpclang_rpl_reader reader = {read_nibble, &image};
    const struct hpclang_rpl_reference_reader mapped = {guarded_mapped_read, &image};
    const struct hpclang_rpl_reference_node root = {0, 45, 0};
    struct hpclang_rpl_reference_node work[1];
    struct guarded_edges state;
    begin_guarded_edges(&state);
    assert(!hpclang_rpl_reference_visit_guarded_nested(&reader, &root, work, 1, 3, guarded_edge,
                                                       observe_reference, &state));
    assert(state.edges.count == 4 && state.seen[0] && state.seen[10] && state.seen[20] &&
           state.seen[35] && state.seen[40]);
    assert(!memcmp(&image, &original, sizeof image));
    assert(!hpclang_rpl_reference_guards_validate(&mapped, state.guards, state.builder.count));
    for (unsigned i = 0; i < image.size; ++i) {
        if (!state.seen[i])
            continue;
        image.data[i] ^= 1;
        assert(hpclang_rpl_reference_guards_validate(&mapped, state.guards, state.builder.count) ==
               HPCLANG_RPL_SPAN_MALFORMED);
        image.data[i] ^= 1;
    }
    /* Every callback failure must survive the parser's read-error conversion. */
    uint32_t observations = state.observations;
    for (uint32_t stop = 1; stop <= observations; ++stop) {
        begin_guarded_edges(&state);
        state.stop_at = stop;
        assert(hpclang_rpl_reference_visit_guarded_nested(&reader, &root, work, 1, 3, guarded_edge,
                                                          observe_reference, &state) == 73);
        assert(state.observations == stop);
    }
    for (uint32_t at = 30; at < 40; ++at) {
        image.fail = at;
        begin_guarded_edges(&state);
        assert(hpclang_rpl_reference_visit_guarded_nested(&reader, &root, work, 1, 3, guarded_edge,
                                                          observe_reference,
                                                          &state) == HPCLANG_RPL_SPAN_UNAVAILABLE);
        assert(!state.seen[at]);
    }
    image.fail = UINT32_MAX;
    const struct hpclang_rpl_reference_node compact = {30, 10, 0x2baa};
    begin_guarded_edges(&state);
    assert(!hpclang_rpl_reference_visit_guarded_nested(&reader, &compact, work, 1, 1, guarded_edge,
                                                       observe_reference, &state));
    for (unsigned i = 0; i < sizeof state.seen; ++i)
        assert(state.seen[i] == (i >= 30 && i < 40));
    begin_guarded_edges(&state);
    state.edges.stop = 81;
    assert(hpclang_rpl_reference_visit_guarded_nested(&reader, &root, work, 1, 3, guarded_edge,
                                                      observe_reference, &state) == 81);
    assert(hpclang_rpl_reference_visit_guarded_nested(&reader, &root, work, 1, 3, guarded_edge,
                                                      NULL,
                                                      &state) == HPCLANG_RPL_SPAN_UNAVAILABLE);
    begin_guarded_edges(&state);
    assert(!hpclang_rpl_reference_guards_begin(&state.builder, state.guards, 1));
    assert(hpclang_rpl_reference_visit_guarded_nested(&reader, &root, work, 1, 3, guarded_edge,
                                                      observe_reference,
                                                      &state) == HPCLANG_RPL_SPAN_BOUNDS);
    assert(!state.builder.count);
    /* Opaque payload bytes are not fabricated into dependencies or silently
     * declared reference-free; the unresolved edge must still reach the caller. */
    image = (struct image){.size = 15, .fail = UINT32_MAX};
    put(&image, 0, 0x2dcc, 5);
    put(&image, 5, 10, 5);
    put(&image, 10, 0x81234, 5);
    const struct hpclang_rpl_reference_node opaque = {0, 15, 0};
    begin_guarded_edges(&state);
    assert(!hpclang_rpl_reference_visit_guarded_nested(&reader, &opaque, work, 1, 1, guarded_edge,
                                                       observe_reference, &state));
    assert(state.edges.count == 1 && state.edges.items[0].kind == HPCLANG_RPL_REFERENCE_OPAQUE);
    for (unsigned i = 10; i < 15; ++i)
        assert(!state.seen[i]);
    image.data[0] = 16;
    begin_guarded_edges(&state);
    assert(hpclang_rpl_reference_visit_guarded_nested(&reader, &opaque, work, 1, 1, guarded_edge,
                                                      observe_reference, &state));
    assert(!state.seen[0]);
}

int main(void)
{
    identified_dispatch_references();
    guarded_dispatch_references();
    guarded_nested_references();
    mixed_width_metadata();
    code_operand_replay();
    cached_command_program();
    merge_reference_edits();
    nested_unresolved();
    compact_guard_observations();
    catalog_relocation_plan();
    catalog_executable_switches();
    catalog_offset_plans();
    physical_alias_replay();
    execution_address_planning();
    rebuilt_catalog_replay();
    for (unsigned first = 1; first <= 5; ++first)
        for (unsigned second = 1; second <= 5; ++second) {
            batch_replay(first, second, hpclang_rpl_reference_edits_replay);
            batch_replay(first, second, mapped_batch_replay);
        }
    metadata_guards();
    batch_preflight();
    qualified_relocation();
    offset_relocation();
    dispatch_schemas();
    execution_entries();
    relocation_patches();
    nested_array();
    acptr_switch_execution();
    indices_and_opaque();
    linked_aliases();
    native_envelopes();
    directories();
    return 0;
}
