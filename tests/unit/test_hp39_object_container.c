#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "hp39-object-container.h"

static void assert_bytes(
    const uint8_t *actual, size_t actual_size, const char *expected)
{
    size_t expected_size = strlen(expected);
    assert(actual_size == expected_size);
    assert(memcmp(actual, expected, expected_size) == 0);
}

static void test_hpgcc_aplet_header(void)
{
    static const uint8_t data[] = {
        'H','P','3','9','B','i','n','B',
        17u,
        'H','P','C','L','A','N','G',' ','H','P','C','1',' ','T','E','S','T',
        0x96u, 0x2au, 0x30u, 0x11u, 0x2cu, 0xa7u
    };
    struct hpclang_hp39_container_view view;
    assert(hpclang_hp39_container_parse(data, sizeof(data), &view) ==
           HPCLANG_HP39_CONTAINER_OK);
    assert(view.subtype == 'B');
    assert_bytes(view.name, view.name_length, "HPCLANG HPC1 TEST");
    assert(view.payload_size == 6u);
    assert(view.payload[0] == 0x96u);
    assert(view.payload[5] == 0xa7u);
}

static void test_hpgcc_library_header(void)
{
    static const uint8_t data[] = {
        'H','P','3','9','B','i','n','H',
        12u,
        'A','R','M','T','o','o','l','b','o','x','3','9',
        0x40u, 0x2bu, 0x20u, 0xb7u
    };
    struct hpclang_hp39_container_view view;
    assert(hpclang_hp39_container_parse(data, sizeof(data), &view) ==
           HPCLANG_HP39_CONTAINER_OK);
    assert(view.subtype == 'H');
    assert_bytes(view.name, view.name_length, "ARMToolbox39");
    assert(view.payload_size == 4u);
}

static void test_concrete_subtypes(void)
{
    uint8_t data[] = {
        'H','P','3','9','B','i','n','A', 0u, 0xa5u
    };
    for (uint8_t subtype = 'A'; subtype <= 'J'; ++subtype) {
        struct hpclang_hp39_container_view view;
        data[7] = subtype;
        assert(hpclang_hp39_container_parse(data, sizeof(data), &view) ==
               HPCLANG_HP39_CONTAINER_OK);
        assert(view.subtype == subtype);
        assert(view.name_length == 0u);
        assert(view.payload_size == 1u);
        assert(view.payload[0] == 0xa5u);
    }
}

static void test_malformed_containers(void)
{
    static const uint8_t short_header[] = "HP39Bin";
    static const uint8_t ascii_header[] = "HP39AscA ";
    static const uint8_t wildcard[] = {
        'H','P','3','9','B','i','n','K', 0u
    };
    static const uint8_t bad_subtype[] = {
        'H','P','3','9','B','i','n','@', 0u
    };
    static const uint8_t truncated_name[] = {
        'H','P','3','9','B','i','n','B', 4u, 'A','B'
    };
    const struct {
        const uint8_t *data;
        size_t size;
    } cases[] = {
        { short_header, sizeof(short_header) - 1u },
        { ascii_header, sizeof(ascii_header) - 1u },
        { wildcard, sizeof(wildcard) },
        { bad_subtype, sizeof(bad_subtype) },
        { truncated_name, sizeof(truncated_name) },
    };

    for (size_t i = 0u; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        struct hpclang_hp39_container_view view;
        assert(hpclang_hp39_container_parse(
                   cases[i].data, cases[i].size, &view) ==
               HPCLANG_HP39_CONTAINER_MALFORMED);
        assert(view.subtype == 0u);
        assert(view.name == NULL);
        assert(view.payload == NULL);
    }

    struct hpclang_hp39_container_view view;
    assert(hpclang_hp39_container_parse(NULL, 1u, &view) ==
           HPCLANG_HP39_CONTAINER_INVALID);
    assert(hpclang_hp39_container_parse(NULL, 0u, NULL) ==
           HPCLANG_HP39_CONTAINER_INVALID);
}

int main(void)
{
    test_hpgcc_aplet_header();
    test_hpgcc_library_header();
    test_concrete_subtypes();
    test_malformed_containers();
    return 0;
}
