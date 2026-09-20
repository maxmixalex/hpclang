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

struct hp39_function_variable_schema {
    const char *category;
    uint8_t category_length;
    const char *name;
    uint8_t name_length;
    enum hpclang_hp39_function_variable_projection_kind kind;
    uint8_t fixed_field_index;
    uint16_t mask;
};

#define HP39_VAR_UNRESOLVED(category_, name_) \
    { category_, (uint8_t)(sizeof(category_) - 1u), \
      name_, (uint8_t)(sizeof(name_) - 1u), \
      HPCLANG_HP39_FUNCTION_VARIABLE_UNRESOLVED, UINT8_MAX, 0u }
#define HP39_VAR_DIRECT(category_, name_, field_) \
    { category_, (uint8_t)(sizeof(category_) - 1u), \
      name_, (uint8_t)(sizeof(name_) - 1u), \
      HPCLANG_HP39_FUNCTION_VARIABLE_DIRECT_FIELD, field_, 0u }
#define HP39_VAR_FLAG(category_, name_, field_, mask_) \
    { category_, (uint8_t)(sizeof(category_) - 1u), \
      name_, (uint8_t)(sizeof(name_) - 1u), \
      HPCLANG_HP39_FUNCTION_VARIABLE_FLAG_BOOLEAN, field_, mask_ }
#define HP39_VAR_FLAG_NOT(category_, name_, field_, mask_) \
    { category_, (uint8_t)(sizeof(category_) - 1u), \
      name_, (uint8_t)(sizeof(name_) - 1u), \
      HPCLANG_HP39_FUNCTION_VARIABLE_FLAG_BOOLEAN_INVERTED, field_, mask_ }

/*
 * Function ApletVars is the Topic Type property #8 schema at FPTR 8:02A
 * (bank-7 logical 0x599B0). Keep this table in the ROM-published category and
 * variable order, including unresolved entries.
 *
 * Direct-field mappings are backed by HPGCC39 Sharedvar.h + Funcdir.h and the
 * Stock fixed-offset accessors. PlotFlag/Plot2Flg masks and Table TFlags bits
 * come from HEADER.H. NumType is intentionally inverted: TBBuildBit set means
 * Build Your Own, while the published NumType value is 0 for Build Your Own
 * and 1 for Automatic.
 *
 * Similar-looking storage is deliberately not enough evidence: PageNum/Page,
 * F0..F9, NumRow/NumCol/NumIndep, Format/Digits, Xcross/Ycross and Indep stay
 * unresolved until their Stock getter semantics are mechanically closed.
 */
static const struct hp39_function_variable_schema hp39_function_variables[] = {
    HP39_VAR_UNRESOLVED("Sketch", "PageNum"),
    HP39_VAR_UNRESOLVED("Sketch", "Page"),
    HP39_VAR_UNRESOLVED("Note", "NoteText"),

    HP39_VAR_DIRECT("Numeric", "NumZoom", 24u),
    HP39_VAR_FLAG_NOT("Numeric", "NumType", 25u, 2u),
    HP39_VAR_DIRECT("Numeric", "NumStep", 23u),
    HP39_VAR_DIRECT("Numeric", "NumStart", 22u),
    HP39_VAR_UNRESOLVED("Numeric", "NumRow"),
    HP39_VAR_UNRESOLVED("Numeric", "NumIndep"),
    HP39_VAR_FLAG("Numeric", "NumFont", 25u, 1u),
    HP39_VAR_UNRESOLVED("Numeric", "NumCol"),
    HP39_VAR_UNRESOLVED("Numeric", "Format"),
    HP39_VAR_UNRESOLVED("Numeric", "Digits"),

    HP39_VAR_UNRESOLVED("Symbolic", "F0"),
    HP39_VAR_UNRESOLVED("Symbolic", "F9"),
    HP39_VAR_UNRESOLVED("Symbolic", "F8"),
    HP39_VAR_UNRESOLVED("Symbolic", "F7"),
    HP39_VAR_UNRESOLVED("Symbolic", "F6"),
    HP39_VAR_UNRESOLVED("Symbolic", "F5"),
    HP39_VAR_UNRESOLVED("Symbolic", "F4"),
    HP39_VAR_UNRESOLVED("Symbolic", "F3"),
    HP39_VAR_UNRESOLVED("Symbolic", "F2"),
    HP39_VAR_UNRESOLVED("Symbolic", "F1"),
    HP39_VAR_DIRECT("Symbolic", "Angle", 12u),

    HP39_VAR_DIRECT("Plot FCN", "Slope", 35u),
    HP39_VAR_DIRECT("Plot FCN", "Root", 33u),
    HP39_VAR_DIRECT("Plot FCN", "Isect", 36u),
    HP39_VAR_DIRECT("Plot FCN", "Extremum", 37u),
    HP39_VAR_DIRECT("Plot FCN", "Area", 34u),

    HP39_VAR_DIRECT("Plot", "Vzoom", 11u),
    HP39_VAR_DIRECT("Plot", "Hzoom", 10u),
    HP39_VAR_DIRECT("Plot", "Ymax", 3u),
    HP39_VAR_DIRECT("Plot", "Ymin", 2u),
    HP39_VAR_DIRECT("Plot", "Xmax", 1u),
    HP39_VAR_DIRECT("Plot", "Xmin", 0u),
    HP39_VAR_DIRECT("Plot", "VTick", 9u),
    HP39_VAR_DIRECT("Plot", "HTick", 8u),
    HP39_VAR_UNRESOLVED("Plot", "Ycross"),
    HP39_VAR_UNRESOLVED("Plot", "Xcross"),
    HP39_VAR_FLAG("Plot", "Tracing", 18u, 1024u),
    HP39_VAR_FLAG("Plot", "Simult", 17u, 256u),
    HP39_VAR_FLAG("Plot", "Recenter", 17u, 128u),
    HP39_VAR_FLAG("Plot", "Labels", 17u, 64u),
    HP39_VAR_FLAG("Plot", "InvCursor", 17u, 512u),
    HP39_VAR_UNRESOLVED("Plot", "Indep"),
    HP39_VAR_FLAG("Plot", "Grid", 17u, 16u),
    HP39_VAR_FLAG("Plot", "HighRes", 17u, 32u),
    HP39_VAR_FLAG("Plot", "Coord", 18u, 2048u),
    HP39_VAR_FLAG("Plot", "Connect", 17u, 4u),
    HP39_VAR_FLAG("Plot", "Axes", 17u, 2u),
};

_Static_assert(
    sizeof(hp39_function_variables) / sizeof(hp39_function_variables[0]) ==
        HPCLANG_HP39_FUNCTION_VARIABLE_COUNT,
    "Function ApletVars projection must stay ROM-schema complete");

#undef HP39_VAR_UNRESOLVED
#undef HP39_VAR_DIRECT
#undef HP39_VAR_FLAG
#undef HP39_VAR_FLAG_NOT

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

struct hp39_bint_buffer {
    uint8_t nibbles[10];
};

static int hp39_bint_buffer_read(
    void *context,
    uint32_t address,
    uint8_t *value)
{
    struct hp39_bint_buffer *buffer =
        (struct hp39_bint_buffer *)context;
    if (buffer == NULL || value == NULL || address >= 10u)
        return 0;
    *value = buffer->nibbles[address];
    return 1;
}

static int hp39_make_bint_snapshot(
    uint32_t value,
    struct hpclang_object **object)
{
    if (object == NULL) {
        errno = EINVAL;
        return -1;
    }
    *object = NULL;
    if (value > UINT32_C(0x000fffff)) {
        errno = ERANGE;
        return -1;
    }

    struct hp39_bint_buffer buffer = { { 0u } };
    for (uint32_t i = 0u; i < 5u; ++i) {
        buffer.nibbles[i] =
            (uint8_t)((HP39_RPL_DOBINT >> (i * 4u)) & 0x0fu);
        buffer.nibbles[5u + i] =
            (uint8_t)((value >> (i * 4u)) & 0x0fu);
    }

    struct hpclang_rpl_reader reader = {
        .read_nibble = hp39_bint_buffer_read,
        .context = &buffer,
    };
    return hpclang_object_snapshot_copy(&reader, 0u, 10u, object);
}

static void hp39_function_variable_projection_copy(
    const struct hp39_function_variable_schema *schema,
    struct hpclang_hp39_function_variable_projection *projection)
{
    projection->category = schema->category;
    projection->category_length = schema->category_length;
    projection->name = schema->name;
    projection->name_length = schema->name_length;
    projection->kind = schema->kind;
    projection->fixed_field_index =
        schema->kind == HPCLANG_HP39_FUNCTION_VARIABLE_UNRESOLVED
            ? HPCLANG_HP39_FUNCTION_VARIABLE_NO_FIELD
            : (uint32_t)schema->fixed_field_index;
    projection->mask = schema->mask;
}

int hpclang_hp39_function_variable_projection_at(
    uint32_t index,
    struct hpclang_hp39_function_variable_projection *projection)
{
    if (projection == NULL ||
        index >= HPCLANG_HP39_FUNCTION_VARIABLE_COUNT) {
        errno = EINVAL;
        return -1;
    }

    hp39_function_variable_projection_copy(
        &hp39_function_variables[index], projection);
    return 0;
}

int hpclang_hp39_function_variable_projection_find(
    const uint8_t *category,
    size_t category_length,
    const uint8_t *name,
    size_t name_length,
    struct hpclang_hp39_function_variable_projection *projection)
{
    if (category == NULL || category_length == 0u ||
        name == NULL || name_length == 0u || projection == NULL) {
        errno = EINVAL;
        return -1;
    }

    for (uint32_t i = 0u;
         i < HPCLANG_HP39_FUNCTION_VARIABLE_COUNT;
         ++i) {
        const struct hp39_function_variable_schema *schema =
            &hp39_function_variables[i];
        if (category_length == schema->category_length &&
            name_length == schema->name_length &&
            memcmp(category, schema->category, category_length) == 0 &&
            memcmp(name, schema->name, name_length) == 0) {
            hp39_function_variable_projection_copy(schema, projection);
            return 0;
        }
    }

    errno = ENOENT;
    return -1;
}

int hpclang_hp39_function_variable_snapshot(
    const struct hpclang_rpl_reader *reader,
    uint32_t aplet_object,
    const uint8_t *category,
    size_t category_length,
    const uint8_t *name,
    size_t name_length,
    struct hpclang_object **object)
{
    if (object == NULL) {
        errno = EINVAL;
        return -1;
    }
    *object = NULL;

    struct hpclang_hp39_function_variable_projection projection;
    if (hpclang_hp39_function_variable_projection_find(
            category,
            category_length,
            name,
            name_length,
            &projection) != 0)
        return -1;

    if (projection.kind ==
        HPCLANG_HP39_FUNCTION_VARIABLE_UNRESOLVED) {
        errno = ENOTSUP;
        return -1;
    }

    if (projection.kind ==
        HPCLANG_HP39_FUNCTION_VARIABLE_DIRECT_FIELD)
        return hpclang_hp39_function_field_snapshot(
            reader,
            aplet_object,
            projection.fixed_field_index,
            object);

    struct hpclang_hp39_function_field_view field;
    if (hpclang_hp39_function_field_at(
            reader,
            aplet_object,
            projection.fixed_field_index,
            &field) != 0)
        return -1;
    if (field.prologue != HP39_RPL_DOBINT) {
        errno = EILSEQ;
        return -1;
    }

    uint32_t flags = 0u;
    if (!hp39_read_word(
            reader, field.object_address + 5u, 5u, &flags)) {
        errno = EIO;
        return -1;
    }

    uint32_t value = (flags & projection.mask) != 0u ? 1u : 0u;
    if (projection.kind ==
        HPCLANG_HP39_FUNCTION_VARIABLE_FLAG_BOOLEAN_INVERTED)
        value ^= 1u;

    return hp39_make_bint_snapshot(value, object);
}
