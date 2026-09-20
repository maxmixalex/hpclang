#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <hpclang/object.h>

#include "hp39-function-fields.h"
#include "object-snapshot.h"

#define FIXTURE_NIBBLES 0x100000u
#define RPL_DORRP UINT32_C(0x00002a96)
#define RPL_DOLIST UINT32_C(0x00002a74)
#define RPL_DOROMP UINT32_C(0x00002e92)
#define RPL_DOREAL UINT32_C(0x00002933)
#define RPL_DOBINT UINT32_C(0x00002911)
#define RPL_DOARRY UINT32_C(0x000029e8)
#define RPL_SEMI UINT32_C(0x0000312b)
#define RPL_USEROB UINT32_C(0x00080711)

struct fixture {
    uint8_t nibbles[FIXTURE_NIBBLES];
};

static int fixture_read(void *context, uint32_t address, uint8_t *value)
{
    struct fixture *fixture = (struct fixture *)context;
    if (fixture == NULL || value == NULL || address >= FIXTURE_NIBBLES)
        return 0;
    *value = fixture->nibbles[address];
    return 1;
}

static void put_word(
    struct fixture *fixture,
    uint32_t address,
    uint32_t value,
    uint32_t nibbles)
{
    for (uint32_t i = 0u; i < nibbles; ++i)
        fixture->nibbles[address + i] =
            (uint8_t)((value >> (i * 4u)) & 0x0fu);
}

static void put_function_type(struct fixture *fixture, uint32_t address)
{
    static const uint32_t romptrs[] = {
        UINT32_C(0x000f0f1),
        UINT32_C(0x00000f1),
        UINT32_C(0x00080f1),
        UINT32_C(0x002b0f1),
        UINT32_C(0x00320f1),
        UINT32_C(0x00330f1),
    };

    put_word(fixture, address, RPL_DOLIST, 5u);
    address += 5u;
    for (uint32_t i = 0u;
         i < (uint32_t)(sizeof(romptrs) / sizeof(romptrs[0]));
         ++i) {
        put_word(fixture, address, RPL_DOROMP, 5u);
        put_word(fixture, address + 5u, romptrs[i], 6u);
        address += 11u;
    }
    put_word(fixture, address, RPL_SEMI, 5u);
}

static uint32_t put_null_field(
    struct fixture *fixture,
    uint32_t item,
    uint32_t prologue,
    uint32_t value)
{
    put_word(fixture, item, 0u, 2u);
    uint32_t object = item + 2u;

    if (prologue == RPL_DOREAL) {
        put_word(fixture, object, prologue, 5u);
        put_word(fixture, object + 5u, 0u, 3u);
        put_word(fixture, object + 8u, value, 12u);
        put_word(fixture, object + 20u, 0u, 1u);
        return object + 21u;
    }
    if (prologue == RPL_DOBINT) {
        put_word(fixture, object, prologue, 5u);
        put_word(fixture, object + 5u, value, 5u);
        return object + 10u;
    }

    assert(prologue == RPL_DOLIST);
    put_word(fixture, object, RPL_DOLIST, 5u);
    put_word(fixture, object + 5u, RPL_SEMI, 5u);
    return object + 10u;
}

static void put_real_body(
    struct fixture *fixture,
    uint32_t address,
    uint64_t mantissa,
    int negative)
{
    put_word(fixture, address, 0u, 3u);
    for (uint32_t i = 0u; i < 12u; ++i) {
        fixture->nibbles[address + 3u + i] =
            (uint8_t)(mantissa % UINT64_C(10));
        mantissa /= UINT64_C(10);
    }
    fixture->nibbles[address + 15u] = negative ? 9u : 0u;
}

static uint32_t put_real_array_1d(
    struct fixture *fixture,
    uint32_t item)
{
    static const uint64_t mantissas[] = {
        UINT64_C(100000000000),
        UINT64_C(250000000000),
        UINT64_C(300000000000),
    };
    static const int negative[] = { 0, 0, 1 };
    const uint32_t count =
        (uint32_t)(sizeof(mantissas) / sizeof(mantissas[0]));

    put_word(fixture, item, 0u, 2u);
    uint32_t object = item + 2u;
    put_word(fixture, object, RPL_DOARRY, 5u);
    put_word(fixture, object + 5u, 20u + count * 16u, 5u);
    put_word(fixture, object + 10u, RPL_DOREAL, 5u);
    put_word(fixture, object + 15u, 1u, 5u);
    put_word(fixture, object + 20u, count, 5u);
    for (uint32_t i = 0u; i < count; ++i)
        put_real_body(
            fixture,
            object + 25u + i * 16u,
            mantissas[i],
            negative[i]);
    return object + 25u + count * 16u;
}

int main(void)
{
    static struct fixture fixture;
    struct hpclang_rpl_reader reader = {
        .read_nibble = fixture_read,
        .context = &fixture,
    };

    static const uint32_t field_prologues[
        HPCLANG_HP39_FUNCTION_FIXED_FIELD_COUNT] = {
        RPL_DOREAL, RPL_DOREAL, RPL_DOREAL, RPL_DOREAL,
        RPL_DOREAL, RPL_DOREAL, RPL_DOREAL, RPL_DOREAL,
        RPL_DOREAL, RPL_DOREAL, RPL_DOREAL, RPL_DOREAL,
        RPL_DOBINT,
        RPL_DOREAL, RPL_DOREAL, RPL_DOREAL,
        RPL_DOBINT, RPL_DOBINT, RPL_DOBINT,
        RPL_DOREAL, RPL_DOREAL, RPL_DOREAL,
        RPL_DOREAL, RPL_DOREAL, RPL_DOREAL,
        RPL_DOBINT, RPL_DOBINT, RPL_DOBINT, RPL_DOBINT,
        RPL_DOBINT, RPL_DOBINT, RPL_DOBINT, RPL_DOBINT,
        RPL_DOREAL, RPL_DOREAL, RPL_DOREAL, RPL_DOREAL, RPL_DOREAL,
    };

    const uint32_t home = 0x90000u;
    const uint32_t root = 0x100u;
    uint32_t item = root + 18u;
    uint32_t previous_item = item;
    uint32_t field_objects[HPCLANG_HP39_FUNCTION_FIXED_FIELD_COUNT] = { 0u };
    uint32_t tail_objects[5] = { 0u };

    put_word(&fixture, RPL_USEROB, home, 5u);
    put_word(&fixture, home, RPL_DORRP, 5u);
    put_word(&fixture, home + 5u, 0u, 3u);
    put_word(&fixture, home + 8u, 0u, 5u);

    put_word(&fixture, root, RPL_DORRP, 5u);
    put_word(&fixture, root + 5u, 0x7ffu, 3u);
    put_word(&fixture, root + 13u, 0u, 5u);
    put_word(&fixture, item, 0u, 2u);
    uint32_t function_type = item + 2u;
    put_function_type(&fixture, function_type);
    uint32_t end = function_type + 76u;

    for (uint32_t i = 0u;
         i < HPCLANG_HP39_FUNCTION_FIXED_FIELD_COUNT;
         ++i) {
        uint32_t link = end;
        item = link + 5u;
        put_word(&fixture, link, link - previous_item, 5u);
        field_objects[i] = item + 2u;
        end = put_null_field(
            &fixture, item, field_prologues[i], i + 1u);
        previous_item = item;
    }

    /*
     * Native Function carries five larger sections after its fixed prefix.
     * funcCache@ is LastBut3, hence the middle (tail_objects[2]) object.
     */
    for (uint32_t i = 0u; i < 5u; ++i) {
        uint32_t link = end;
        item = link + 5u;
        put_word(&fixture, link, link - previous_item, 5u);
        tail_objects[i] = item + 2u;
        end = i == 2u
            ? put_real_array_1d(&fixture, item)
            : put_null_field(&fixture, item, RPL_DOLIST, 0u);
        previous_item = item;
    }
    put_word(&fixture, root + 8u, previous_item - (root + 8u), 5u);

    /* Source-proven public-variable flag storage. */
    put_word(
        &fixture,
        field_objects[17u] + 5u,
        2u | 4u | 16u | 32u | 64u | 128u | 256u | 512u,
        5u);
    put_word(
        &fixture,
        field_objects[18u] + 5u,
        1024u | 2048u,
        5u);
    put_word(&fixture, field_objects[25u] + 5u, 1u, 5u);
    put_word(&fixture, field_objects[26u] + 5u, 7u, 5u);
    put_word(&fixture, field_objects[27u] + 5u, 3u, 5u);
    put_word(&fixture, field_objects[31u] + 5u, 4u, 5u);
    put_word(&fixture, field_objects[32u] + 5u, 6u, 5u);

    struct hpclang_hp39_function_field_view field;
    assert(hpclang_hp39_function_field_at(
               &reader, root, 0u, &field) == 0);
    assert(strcmp(field.name, "Xmin") == 0);
    assert(field.prologue == RPL_DOREAL);

    assert(hpclang_hp39_function_field_at(
               &reader, root, 12u, &field) == 0);
    assert(strcmp(field.name, "Angle") == 0);
    assert(field.prologue == RPL_DOBINT);

    assert(hpclang_hp39_function_field_at(
               &reader, root, 33u, &field) == 0);
    assert(strcmp(field.name, "Root") == 0);

    assert(hpclang_hp39_function_field_at(
               &reader, root, 37u, &field) == 0);
    assert(strcmp(field.name, "Extremum") == 0);

    assert(hpclang_hp39_function_field_find(
               &reader,
               root,
               (const uint8_t *)"PlotFlag",
               sizeof("PlotFlag") - 1u,
               &field) == 0);
    assert(field.prologue == RPL_DOBINT);

    errno = 0;
    assert(hpclang_hp39_function_field_find(
               &reader,
               root,
               (const uint8_t *)"Nope",
               sizeof("Nope") - 1u,
               &field) == -1);
    assert(errno == ENOENT);

    struct hpclang_object *object = NULL;
    assert(hpclang_hp39_function_field_snapshot(
               &reader, root, 0u, &object) == 0);
    assert(object != NULL);
    assert(hpclang_object_kind(object) == HPCLANG_OBJECT_REAL);
    hpclang_object_release(object);

    struct hpclang_hp39_function_variable_projection projection;
    assert(hpclang_hp39_function_variable_projection_at(
               0u, &projection) == 0);
    assert(strcmp(projection.category, "Sketch") == 0);
    assert(strcmp(projection.name, "PageNum") == 0);
    assert(projection.kind ==
        HPCLANG_HP39_FUNCTION_VARIABLE_UNRESOLVED);
    assert(projection.fixed_field_index ==
        HPCLANG_HP39_FUNCTION_VARIABLE_NO_FIELD);

    assert(hpclang_hp39_function_variable_projection_at(
               HPCLANG_HP39_FUNCTION_VARIABLE_COUNT - 1u,
               &projection) == 0);
    assert(strcmp(projection.category, "Plot") == 0);
    assert(strcmp(projection.name, "Axes") == 0);
    assert(projection.kind ==
        HPCLANG_HP39_FUNCTION_VARIABLE_FLAG_REAL);
    assert(projection.fixed_field_index == 17u);
    assert(projection.mask == 2u);

    assert(hpclang_hp39_function_variable_projection_find(
               (const uint8_t *)"Numeric",
               sizeof("Numeric") - 1u,
               (const uint8_t *)"NumStart",
               sizeof("NumStart") - 1u,
               &projection) == 0);
    assert(projection.kind ==
        HPCLANG_HP39_FUNCTION_VARIABLE_DIRECT_FIELD);
    assert(projection.fixed_field_index == 22u);

    assert(hpclang_hp39_function_variable_projection_find(
               (const uint8_t *)"Numeric",
               sizeof("Numeric") - 1u,
               (const uint8_t *)"NumType",
               sizeof("NumType") - 1u,
               &projection) == 0);
    assert(projection.kind ==
        HPCLANG_HP39_FUNCTION_VARIABLE_FLAG_REAL_INVERTED);
    assert(projection.fixed_field_index == 25u);
    assert(projection.mask == 2u);

    object = NULL;
    assert(hpclang_hp39_function_variable_snapshot(
               &reader,
               root,
               (const uint8_t *)"Numeric",
               sizeof("Numeric") - 1u,
               (const uint8_t *)"NumStart",
               sizeof("NumStart") - 1u,
               &object) == 0);
    assert(object != NULL);
    assert(hpclang_object_kind(object) == HPCLANG_OBJECT_REAL);
    hpclang_object_release(object);

    double real = -1.0;
    object = NULL;
    assert(hpclang_hp39_function_variable_snapshot(
               &reader,
               root,
               (const uint8_t *)"Numeric",
               sizeof("Numeric") - 1u,
               (const uint8_t *)"NumFont",
               sizeof("NumFont") - 1u,
               &object) == 0);
    assert(hpclang_object_kind(object) == HPCLANG_OBJECT_REAL);
    assert(hpclang_object_real_f64(object, &real) == 0);
    assert(real == 1.0);
    hpclang_object_release(object);

    object = NULL;
    real = -1.0;
    assert(hpclang_hp39_function_variable_snapshot(
               &reader,
               root,
               (const uint8_t *)"Numeric",
               sizeof("Numeric") - 1u,
               (const uint8_t *)"NumType",
               sizeof("NumType") - 1u,
               &object) == 0);
    assert(hpclang_object_real_f64(object, &real) == 0);
    assert(real == 1.0);
    hpclang_object_release(object);

    static const struct {
        const char *name;
        double expected;
    } numeric_bints[] = {
        { "NumRow", 7.0 },
        { "NumCol", 3.0 },
        { "Format", 4.0 },
        { "Digits", 6.0 },
        { "Angle", 13.0 },
    };
    for (size_t i = 0u;
         i < sizeof(numeric_bints) / sizeof(numeric_bints[0]);
         ++i) {
        const char *category =
            strcmp(numeric_bints[i].name, "Angle") == 0
                ? "Symbolic" : "Numeric";
        object = NULL;
        real = -1.0;
        assert(hpclang_hp39_function_variable_snapshot(
                   &reader,
                   root,
                   (const uint8_t *)category,
                   strlen(category),
                   (const uint8_t *)numeric_bints[i].name,
                   strlen(numeric_bints[i].name),
                   &object) == 0);
        assert(hpclang_object_kind(object) == HPCLANG_OBJECT_REAL);
        assert(hpclang_object_real_f64(object, &real) == 0);
        assert(real == numeric_bints[i].expected);
        hpclang_object_release(object);
    }

    put_word(&fixture, field_objects[25u] + 5u, 2u, 5u);
    object = NULL;
    real = -1.0;
    assert(hpclang_hp39_function_variable_snapshot(
               &reader,
               root,
               (const uint8_t *)"Numeric",
               sizeof("Numeric") - 1u,
               (const uint8_t *)"NumType",
               sizeof("NumType") - 1u,
               &object) == 0);
    assert(hpclang_object_real_f64(object, &real) == 0);
    assert(real == 0.0);
    hpclang_object_release(object);

    static const char *const plot_true_flags[] = {
        "Axes", "Connect", "Grid", "Labels",
        "Recenter", "Simult", "Tracing", "Coord"
    };
    for (size_t i = 0u;
         i < sizeof(plot_true_flags) / sizeof(plot_true_flags[0]);
         ++i) {
        object = NULL;
        real = -1.0;
        const char *flag_name = plot_true_flags[i];
        assert(hpclang_hp39_function_variable_snapshot(
                   &reader,
                   root,
                   (const uint8_t *)"Plot",
                   sizeof("Plot") - 1u,
                   (const uint8_t *)flag_name,
                   strlen(flag_name),
                   &object) == 0);
        assert(hpclang_object_real_f64(object, &real) == 0);
        assert(real == 1.0);
        hpclang_object_release(object);
    }

    static const char *const plot_inverted_flags[] = {
        "FastRes", "InvCross"
    };
    for (size_t i = 0u;
         i < sizeof(plot_inverted_flags) /
             sizeof(plot_inverted_flags[0]);
         ++i) {
        object = NULL;
        real = -1.0;
        assert(hpclang_hp39_function_variable_snapshot(
                   &reader,
                   root,
                   (const uint8_t *)"Plot",
                   sizeof("Plot") - 1u,
                   (const uint8_t *)plot_inverted_flags[i],
                   strlen(plot_inverted_flags[i]),
                   &object) == 0);
        assert(hpclang_object_real_f64(object, &real) == 0);
        assert(real == 0.0);
        hpclang_object_release(object);
    }

    assert(hpclang_hp39_function_variable_projection_find(
               (const uint8_t *)"Plot",
               sizeof("Plot") - 1u,
               (const uint8_t *)"Xzoom",
               sizeof("Xzoom") - 1u,
               &projection) == 0);
    assert(projection.kind ==
        HPCLANG_HP39_FUNCTION_VARIABLE_DIRECT_FIELD);
    assert(projection.fixed_field_index == 10u);

    assert(hpclang_hp39_function_variable_projection_find(
               (const uint8_t *)"Plot",
               sizeof("Plot") - 1u,
               (const uint8_t *)"FastRes",
               sizeof("FastRes") - 1u,
               &projection) == 0);
    assert(projection.kind ==
        HPCLANG_HP39_FUNCTION_VARIABLE_FLAG_REAL_INVERTED);
    assert(projection.fixed_field_index == 17u);
    assert(projection.mask == 32u);

    errno = 0;
    assert(hpclang_hp39_function_variable_projection_find(
               (const uint8_t *)"Plot",
               sizeof("Plot") - 1u,
               (const uint8_t *)"HighRes",
               sizeof("HighRes") - 1u,
               &projection) == -1);
    assert(errno == ENOENT);

    assert(hpclang_hp39_function_variable_projection_find(
               (const uint8_t *)"Numeric",
               sizeof("Numeric") - 1u,
               (const uint8_t *)"NumIndep",
               sizeof("NumIndep") - 1u,
               &projection) == 0);
    assert(projection.kind ==
        HPCLANG_HP39_FUNCTION_VARIABLE_REAL_ARRAY_LIST);
    assert(projection.fixed_field_index ==
        HPCLANG_HP39_FUNCTION_VARIABLE_NO_FIELD);

    object = NULL;
    assert(hpclang_hp39_function_variable_snapshot(
               &reader,
               root,
               (const uint8_t *)"Numeric",
               sizeof("Numeric") - 1u,
               (const uint8_t *)"NumIndep",
               sizeof("NumIndep") - 1u,
               &object) == 0);
    assert(object != NULL);
    assert(hpclang_object_kind(object) == HPCLANG_OBJECT_LIST);
    assert(object->nibble_count == 10u + 3u * 21u);

    for (uint32_t i = 0u; i < 3u; ++i) {
        uint32_t out = 5u + i * 21u;
        for (uint32_t j = 0u; j < 5u; ++j) {
            uint8_t nibble = 0xffu;
            assert(hpclang_object_snapshot_read_nibble(
                object, out + j, &nibble));
            assert(nibble ==
                (uint8_t)((RPL_DOREAL >> (j * 4u)) & 0x0fu));
        }
        for (uint32_t j = 0u; j < 16u; ++j) {
            uint8_t nibble = 0xffu;
            assert(hpclang_object_snapshot_read_nibble(
                object, out + 5u + j, &nibble));
            assert(nibble ==
                fixture.nibbles[tail_objects[2] + 25u + i * 16u + j]);
        }
    }
    for (uint32_t j = 0u; j < 5u; ++j) {
        uint8_t nibble = 0xffu;
        assert(hpclang_object_snapshot_read_nibble(
            object, 5u + 3u * 21u + j, &nibble));
        assert(nibble ==
            (uint8_t)((RPL_SEMI >> (j * 4u)) & 0x0fu));
    }
    hpclang_object_release(object);

    put_word(&fixture, tail_objects[2] + 10u, RPL_DOBINT, 5u);
    object = NULL;
    errno = 0;
    assert(hpclang_hp39_function_variable_snapshot(
               &reader, root,
               (const uint8_t *)"Numeric", sizeof("Numeric") - 1u,
               (const uint8_t *)"NumIndep", sizeof("NumIndep") - 1u,
               &object) == -1);
    assert(object == NULL);
    assert(errno == EILSEQ);
    put_word(&fixture, tail_objects[2] + 10u, RPL_DOREAL, 5u);

    put_word(&fixture, tail_objects[2] + 15u, 2u, 5u);
    object = NULL;
    errno = 0;
    assert(hpclang_hp39_function_variable_snapshot(
               &reader, root,
               (const uint8_t *)"Numeric", sizeof("Numeric") - 1u,
               (const uint8_t *)"NumIndep", sizeof("NumIndep") - 1u,
               &object) == -1);
    assert(object == NULL);
    assert(errno == EILSEQ);
    put_word(&fixture, tail_objects[2] + 15u, 1u, 5u);

    put_word(&fixture, tail_objects[2] + 5u, 21u + 3u * 16u, 5u);
    object = NULL;
    errno = 0;
    assert(hpclang_hp39_function_variable_snapshot(
               &reader, root,
               (const uint8_t *)"Numeric", sizeof("Numeric") - 1u,
               (const uint8_t *)"NumIndep", sizeof("NumIndep") - 1u,
               &object) == -1);
    assert(object == NULL);
    assert(errno == EILSEQ);
    put_word(&fixture, tail_objects[2] + 5u, 20u + 3u * 16u, 5u);

    errno = 0;
    assert(hpclang_hp39_function_variable_projection_find(
               (const uint8_t *)"Numeric",
               sizeof("Numeric") - 1u,
               (const uint8_t *)"NotAVariable",
               sizeof("NotAVariable") - 1u,
               &projection) == -1);
    assert(errno == ENOENT);

    /* A compatible-looking directory without the Function tTYPE is rejected. */
    put_word(&fixture, function_type + 10u, UINT32_C(0x123456), 6u);
    errno = 0;
    assert(hpclang_hp39_function_field_at(
               &reader, root, 0u, &field) == -1);
    assert(errno == ENOTSUP);

    return 0;
}
