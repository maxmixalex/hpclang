#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <hpclang/object.h>

#include "hp39-catalog.h"
#include "hp39-native-directory.h"
#include "object-snapshot.h"

#define FIXTURE_NIBBLES 0x100000u
#define RPL_DORRP UINT32_C(0x00002a96)
#define RPL_DOCSTR UINT32_C(0x00002a2c)
#define RPL_DOLIST UINT32_C(0x00002a74)
#define RPL_SEMI UINT32_C(0x0000312b)
#define RPL_USEROB UINT32_C(0x00080711)

struct fixture
{
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

static void put_word(struct fixture *fixture, uint32_t address, uint32_t value, uint32_t nibbles)
{
    for (uint32_t i = 0u; i < nibbles; ++i)
        fixture->nibbles[address + i] = (uint8_t)((value >> (i * 4u)) & 0x0fu);
}

static void put_byte(struct fixture *fixture, uint32_t address, uint8_t value)
{
    put_word(fixture, address, value, 2u);
}

static uint32_t put_item(struct fixture *fixture, uint32_t item, const char *raw_name,
                         const uint8_t *body, size_t body_length)
{
    size_t raw_length = strlen(raw_name);
    put_word(fixture, item, (uint32_t)raw_length, 2u);
    for (size_t i = 0u; i < raw_length; ++i)
        put_byte(fixture, item + 2u + (uint32_t)i * 2u, (uint8_t)raw_name[i]);
    put_byte(fixture, item + 2u + (uint32_t)raw_length * 2u, (uint8_t)raw_length);

    uint32_t object = item + 4u + 2u * (uint32_t)raw_length;
    if (body != NULL) {
        put_word(fixture, object, RPL_DOCSTR, 5u);
        put_word(fixture, object + 5u, 5u + (uint32_t)body_length * 2u, 5u);
        for (size_t i = 0u; i < body_length; ++i)
            put_byte(fixture, object + 10u + (uint32_t)i * 2u, body[i]);
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
    static struct fixture fixture;
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

    static const uint8_t body_b[] = {'B', 'O', 'D', 'Y'};
    put_item(&fixture, item_b, "'B", body_b, sizeof(body_b));
    put_item(&fixture, item_internal, ":B", NULL, 0u);
    put_item(&fixture, item_a, "'A", (const uint8_t *)"", 0u);

    put_word(&fixture, item_b - 5u, item_b - 5u - item_internal, 5u);
    put_word(&fixture, item_internal - 5u, item_internal - 5u - item_a, 5u);
    put_word(&fixture, item_a - 5u, 0u, 5u);

    struct hpclang_object *catalog = NULL;
    if (!expect(hpclang_hp39_native_catalog_snapshot(&reader, context_pointer, &catalog) == 0,
                "catalog snapshot succeeds") ||
        !expect(catalog != NULL, "catalog is returned") ||
        !expect(hpclang_object_kind(catalog) == HPCLANG_OBJECT_LIST, "catalog is a list") ||
        !expect(catalog->nibble_count == 28u, "catalog has exact B,A span"))
        return 1;

    uint32_t count = UINT32_MAX;
    int found = 0;
    if (!expect(hpclang_hp39_catalog_name_count(catalog, &count) == 0 && count == 2u,
                "catalog contains exactly two projected names") ||
        !expect(hpclang_hp39_catalog_contains_name(catalog, (const uint8_t *)"B", 1u, &found) ==
                        0 &&
                    found,
                "catalog contains B") ||
        !expect(hpclang_hp39_catalog_contains_name(catalog, (const uint8_t *)"A", 1u, &found) ==
                        0 &&
                    found,
                "catalog contains A") ||
        !expect(hpclang_hp39_catalog_contains_name(catalog, (const uint8_t *)":B", 2u, &found) ==
                        0 &&
                    !found,
                "catalog hides internal colon record"))
        return 1;
    hpclang_object_release(catalog);

    struct hpclang_object *body = NULL;
    size_t body_size = 0u;
    uint8_t copied[sizeof(body_b)] = {0u};
    if (!expect(hpclang_hp39_native_body_snapshot(&reader, context_pointer, (const uint8_t *)"B",
                                                  1u, &body) == 0,
                "B body snapshot succeeds") ||
        !expect(hpclang_object_string_size(body, &body_size) == 0 && body_size == sizeof(body_b),
                "B body has exact size") ||
        !expect(hpclang_object_string_copy(body, copied, sizeof(copied)) == 0 &&
                    memcmp(copied, body_b, sizeof(body_b)) == 0,
                "B body bytes are exact"))
        return 1;
    hpclang_object_release(body);

    body = NULL;
    body_size = 99u;
    if (!expect(hpclang_hp39_native_body_snapshot(&reader, context_pointer, (const uint8_t *)"A",
                                                  1u, &body) == 0,
                "empty A body is distinct from missing") ||
        !expect(hpclang_object_string_size(body, &body_size) == 0 && body_size == 0u,
                "empty A body remains an empty string"))
        return 1;
    hpclang_object_release(body);

    body = NULL;
    errno = 0;
    if (!expect(hpclang_hp39_native_body_snapshot(&reader, context_pointer,
                                                  (const uint8_t *)"MISSING", 7u, &body) == -1 &&
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
    if (!expect(hpclang_hp39_native_catalog_snapshot(&reader, empty_context, &catalog) == 0,
                "empty directory catalog snapshot succeeds") ||
        !expect(hpclang_hp39_catalog_name_count(catalog, &count) == 0 && count == 0u,
                "empty directory becomes empty DOLIST"))
        return 1;
    hpclang_object_release(catalog);

    /*
     * Real HP39 HOME stores a null-named Hidden directory. Its private
     * apostrophe-prefixed ApDirList variable is a DOLIST of display strings.
     */
    const uint32_t home_root = 0x81000u;
    const uint32_t hidden_item = 0x81200u;
    const uint32_t hidden_root = hidden_item + 2u;
    const uint32_t aplet_item = 0x81300u;
    const uint32_t apletdir_item = 0x81400u;
    const uint32_t function_item = 0x81500u;
    put_word(&fixture, RPL_USEROB, home_root, 5u);
    put_word(&fixture, home_root, RPL_DORRP, 5u);
    put_word(&fixture, home_root + 5u, 0u, 3u); /* no attached libraries */
    put_word(&fixture, home_root + 8u, hidden_item - (home_root + 8u), 5u);
    put_word(&fixture, hidden_item - 5u, 0u, 5u);
    put_word(&fixture, hidden_item, 0u, 2u); /* null-name special form */
    put_word(&fixture, hidden_root, RPL_DORRP, 5u);
    put_word(&fixture, hidden_root + 8u, aplet_item - (hidden_root + 8u), 5u);
    put_word(&fixture, aplet_item - 5u, 0u, 5u);

    const char *aplet_raw_name = "'ApDirList";
    size_t aplet_raw_length = strlen(aplet_raw_name);
    put_word(&fixture, aplet_item, (uint32_t)aplet_raw_length, 2u);
    for (size_t i = 0u; i < aplet_raw_length; ++i)
        put_byte(&fixture, aplet_item + 2u + (uint32_t)i * 2u, (uint8_t)aplet_raw_name[i]);
    put_byte(&fixture, aplet_item + 2u + (uint32_t)aplet_raw_length * 2u,
             (uint8_t)aplet_raw_length);
    uint32_t aplet_object = aplet_item + 4u + (uint32_t)aplet_raw_length * 2u;
    put_word(&fixture, aplet_object, RPL_DOLIST, 5u);
    uint32_t aplet_string = aplet_object + 5u;
    static const uint8_t function_name[] = {'F', 'u', 'n', 'c', 't', 'i', 'o', 'n'};
    put_word(&fixture, aplet_string, RPL_DOCSTR, 5u);
    put_word(&fixture, aplet_string + 5u, 5u + (uint32_t)sizeof(function_name) * 2u, 5u);
    for (size_t i = 0u; i < sizeof(function_name); ++i)
        put_byte(&fixture, aplet_string + 10u + (uint32_t)i * 2u, function_name[i]);
    put_word(&fixture, aplet_string + 10u + (uint32_t)sizeof(function_name) * 2u, RPL_SEMI, 5u);

    catalog = NULL;
    count = UINT32_MAX;
    found = 0;
    if (!expect(hpclang_hp39_native_aplet_catalog_snapshot(&reader, &catalog) == 0,
                "Aplet catalog reads HOME Hidden/ApDirList") ||
        !expect(hpclang_hp39_catalog_name_count(catalog, &count) == 0 && count == 1u,
                "Aplet catalog preserves one display string") ||
        !expect(hpclang_hp39_catalog_contains_name(catalog, function_name, sizeof(function_name),
                                                   &found) == 0 &&
                    found,
                "Aplet catalog contains Function"))
        return 1;
    hpclang_object_release(catalog);

    /*
     * Add 'apletdir only after the ApDirList witness above has been read.
     * Linking an all-zero future fixture item into HOME earlier accidentally
     * makes it look like the legitimate null-named Hidden entry.
     */
    put_word(&fixture, home_root + 8u, apletdir_item - (home_root + 8u), 5u);
    put_word(&fixture, apletdir_item - 5u, apletdir_item - 5u - hidden_item, 5u);

    /*
     * Stock RclAplet first enters HOME's 'apletdir and prepends an apostrophe
     * to the display name. Model that exact storage path with one empty
     * Function Aplet directory.
     */
    const char *apletdir_raw_name = "'apletdir";
    size_t apletdir_raw_length = strlen(apletdir_raw_name);
    put_word(&fixture, apletdir_item, (uint32_t)apletdir_raw_length, 2u);
    for (size_t i = 0u; i < apletdir_raw_length; ++i)
        put_byte(&fixture, apletdir_item + 2u + (uint32_t)i * 2u, (uint8_t)apletdir_raw_name[i]);
    put_byte(&fixture, apletdir_item + 2u + (uint32_t)apletdir_raw_length * 2u,
             (uint8_t)apletdir_raw_length);
    uint32_t apletdir_root = apletdir_item + 4u + (uint32_t)apletdir_raw_length * 2u;
    put_word(&fixture, apletdir_root, RPL_DORRP, 5u);
    put_word(&fixture, apletdir_root + 8u, function_item - (apletdir_root + 8u), 5u);
    put_word(&fixture, function_item - 5u, 0u, 5u);

    const char *function_raw_name = "'Function";
    size_t function_raw_length = strlen(function_raw_name);
    put_word(&fixture, function_item, (uint32_t)function_raw_length, 2u);
    for (size_t i = 0u; i < function_raw_length; ++i)
        put_byte(&fixture, function_item + 2u + (uint32_t)i * 2u, (uint8_t)function_raw_name[i]);
    put_byte(&fixture, function_item + 2u + (uint32_t)function_raw_length * 2u,
             (uint8_t)function_raw_length);
    uint32_t function_root = function_item + 4u + (uint32_t)function_raw_length * 2u;
    put_word(&fixture, function_root, RPL_DORRP, 5u);
    put_word(&fixture, function_root + 8u, 0u, 5u);

    uint32_t resolved_aplet = 0u;
    if (!expect(hpclang_hp39_native_aplet_resolve(&reader, function_name, sizeof(function_name),
                                                  &resolved_aplet) == 0 &&
                    resolved_aplet == function_root,
                "Aplet preflight resolves its native directory without copying") ||
        !expect(hpclang_hp39_native_aplet_resolve(&reader, NULL, 0u, &resolved_aplet) == -1 &&
                    errno == EINVAL && resolved_aplet == 0u,
                "invalid preflight clears its borrowed address") ||
        !expect(hpclang_hp39_native_aplet_resolve(&reader, (const uint8_t *)"Missing", 7u,
                                                  &resolved_aplet) == -1 &&
                    errno == ENOENT && resolved_aplet == 0u,
                "missing preflight cannot retain a stale native address"))
        return 1;
    struct hpclang_object *aplet = NULL;
    if (!expect(hpclang_hp39_native_aplet_snapshot(&reader, function_name, sizeof(function_name),
                                                   &aplet) == 0,
                "Function Aplet snapshot follows HOME/'apletdir/'Function") ||
        !expect(aplet != NULL, "Function Aplet snapshot is returned") ||
        !expect(hpclang_object_kind(aplet) == HPCLANG_OBJECT_DIRECTORY,
                "Function Aplet snapshot is a directory") ||
        !expect(aplet->nibble_count == 13u, "empty Function fixture has exact DORRP span"))
        return 1;
    hpclang_object_release(aplet);

    aplet = NULL;
    errno = 0;
    if (!expect(hpclang_hp39_native_aplet_snapshot(&reader, (const uint8_t *)"Missing",
                                                   sizeof("Missing") - 1u, &aplet) == -1 &&
                    aplet == NULL && errno == ENOENT,
                "missing Aplet reports ENOENT"))
        return 1;

    /* GetApDirList semantics: missing private variable becomes NULL{}. */
    put_word(&fixture, hidden_root + 8u, 0u, 5u);
    catalog = NULL;
    count = UINT32_MAX;
    if (!expect(hpclang_hp39_native_aplet_catalog_snapshot(&reader, &catalog) == 0,
                "missing ApDirList becomes an empty catalog") ||
        !expect(hpclang_hp39_catalog_name_count(catalog, &count) == 0 && count == 0u,
                "missing ApDirList becomes empty DOLIST"))
        return 1;
    hpclang_object_release(catalog);

    const uint32_t bad_context = 0x40u;
    put_word(&fixture, bad_context, 0x1c0u, 5u);
    put_word(&fixture, 0x1c0u, 0u, 5u);
    catalog = NULL;
    errno = 0;
    if (!expect(hpclang_hp39_native_catalog_snapshot(&reader, bad_context, &catalog) == -1 &&
                    catalog == NULL && errno == EILSEQ,
                "non-directory context is rejected"))
        return 1;

    puts("PASS: HP39 native directory catalog/body reader");
    return 0;
}
