#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <hpclang/object.h>
#include <saturn.h>

enum {
    SAT_STACK_SCOPE_TOKEN = 0x48504331u
};

unsigned sat_stack_init(void)
{
    uint32_t depth = 0u;
    if (hpclang_stack_depth(&depth) != 0)
        return 0u;
    (void)depth;
    return SAT_STACK_SCOPE_TOKEN;
}

void sat_stack_exit(unsigned rpl_stack_bias)
{
    /* Launcher bias is already hidden by the canonical hpclang stack view. */
    (void)rpl_stack_bias;
}

int sat_stack_depth(void)
{
    uint32_t depth = 0u;
    if (hpclang_stack_depth(&depth) != 0)
        return 0;
    if (depth > (uint32_t)INT32_MAX)
        return INT32_MAX;
    return (int)depth;
}

int sat_stack_drop(void)
{
    uint32_t depth = 0u;
    if (hpclang_stack_depth(&depth) != 0 || depth == 0u)
        return 0;
    return hpclang_stack_drop(1u) == 0 ? 1 : 0;
}

int sat_stack_dropn(int n)
{
    if (n <= 0)
        return 0;

    uint32_t depth = 0u;
    if (hpclang_stack_depth(&depth) != 0 || depth == 0u)
        return 0;

    uint32_t requested = (uint32_t)n;
    uint32_t count = requested < depth ? requested : depth;
    if (hpclang_stack_drop(count) != 0)
        return 0;
    return (int)count;
}

static struct hpclang_object *sat_pick_object(uint32_t level)
{
    struct hpclang_object *object = NULL;
    if (hpclang_stack_peek(level, &object) != 0)
        return NULL;
    return object;
}

char *sat_pick_str(int level, char *buf)
{
    if (level <= 0 || buf == NULL)
        return NULL;

    struct hpclang_object *object = sat_pick_object((uint32_t)level);
    if (object == NULL)
        return NULL;

    size_t byte_count = 0u;
    if (hpclang_object_string_size(object, &byte_count) != 0 ||
        hpclang_object_string_copy(
            object,
            (uint8_t *)buf,
            byte_count) != 0) {
        hpclang_object_release(object);
        return NULL;
    }

    buf[byte_count] = '\0';
    hpclang_object_release(object);
    return buf;
}

char *sat_pick_str_alloc(int level)
{
    if (level <= 0)
        return NULL;

    struct hpclang_object *object = sat_pick_object((uint32_t)level);
    if (object == NULL)
        return NULL;

    size_t byte_count = 0u;
    if (hpclang_object_string_size(object, &byte_count) != 0 ||
        byte_count == SIZE_MAX) {
        hpclang_object_release(object);
        return NULL;
    }

    char *buf = malloc(byte_count + 1u);
    if (buf == NULL) {
        hpclang_object_release(object);
        return NULL;
    }
    if (hpclang_object_string_copy(
            object,
            (uint8_t *)buf,
            byte_count) != 0) {
        free(buf);
        hpclang_object_release(object);
        return NULL;
    }

    buf[byte_count] = '\0';
    hpclang_object_release(object);
    return buf;
}

char *sat_pop_str(char *buf)
{
    char *result = sat_pick_str(1, buf);
    if (result == NULL)
        return NULL;
    if (hpclang_stack_drop(1u) != 0)
        return NULL;
    return result;
}

char *sat_pop_str_alloc(void)
{
    char *result = sat_pick_str_alloc(1);
    if (result == NULL)
        return NULL;
    if (hpclang_stack_drop(1u) != 0) {
        free(result);
        return NULL;
    }
    return result;
}

static void sat_unquote_owned(char *value, char quote)
{
    if (value == NULL)
        return;

    size_t length = strlen(value);
    if (length >= 2u && value[0] == quote && value[length - 1u] == quote) {
        memmove(value, value + 1u, length - 2u);
        value[length - 2u] = '\0';
    }
}

int _sat_stack_pop_stdargs(const char *arg0, char ***argv)
{
    if (argv == NULL)
        return 0;
    *argv = NULL;

    uint32_t depth = 0u;
    if (hpclang_stack_depth(&depth) != 0 || depth > (uint32_t)INT32_MAX)
        return 0;

    size_t arg_start = arg0 != NULL ? 1u : 0u;
    if ((size_t)depth > SIZE_MAX - arg_start)
        return 0;
    size_t argc = (size_t)depth + arg_start;
    if (argc == 0u)
        return 0;
    if (argc > SIZE_MAX / sizeof(char *))
        return 0;

    char **result = calloc(argc, sizeof(*result));
    if (result == NULL)
        return 0;

    if (arg0 != NULL)
        result[0] = (char *)arg0;

    for (uint32_t i = 0u; i < depth; ++i) {
        uint32_t level = depth - i;
        char *value = sat_pick_str_alloc((int)level);
        if (value == NULL) {
            for (uint32_t j = 0u; j < i; ++j)
                free(result[arg_start + j]);
            free(result);
            return 0;
        }
        sat_unquote_owned(value, '\'');
        result[arg_start + i] = value;
    }

    if (depth != 0u && hpclang_stack_drop(depth) != 0) {
        for (uint32_t i = 0u; i < depth; ++i)
            free(result[arg_start + i]);
        free(result);
        return 0;
    }

    *argv = result;
    return (int)argc;
}

int sat_stack_pop_stdargs(char ***argv)
{
    return _sat_stack_pop_stdargs(NULL, argv);
}

int sat_stack_push_string(const char *str)
{
    if (str == NULL)
        return 0;

    struct hpclang_object *object = NULL;
    if (hpclang_object_create_string(
            (const uint8_t *)str,
            strlen(str),
            &object) != 0) {
        return 0;
    }

    int result = hpclang_stack_push(object) == 0 ? 1 : 0;
    hpclang_object_release(object);
    return result;
}

static int sat_format_int(char *buffer, size_t capacity, int value, int radix)
{
    static const char digits[] = "0123456789abcdefghijklmnopqrstuvwxyz";
    char reversed[sizeof(unsigned int) * 8u + 1u];
    size_t count = 0u;
    size_t prefix = 0u;

    if (buffer == NULL || capacity == 0u || radix < 2 || radix > 36)
        return 0;

    unsigned int magnitude;
    if (value < 0) {
        if (capacity < 2u)
            return 0;
        buffer[0] = '-';
        prefix = 1u;
        magnitude = 0u - (unsigned int)value;
    } else {
        magnitude = (unsigned int)value;
    }

    do {
        reversed[count++] = digits[magnitude % (unsigned int)radix];
        magnitude /= (unsigned int)radix;
    } while (magnitude != 0u);

    if (prefix + count + 1u > capacity)
        return 0;

    for (size_t i = 0u; i < count; ++i)
        buffer[prefix + i] = reversed[count - i - 1u];
    buffer[prefix + count] = '\0';
    return 1;
}

int _sat_stack_push_int(int z, int radix, int binary)
{
    char digits[sizeof(unsigned int) * 8u + 2u];
    if (!sat_format_int(digits, sizeof(digits), z, radix))
        return 0;

    char suffix = '\0';
    if (binary) {
        switch (radix) {
        case 8:
            suffix = 'o';
            break;
        case 10:
            suffix = 'd';
            break;
        case 16:
            suffix = 'h';
            break;
        default:
            break;
        }
    }

    if (suffix == '\0')
        return sat_stack_push_string(digits);

    char formatted[sizeof(digits) + 3u];
    formatted[0] = '#';
    size_t digit_count = strlen(digits);
    memcpy(formatted + 1u, digits, digit_count);
    formatted[digit_count + 1u] = suffix;
    formatted[digit_count + 2u] = '\0';
    return sat_stack_push_string(formatted);
}

int sat_stack_push_int(int z)
{
    return _sat_stack_push_int(z, 10, 0);
}

int sat_stack_push_double(double d)
{
    /* Historical HPGCC uses sprintf("%f") and pushes the resulting String. */
    char buffer[384];
    int length = snprintf(buffer, sizeof(buffer), "%f", d);
    if (length < 0 || (size_t)length >= sizeof(buffer))
        return 0;
    return sat_stack_push_string(buffer);
}

BOOL sat_push_zint_llong(LONGLONG num)
{
    struct hpclang_object *object = NULL;
    if (hpclang_object_create_integer_i64((int64_t)num, &object) != 0)
        return FALSE;

    BOOL result = hpclang_stack_push(object) == 0 ? TRUE : FALSE;
    hpclang_object_release(object);
    return result;
}

LONGLONG sat_pop_zint_llong(void)
{
    struct hpclang_object *object = sat_pick_object(1u);
    if (object == NULL)
        return 0;

    int64_t value = 0;
    if (hpclang_object_integer_i64(object, &value) != 0) {
        hpclang_object_release(object);
        return 0;
    }
    hpclang_object_release(object);

    if (hpclang_stack_drop(1u) != 0)
        return 0;
    return (LONGLONG)value;
}

BOOL sat_push_real(double number)
{
    struct hpclang_object *object = NULL;
    if (hpclang_object_create_real_f64(number, &object) != 0)
        return FALSE;

    BOOL result = hpclang_stack_push(object) == 0 ? TRUE : FALSE;
    hpclang_object_release(object);
    return result;
}

double sat_pop_real(void)
{
    struct hpclang_object *object = sat_pick_object(1u);
    if (object == NULL)
        return 0.0;

    double value = 0.0;
    if (hpclang_object_real_f64(object, &value) != 0) {
        hpclang_object_release(object);
        return 0.0;
    }
    hpclang_object_release(object);

    if (hpclang_stack_drop(1u) != 0)
        return 0.0;
    return value;
}

BOOL sat_push_char(char byte)
{
    struct hpclang_object *object = NULL;
    if (hpclang_object_create_character((uint8_t)byte, &object) != 0)
        return FALSE;

    BOOL result = hpclang_stack_push(object) == 0 ? TRUE : FALSE;
    hpclang_object_release(object);
    return result;
}