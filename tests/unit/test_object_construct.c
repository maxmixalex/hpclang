#include <assert.h>
#include <errno.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <hpclang/object.h>

#include "object-snapshot.h"

static void test_integer(int64_t expected)
{
    struct hpclang_object *object = NULL;
    assert(hpclang_object_create_integer_i64(expected, &object) == 0);
    assert(object != NULL);
    assert(hpclang_object_kind(object) == HPCLANG_OBJECT_INTEGER);
    assert(object->native_prologue == 0x02614u);

    int64_t actual = 0;
    assert(hpclang_object_integer_i64(object, &actual) == 0);
    assert(actual == expected);
    hpclang_object_release(object);
}

static void test_real(double expected)
{
    struct hpclang_object *object = NULL;
    assert(hpclang_object_create_real_f64(expected, &object) == 0);
    assert(object != NULL);
    assert(hpclang_object_kind(object) == HPCLANG_OBJECT_REAL);
    assert(object->native_prologue == 0x02933u);
    assert(object->nibble_count == 21u);

    double actual = 0.0;
    assert(hpclang_object_real_f64(object, &actual) == 0);
    if (expected == 0.0) {
        assert(actual == 0.0);
        assert((signbit(actual) != 0) == (signbit(expected) != 0));
    } else {
        double ratio = actual / expected;
        assert(ratio > 0.99999999999);
        assert(ratio < 1.00000000001);
    }
    hpclang_object_release(object);
}

int main(void)
{
    test_integer(INT64_C(0));
    test_integer(INT64_C(123456789));
    test_integer(-INT64_C(987654321));
    test_integer(INT64_MAX);
    test_integer(INT64_MIN);

    test_real(0.0);
    test_real(-0.0);
    test_real(1.5);
    test_real(-2.25);
    test_real(0.1);
    test_real(123456789012.0);
    test_real(1.0e-300);
    test_real(1.0e300);

    struct hpclang_object *object = NULL;
    assert(hpclang_object_create_real_f64(9.999999999996, &object) == 0);
    double rounded = 0.0;
    assert(hpclang_object_real_f64(object, &rounded) == 0);
    assert(rounded == 10.0);
    hpclang_object_release(object);

    errno = 0;
    object = (struct hpclang_object *)(uintptr_t)1u;
    assert(hpclang_object_create_real_f64(INFINITY, &object) == -1);
    assert(object == NULL);
    assert(errno == EDOM);

    errno = 0;
    object = (struct hpclang_object *)(uintptr_t)1u;
    assert(hpclang_object_create_real_f64(NAN, &object) == -1);
    assert(object == NULL);
    assert(errno == EDOM);

    const uint8_t expected[] = {0x41u, 0x00u, 0xffu};
    object = NULL;

    assert(hpclang_object_create_string(expected, sizeof(expected), &object) == 0);
    assert(object != NULL);
    assert(hpclang_object_kind(object) == HPCLANG_OBJECT_STRING);
    assert(object->native_prologue == 0x02a2cu);
    assert(object->nibble_count == 16u);

    size_t size = 0u;
    assert(hpclang_object_string_size(object, &size) == 0);
    assert(size == sizeof(expected));

    uint8_t copied[sizeof(expected)] = {0};
    assert(hpclang_object_string_copy(object, copied, sizeof(copied)) == 0);
    assert(memcmp(copied, expected, sizeof(expected)) == 0);
    hpclang_object_release(object);

    object = NULL;
    assert(hpclang_object_create_string(NULL, 0u, &object) == 0);
    assert(object != NULL);
    assert(hpclang_object_string_size(object, &size) == 0);
    assert(size == 0u);
    hpclang_object_release(object);

    errno = 0;
    object = (struct hpclang_object *)(uintptr_t)1u;
    assert(hpclang_object_create_integer_i64(1, NULL) == -1);
    assert(errno == EFAULT);

    errno = 0;
    object = (struct hpclang_object *)(uintptr_t)1u;
    assert(hpclang_object_create_real_f64(1.0, NULL) == -1);
    assert(errno == EFAULT);

    errno = 0;
    object = (struct hpclang_object *)(uintptr_t)1u;
    assert(hpclang_object_create_string(NULL, 1u, &object) == -1);
    assert(object == NULL);
    assert(errno == EFAULT);

    return 0;
}