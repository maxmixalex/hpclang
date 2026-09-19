#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "hp39-object-directory.h"

static void assert_bytes(
    const uint8_t *actual, size_t actual_size, const char *expected)
{
    size_t expected_size = strlen(expected);
    assert(actual_size == expected_size);
    assert(memcmp(actual, expected, expected_size) == 0);
}

static void test_hpgcc_root_fixture(void)
{
    static const uint8_t data[] =
        "HP39AscA B 12 HPGCC000.00017 HPCLANG HPC1 TEST H 11 LIB275L.0007 LIB275L ";
    struct hpclang_hp39_directory_cursor cursor;
    struct hpclang_hp39_directory_entry_view entry;
    assert(hpclang_hp39_directory_cursor_open(
               data, sizeof(data) - 1u, &cursor) == HPCLANG_HP39_DIRECTORY_OK);

    assert(hpclang_hp39_directory_cursor_next(&cursor, &entry) == 1);
    assert(entry.subtype == 'B');
    assert_bytes(entry.filename, entry.filename_length, "HPGCC000.000");
    assert_bytes(entry.name, entry.name_length, "HPCLANG HPC1 TEST");

    assert(hpclang_hp39_directory_cursor_next(&cursor, &entry) == 1);
    assert(entry.subtype == 'H');
    assert_bytes(entry.filename, entry.filename_length, "LIB275L.000");
    assert_bytes(entry.name, entry.name_length, "LIB275L");
    assert(hpclang_hp39_directory_cursor_next(&cursor, &entry) == 0);
}

static void test_hpgcc_current_fixture(void)
{
    static const uint8_t data[] = "HP39AscA A 11 HP39DIR.0004 Root";
    struct hpclang_hp39_directory_cursor cursor;
    struct hpclang_hp39_directory_entry_view entry;
    assert(hpclang_hp39_directory_cursor_open(
               data, sizeof(data) - 1u, &cursor) == HPCLANG_HP39_DIRECTORY_OK);
    assert(hpclang_hp39_directory_cursor_next(&cursor, &entry) == 1);
    assert(entry.subtype == 'A');
    assert_bytes(entry.filename, entry.filename_length, "HP39DIR.000");
    assert_bytes(entry.name, entry.name_length, "Root");
    assert(hpclang_hp39_directory_cursor_next(&cursor, &entry) == 0);
}

static void test_empty_directory(void)
{
    static const uint8_t data[] = "HP39AscA ";
    struct hpclang_hp39_directory_cursor cursor;
    struct hpclang_hp39_directory_entry_view entry;
    assert(hpclang_hp39_directory_cursor_open(
               data, sizeof(data) - 1u, &cursor) == HPCLANG_HP39_DIRECTORY_OK);
    assert(hpclang_hp39_directory_cursor_next(&cursor, &entry) == 0);
}

static void test_malformed_records(void)
{
    static const uint8_t bad_header[] = "HP39BinA B 1 X1 Y";
    static const uint8_t truncated_filename[] = "HP39AscA B 12 SHORT";
    static const uint8_t missing_length[] = "HP39AscA B X FILE1 N";
    static const uint8_t bad_subtype[] = "HP39AscA Z 1 X1 N";
    static const uint8_t bad_tail[] = "HP39AscA B 1 X1 N!";
    static const uint8_t overflow_length[] =
        "HP39AscA B 999999999999999999999999999999999999999 X1 N";
    const struct {
        const uint8_t *data;
        size_t size;
        int open_result;
    } cases[] = {
        { bad_header, sizeof(bad_header) - 1u, HPCLANG_HP39_DIRECTORY_MALFORMED },
        { truncated_filename, sizeof(truncated_filename) - 1u, HPCLANG_HP39_DIRECTORY_OK },
        { missing_length, sizeof(missing_length) - 1u, HPCLANG_HP39_DIRECTORY_OK },
        { bad_subtype, sizeof(bad_subtype) - 1u, HPCLANG_HP39_DIRECTORY_OK },
        { bad_tail, sizeof(bad_tail) - 1u, HPCLANG_HP39_DIRECTORY_OK },
        { overflow_length, sizeof(overflow_length) - 1u, HPCLANG_HP39_DIRECTORY_OK },
    };

    for (size_t i = 0u; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        struct hpclang_hp39_directory_cursor cursor;
        struct hpclang_hp39_directory_entry_view entry;
        int result = hpclang_hp39_directory_cursor_open(
            cases[i].data, cases[i].size, &cursor);
        assert(result == cases[i].open_result);
        if (result == HPCLANG_HP39_DIRECTORY_OK)
            assert(hpclang_hp39_directory_cursor_next(&cursor, &entry) < 0);
    }
}

int main(void)
{
    test_hpgcc_root_fixture();
    test_hpgcc_current_fixture();
    test_empty_directory();
    test_malformed_records();
    return 0;
}
