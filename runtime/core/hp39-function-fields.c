#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "hp39-function-fields.h"
#include "object-snapshot.h"
#include "rpl-object-store.h"

#define HP39_SATURN_LIMIT UINT32_C(0x00100000)
#define HP39_RPL_DOLIST UINT32_C(0x00002a74)
#define HP39_RPL_DOROMP UINT32_C(0x00002e92)
#define HP39_RPL_DOBINT UINT32_C(0x00002911)
#define HP39_RPL_DOREAL UINT32_C(0x00002933)
#define HP39_RPL_SEMI UINT32_C(0x0000312b)

struct hp39_function_field_schema {
    const char *name;
    uint8_t name_length;
    uint32_t prologue;
};

/*
 * Source authority: HPGCC39's HP39-Aplet/Skeleton/=Sharedvar.h followed by
 * =Funcdir.h. These are the fixed-length Function Aplet fields, not the final
 * VAR-menu/ApletVars projection. Internal fields such as PlotCKSUM therefore
 * remain represented here even if a later public Variable service hides them.
 */
static const struct hp39_function_field_schema hp39_function_fields[] = {
    { "Xmin", 4u, HP39_RPL_DOREAL },
    { "Xmax", 4u, HP39_RPL_DOREAL },
    { "Ymin", 4u, HP39_RPL_DOREAL },
    { "Ymax", 4u, HP39_RPL_DOREAL },
    { "Xmin2", 5u, HP39_RPL_DOREAL },
    { "Xmax2", 5u, HP39_RPL_DOREAL },
    { "Ymin2", 5u, HP39_RPL_DOREAL },
    { "Ymax2", 5u, HP39_RPL_DOREAL },
    { "HTick", 5u, HP39_RPL_DOREAL },
    { "VTick", 5u, HP39_RPL_DOREAL },
    { "HZoom", 5u, HP39_RPL_DOREAL },
    { "VZoom", 5u, HP39_RPL_DOREAL },
    { "Angle", 5u, HP39_RPL_DOBINT },
    { "LastX", 5u, HP39_RPL_DOREAL },
    { "LastY", 5u, HP39_RPL_DOREAL },
    { "LastIndep", 9u, HP39_RPL_DOREAL },
    { "LastEq", 6u, HP39_RPL_DOBINT },
    { "PlotFlag", 8u, HP39_RPL_DOBINT },
    { "Plot2Flg", 8u, HP39_RPL_DOBINT },
    { "PlotCKSUM", 9u, HP39_RPL_DOREAL },
    { "SplitCKSUM", 10u, HP39_RPL_DOREAL },
    { "Split2CKSUM", 11u, HP39_RPL_DOREAL },
    { "TStart", 6u, HP39_RPL_DOREAL },
    { "TStep", 5u, HP39_RPL_DOREAL },
    { "TZoom", 5u, HP39_RPL_DOREAL },
    { "TFlags", 6u, HP39_RPL_DOBINT },
    { "Trow", 4u, HP39_RPL_DOBINT },
    { "Tcol", 4u, HP39_RPL_DOBINT },
    { "TCol1", 5u, HP39_RPL_DOBINT },
    { "SplitRow", 8u, HP39_RPL_DOBINT },
    { "BTRow1", 6u, HP39_RPL_DOBINT },
    { "Format", 6u, HP39_RPL_DOBINT },
    { "Digits", 6u, HP39_RPL_DOBINT },
    { "Root", 4u, HP39_RPL_DOREAL },
    { "Area", 4u, HP39_RPL_DOREAL },
    { "Slope", 5u, HP39_RPL_DOREAL },
    { "Isect", 5u, HP39_RPL_DOREAL },
    { "Extremum", 8u, HP39_RPL_DOREAL },
};

_Static_assert(
    sizeof(hp39_function_fields) / sizeof(hp39_function_fields[0]) ==
        HPCLANG_HP39_FUNCTION_FIXED_FIELD_COUNT,
    "Function fixed-field schema must stay source-complete");

static int hp39_read_word(
    const struct hpclang_rpl_reader *reader,
    uint32_t address,
    uint32_t nibbles,
    uint32_t *value)
{
    if (reader == NULL || reader->read_nibble == NULL || value == NULL ||
        nibbles == 0u || nibbles > 8u || address >= HP39_SATURN_LIMIT ||
        nibbles > HP39_SATURN_LIMIT - address)
        return 0;

    uint32_t out = 0u;
    for (uint32_t i = 0u; i < nibbles; ++i) {
        uint8_t nibble = 0u;
        if (!reader->read_nibble(reader->context, address + i, &nibble) ||
            nibble > 0x0fu)
            return 0;
        out |= (uint32_t)nibble << (i * 4u);
    }
    *value = out;
    return 1;
}

static int hp39_function_type_ok(
    const struct hpclang_rpl_reader *reader,
    uint32_t object_address)
{
    /*
     * Native Function tTYPE:
     * { nFUNCTOPIC nFUNCTION xFUNCTAB xFUNCSYMB xDFLTNOTE xDFLTPICT }
     * SUPROM39.A encodes these six ROMPTR payloads exactly as below.
     */
    static const uint32_t romptrs[] = {
        UINT32_C(0x000f0f1),
        UINT32_C(0x00000f1),
        UINT32_C(0x00080f1),
        UINT32_C(0x002b0f1),
        UINT32_C(0x00320f1),
        UINT32_C(0x00330f1),
    };

    uint32_t prologue = 0u;
    if (!hp39_read_word(reader, object_address, 5u, &prologue))
        return 0;
    if (prologue != HP39_RPL_DOLIST)
        return 0;

    uint32_t cursor = object_address + 5u;
    for (uint32_t i = 0u;
         i < (uint32_t)(sizeof(romptrs) / sizeof(romptrs[0]));
         ++i) {
        uint32_t payload = 0u;
        if (!hp39_read_word(reader, cursor, 5u, &prologue) ||
            prologue != HP39_RPL_DOROMP ||
            !hp39_read_word(reader, cursor + 5u, 6u, &payload) ||
            payload != romptrs[i])
            return 0;
        cursor += 11u;
    }

    return hp39_read_word(reader, cursor, 5u, &prologue) &&
        prologue == HP39_RPL_SEMI;
}

static int hp39_function_entry_count_and_type(
    const struct hpclang_rpl_reader *reader,
    uint32_t aplet_object,
    uint32_t *entry_count)
{
    if (entry_count == NULL) {
        errno = EINVAL;
        return -1;
    }
    *entry_count = 0u;

    struct hpclang_rpl_directory_cursor cursor;
    int result = hpclang_rpl_directory_cursor_open(
        reader, aplet_object, &cursor);
    if (result != HPCLANG_RPL_STORE_OK) {
        errno = result == HPCLANG_RPL_STORE_UNAVAILABLE ? EIO : EILSEQ;
        return -1;
    }

    struct hpclang_rpl_directory_entry_view entry;
    struct hpclang_rpl_directory_entry_view oldest = { 0u };
    uint32_t count = 0u;
    while ((result = hpclang_rpl_directory_cursor_next(
                reader, &cursor, &entry)) > 0) {
        oldest = entry;
        ++count;
    }
    if (result < 0) {
        errno = result == HPCLANG_RPL_STORE_LIMIT ? EOVERFLOW : EILSEQ;
        return -1;
    }

    if (count < 1u + HPCLANG_HP39_FUNCTION_FIXED_FIELD_COUNT ||
        oldest.name_length != 0u ||
        !hp39_function_type_ok(reader, oldest.object_address)) {
        errno = ENOTSUP;
        return -1;
    }

    *entry_count = count;
    return 0;
}

int hpclang_hp39_function_field_at(
    const struct hpclang_rpl_reader *reader,
    uint32_t aplet_object,
    uint32_t index,
    struct hpclang_hp39_function_field_view *field)
{
    if (reader == NULL || reader->read_nibble == NULL || field == NULL ||
        index >= HPCLANG_HP39_FUNCTION_FIXED_FIELD_COUNT) {
        errno = EINVAL;
        return -1;
    }
    memset(field, 0, sizeof(*field));

    uint32_t count = 0u;
    if (hp39_function_entry_count_and_type(
            reader, aplet_object, &count) != 0)
        return -1;

    /* Cursor order is newest->oldest. Oldest is tTYPE, then fields 0..37. */
    uint32_t wanted_from_newest = count - 2u - index;
    struct hpclang_rpl_directory_cursor cursor;
    int result = hpclang_rpl_directory_cursor_open(
        reader, aplet_object, &cursor);
    if (result != HPCLANG_RPL_STORE_OK) {
        errno = EILSEQ;
        return -1;
    }

    struct hpclang_rpl_directory_entry_view entry;
    uint32_t ordinal = 0u;
    while ((result = hpclang_rpl_directory_cursor_next(
                reader, &cursor, &entry)) > 0) {
        if (ordinal == wanted_from_newest) {
            const struct hp39_function_field_schema *schema =
                &hp39_function_fields[index];
            if (entry.name_length != 0u ||
                entry.prologue != schema->prologue) {
                errno = EILSEQ;
                return -1;
            }

            field->name = schema->name;
            field->name_length = schema->name_length;
            field->object_address = entry.object_address;
            field->prologue = entry.prologue;
            return 0;
        }
        ++ordinal;
    }

    errno = result < 0 ? EILSEQ : ENOENT;
    return -1;
}

int hpclang_hp39_function_field_find(
    const struct hpclang_rpl_reader *reader,
    uint32_t aplet_object,
    const uint8_t *name,
    size_t name_length,
    struct hpclang_hp39_function_field_view *field)
{
    if (name == NULL || name_length == 0u || field == NULL) {
        errno = EINVAL;
        return -1;
    }

    for (uint32_t i = 0u; i < HPCLANG_HP39_FUNCTION_FIXED_FIELD_COUNT; ++i) {
        const struct hp39_function_field_schema *schema =
            &hp39_function_fields[i];
        if (name_length == schema->name_length &&
            memcmp(name, schema->name, name_length) == 0)
            return hpclang_hp39_function_field_at(
                reader, aplet_object, i, field);
    }

    errno = ENOENT;
    return -1;
}

int hpclang_hp39_function_field_snapshot(
    const struct hpclang_rpl_reader *reader,
    uint32_t aplet_object,
    uint32_t index,
    struct hpclang_object **object)
{
    if (object == NULL) {
        errno = EINVAL;
        return -1;
    }
    *object = NULL;

    struct hpclang_hp39_function_field_view field;
    if (hpclang_hp39_function_field_at(
            reader, aplet_object, index, &field) != 0)
        return -1;

    return hpclang_object_snapshot_copy(
        reader,
        field.object_address,
        HP39_SATURN_LIMIT - field.object_address,
        object);
}
