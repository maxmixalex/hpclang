#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <hpclang/object.h>

#include "hp39-catalog.h"
#include "hp39-native-directory.h"
#include "object-snapshot.h"

#define FIXTURE_NIBBLES 0x500u
#define RPL_DORRP UINT32_C(0x00002a96)
#define RPL_DOCSTR UINT32_C(0x00002a2c)

struct fixture {
    uint8_t nibbles[FIXTURE_NIBBLES];
};

static int fixture_read(
    void *context,
    uint32_t address,
    uint8_t *value)
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

static void put_byte(
    struct fixture *fixture,
    uint32_t address,
    uint8_t value)
{
    put_word(fixture, address, value, 2u);
}

static uint32_t put_item(
    struct fixture *fixture,
    uint32_t item,
    const char *raw_name,
    const uint8_t *body,
    size_t body_length)
{
    size_t raw_length = strlen(raw_name);
    put_word(fixture, item, (uint32_t)raw_length, 2u);
    for (size_t i = 0u; i < raw_length; ++i)
        put_byte(
            fixture,
            item + 2u + (uint32_t)i * 2u,
            (uint8_t)raw_name[i]);
    put_byte(
        fixture,
        item + 2u + (uint32_t)raw_length * 2u,
        0u);

    uint32_t object =
        item + 2u + 2u * ((uint32_t)raw_length + 1u);
    if (body != NULL) {
        put_word(fixture, object, RPL_DOCSTR, 5u);
        put_word(
            fixture,
            object + 5u,
            5u + (uint32_t)body_length * 2u,
            5u);
        for (size_t i = 0u; i < body_length; ++i)
            put_byte(
                fixture,
                object + 10u + (uint32_t)i * 2u,
                body[i]);
    }
    return object;
}

static int expect(int condition, const char *message)
{
    if (condition)
        return 1;
    fprintf(stderr, "FAIL: %s\n", message);
    return 0;
}

int main(void)
{
    struct fixture fixture = { { 0u } };
    struct hpclang_rpl_reader reader = {
        .read_nibble = fixture_read,
        .context = &fixture,
    };

    const uint32_t context_pointer = 0x20u;
    const uint32_t root = 0x100u;
    const uint32_t directory = root + 8u;
    const uint32_t item_b = 0x380u;
    const uint32_t item_internal = 0x300u;
    const uint32_t item_a = 0x280u;

    put_word(&fixture, context_pointer, root, 5u);
    put_word(&fixture, root, RPL_DORRP, 5u);
    put_word(&fixture, directory, item_b - directory, 5u);

    static const uint8_t body_b[] = { 'B', 'O', 'D', 'Y' };
    put_item(&fixture, item_b, "'B", body_b, sizeof(body_b));
    put_item(&fixture, item_internal, ":B", NULL, 0u);
    put_item(&fixture, item_a, "'A", (const uint8_t *)"", 0u);

    put_word(&fixture, item_b - 5u, item_b - 5u - item_internal, 5u);
    put_word(
        &fixture,
        item_internal - 5u,
        item_internal - 5u - item_a,
        5u);
    put_word(&fixture, item_a - 5u, 0u, 5u);

    struct hpclang_object *catalog = NULL;
    if (!expect(
            hpclang_hp39_native_catalog_snapshot(
                &reader, context_pointer, &catalog) == 0,
            "catalog snapshot succeeds") ||
        !expect(catalog != NULL, "catalog is returned") ||
        !expect(
            hpclang_object_kind(catalog) == HPCLANG_OBJECT_LIST,
            "catalog is a list") ||
        !expect(catalog->nibble_count == 28u, "catalog has exact B,A span"))
        return 1;

    uint32_t count = UINT32_MAX;
    int found = 0;
    if (!expect(
            hpclang_hp39_catalog_name_count(catalog, &count) == 0 &&
                count == 2u,
            "catalog contains exactly two projected names") ||
        !expect(
            hpclang_hp39_catalog_contains_name(
                catalog, (const uint8_t *)"B", 1u, &found) == 0 && found,
            "catalog contains B") ||
        !expect(
            hpclang_hp39_catalog_contains_name(
                catalog, (const uint8_t *)"A", 1u, &found) == 0 && found,
            "catalog contains A") ||
        !expect(
            hpclang_hp39_catalog_contains_name(
                catalog, (const uint8_t *)":B", 2u, &found) == 0 && !found,
            "catalog hides internal colon record"))
        return 1;
    hpclang_object_release(catalog);

    struct hpclang_object *body = NULL;
    size_t body_size = 0u;
    uint8_t copied[sizeof(body_b)] = { 0u };
    if (!expect(
            hpclang_hp39_native_body_snapshot(
                &reader,
                context_pointer,
                (const uint8_t *)"B",
                1u,
                &body) == 0,
            "B body snapshot succeeds") ||
        !expect(
            hpclang_object_string_size(body, &body_size) == 0 &&
                body_size == sizeof(body_b),
            "B body has exact size") ||
        !expect(
            hpclang_object_string_copy(body, copied, sizeof(copied)) == 0 &&
                memcmp(copied, body_b, sizeof(body_b)) == 0,
            "B body bytes are exact"))
        return 1;
    hpclang_object_release(body);

    body = NULL;
    body_size = 99u;
    if (!expect(
            hpclang_hp39_native_body_snapshot(
                &reader,
                context_pointer,
                (const uint8_t *)"A",
                1u,
                &body) == 0,
            "empty A body is distinct from missing") ||
        !expect(
            hpclang_object_string_size(body, &body_size) == 0 &&
                body_size == 0u,
            "empty A body remains an empty string"))
        return 1;
    hpclang_object_release(body);

    body = NULL;
    errno = 0;
    if (!expect(
            hpclang_hp39_native_body_snapshot(
                &reader,
                context_pointer,
                (const uint8_t *)"MISSING",
                7u,
                &body) == -1 &&
                body == NULL && errno == ENOENT,
            "missing body reports ENOENT"))
        return 1;

    const uint32_t empty_context = 0x30u;
    const uint32_t empty_root = 0x180u;
    put_word(&fixture, empty_context, empty_root, 5u);
    put_word(&fixture, empty_root, RPL_DORRP, 5u);
    put_word(&fixture, empty_root + 8u, 0u, 5u);

    catalog = NULL;
    count = UINT32_MAX;
    if (!expect(
            hpclang_hp39_native_catalog_snapshot(
                &reader, empty_context, &catalog) == 0,
            "empty directory catalog snapshot succeeds") ||
        !expect(
            hpclang_hp39_catalog_name_count(catalog, &count) == 0 &&
                count == 0u,
            "empty directory becomes empty DOLIST"))
        return 1;
    hpclang_object_release(catalog);

    const uint32_t bad_context = 0x40u;
    put_word(&fixture, bad_context, 0x1c0u, 5u);
    put_word(&fixture, 0x1c0u, 0u, 5u);
    catalog = NULL;
    errno = 0;
    if (!expect(
            hpclang_hp39_native_catalog_snapshot(
                &reader, bad_context, &catalog) == -1 &&
                catalog == NULL && errno == EILSEQ,
            "non-directory context is rejected"))
        return 1;

    puts("PASS: HP39 native directory catalog/body reader");
    return 0;
}
