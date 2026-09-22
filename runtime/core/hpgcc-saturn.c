#include <stddef.h>
#include <stdint.h>
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
