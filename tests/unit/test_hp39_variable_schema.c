#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <hpclang/object.h>

#include "hp39-variable-schema.h"
#include "object-snapshot.h"

enum {
    RPL_DOLIST = 0x02a74u,
    RPL_DOCSTR = 0x02a2cu,
    RPL_DOIDNT = 0x02e48u,
    RPL_SEMI = 0x0312bu
};

static void put_nibble(
    struct hpclang_object *object,
    uint32_t offset,
    uint8_t value)
{
    assert(object != NULL);
    assert(offset < object->nibble_count);
    assert(value <= 0x0fu);
    uint8_t *byte = &object->data[offset >> 1];
    if ((offset & 1u) != 0u)
        *byte = (uint8_t)((*byte & 0x0fu) | (value << 4));
    else
        *byte = (uint8_t)((*byte & 0xf0u) | value);
}

static void put_word(
    struct hpclang_object *object,
    uint32_t offset,
    uint32_t nibbles,
    uint32_t value)
{
    for (uint32_t i = 0u; i < nibbles; ++i)
        put_nibble(
            object,
            offset + i,
            (uint8_t)((value >> (4u * i)) & 0x0fu));
}

static void put_byte(
    struct hpclang_object *object,
    uint32_t offset,
    uint8_t value)
{
    put_word(object, offset, 2u, value);
}

static uint32_t put_cstr(
    struct hpclang_object *object,
    uint32_t offset,
    const char *text)
{
    size_t bytes = strlen(text);
    put_word(object, offset, 5u, RPL_DOCSTR);
    put_word(object, offset + 5u, 5u, 5u + (uint32_t)bytes * 2u);
    for (size_t i = 0u; i < bytes; ++i)
        put_byte(
            object,
            offset + 10u + (uint32_t)i * 2u,
            (uint8_t)text[i]);
    return offset + 10u + (uint32_t)bytes * 2u;
}

static uint32_t put_extended_id(
    struct hpclang_object *object,
    uint32_t offset,
    const char *name,
    uint32_t descriptor)
{
    size_t visible = strlen(name);
    assert(visible > 0u && visible <= 252u);
    assert(descriptor <= 0x000fffffu);

    put_word(object, offset, 5u, RPL_DOIDNT);
    put_word(
        object,
        offset + 5u,
        2u,
        (uint32_t)visible + 3u);
    for (size_t i = 0u; i < visible; ++i)
        put_byte(
            object,
            offset + 7u + (uint32_t)i * 2u,
            (uint8_t)name[i]);

    uint32_t tail = offset + 7u + (uint32_t)visible * 2u;
    put_byte(object, tail, (uint8_t)(descriptor & 0xffu));
    put_byte(object, tail + 2u, (uint8_t)((descriptor >> 8u) & 0xffu));
    put_byte(object, tail + 4u, (uint8_t)((descriptor >> 16u) & 0xffu));
    return offset + 7u + ((uint32_t)visible + 3u) * 2u;
}

static struct hpclang_object *make_schema(void)
{
    const uint32_t capacity = 320u;
    struct hpclang_object *schema =
        calloc(1u, sizeof(*schema) + (capacity + 1u) / 2u);
    assert(schema != NULL);
    schema->kind = HPCLANG_OBJECT_LIST;
    schema->native_prologue = RPL_DOLIST;
    schema->nibble_count = capacity;

    uint32_t p = 0u;
    put_word(schema, p, 5u, RPL_DOLIST);
    p += 5u;

    p = put_cstr(schema, p, "Plot");
    put_word(schema, p, 5u, RPL_DOLIST);
    p += 5u;
    p = put_extended_id(schema, p, "Xmin", 0x298e5u);
    p = put_extended_id(schema, p, "Xmax", 0x298f4u);
    put_word(schema, p, 5u, RPL_SEMI);
    p += 5u;

    p = put_cstr(schema, p, "Note");
    put_word(schema, p, 5u, RPL_DOLIST);
    p += 5u;
    p = put_extended_id(schema, p, "NoteText", 0x29a20u);
    put_word(schema, p, 5u, RPL_SEMI);
    p += 5u;

    put_word(schema, p, 5u, RPL_SEMI);
    p += 5u;
    schema->nibble_count = p;
    return schema;
}

struct descriptor_fixture {
    uint32_t address;
    uint8_t nibbles[15];
};

static int descriptor_read_nibble(
    void *context,
    uint32_t address,
    uint8_t *value)
{
    struct descriptor_fixture *fixture =
        (struct descriptor_fixture *)context;
    if (fixture == NULL || value == NULL ||
        address < fixture->address ||
        address >= fixture->address + 15u)
        return 0;
    *value = fixture->nibbles[address - fixture->address];
    return 1;
}

static void fixture_put_word(
    struct descriptor_fixture *fixture,
    uint32_t offset,
    uint32_t value)
{
    assert(offset + 5u <= 15u);
    for (uint32_t i = 0u; i < 5u; ++i)
        fixture->nibbles[offset + i] =
            (uint8_t)((value >> (i * 4u)) & 0x0fu);
}

static void test_schema_find(void)
{
    struct hpclang_object *schema = make_schema();
    struct hpclang_hp39_variable_schema_match match;

    assert(hpclang_hp39_variable_schema_find(
               schema,
               (const uint8_t *)"Plot", 4u,
               (const uint8_t *)"Xmin", 4u,
               &match) == 0);
    assert(match.visible_name_length == 4u);
    assert(match.descriptor_address == 0x298e5u);
    assert(match.id_span == 7u + (4u + 3u) * 2u);

    assert(hpclang_hp39_variable_schema_find(
               schema,
               (const uint8_t *)"Note", 4u,
               (const uint8_t *)"NoteText", 8u,
               &match) == 0);
    assert(match.visible_name_length == 8u);
    assert(match.descriptor_address == 0x29a20u);

    errno = 0;
    assert(hpclang_hp39_variable_schema_find(
               schema,
               (const uint8_t *)"Plot", 4u,
               (const uint8_t *)"Ymin", 4u,
               &match) == -1);
    assert(errno == ENOENT);

    errno = 0;
    assert(hpclang_hp39_variable_schema_find(
               schema,
               (const uint8_t *)"Numeric", 7u,
               (const uint8_t *)"Xmin", 4u,
               &match) == -1);
    assert(errno == ENOENT);

    free(schema);
}

static void test_descriptor_triple(void)
{
    struct descriptor_fixture fixture = {
        .address = 0x298e5u,
        .nibbles = { 0u },
    };
    fixture_put_word(&fixture, 0u, 0x3f4efu);
    fixture_put_word(&fixture, 5u, 0x0dc6cu);
    fixture_put_word(&fixture, 10u, 0x3f4f0u);

    struct hpclang_rpl_reader reader = {
        .read_nibble = descriptor_read_nibble,
        .context = &fixture,
    };
    struct hpclang_hp39_variable_descriptor descriptor;
    assert(hpclang_hp39_variable_descriptor_read(
               &reader, 0x298e5u, &descriptor) == 0);
    assert(descriptor.address == 0x298e5u);
    assert(descriptor.recall_word == 0x3f4efu);
    assert(descriptor.type_predicate_word == 0x0dc6cu);
    assert(descriptor.store_word == 0x3f4f0u);

    errno = 0;
    assert(hpclang_hp39_variable_descriptor_read(
               &reader, 0x29a20u, &descriptor) == -1);
    assert(errno == EIO);
}

static void test_malformed_extended_id(void)
{
    struct hpclang_object *schema = make_schema();
    struct hpclang_hp39_variable_schema_match match;

    /* Corrupt Xmin's three-byte descriptor into an address outside 20 bits. */
    const uint32_t xmin_offset = 5u + 18u + 5u;
    const uint32_t descriptor_tail =
        xmin_offset + 7u + 4u * 2u;
    put_byte(schema, descriptor_tail + 4u, 0xf2u);

    errno = 0;
    assert(hpclang_hp39_variable_schema_find(
               schema,
               (const uint8_t *)"Plot", 4u,
               (const uint8_t *)"Xmin", 4u,
               &match) == -1);
    assert(errno == EILSEQ);
    free(schema);
}

int main(void)
{
    test_schema_find();
    test_descriptor_triple();
    test_malformed_extended_id();
    return 0;
}
