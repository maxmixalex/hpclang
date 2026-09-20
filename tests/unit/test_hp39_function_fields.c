#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <hpclang/object.h>

#include "hp39-function-fields.h"

#define FIXTURE_NIBBLES 0x100000u
#define RPL_DORRP UINT32_C(0x00002a96)
#define RPL_DOLIST UINT32_C(0x00002a74)
#define RPL_DOROMP UINT32_C(0x00002e92)
#define RPL_DOREAL UINT32_C(0x00002933)
#define RPL_DOBINT UINT32_C(0x00002911)
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
        end = put_null_field(
            &fixture, item, field_prologues[i], i + 1u);
        previous_item = item;
    }

    /* Native Function carries five larger sections after its fixed prefix. */
    for (uint32_t i = 0u; i < 5u; ++i) {
        uint32_t link = end;
        item = link + 5u;
        put_word(&fixture, link, link - previous_item, 5u);
        end = put_null_field(&fixture, item, RPL_DOLIST, 0u);
        previous_item = item;
    }
    put_word(&fixture, root + 8u, previous_item - (root + 8u), 5u);

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

    /* A compatible-looking directory without the Function tTYPE is rejected. */
    put_word(&fixture, function_type + 10u, UINT32_C(0x123456), 6u);
    errno = 0;
    assert(hpclang_hp39_function_field_at(
               &reader, root, 0u, &field) == -1);
    assert(errno == ENOTSUP);

    return 0;
}
