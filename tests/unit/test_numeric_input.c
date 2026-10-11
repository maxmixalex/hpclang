#include <assert.h>
#include <errno.h>
#include <float.h>
#include <math.h>
#include <string.h>
#include <hpclang/input.h>

static struct hpclang_text_input input;
static char buffer[128];

static void line(const char *text)
{
    assert(hpclang_text_input_init(&input, buffer, sizeof(buffer)) == 0);
    for (; *text; ++text)
        assert(hpclang_text_input_edit(&input, HPCLANG_INPUT_INSERT, (unsigned char)*text) ==
               HPCLANG_INPUT_EDITING);
}

static void integer(const char *text, int64_t expected)
{
    line(text);
    int64_t value = 17;
    errno = EDOM;
    assert(hpclang_text_input_parse_int64(&input, &value) == 0 && value == expected);
    assert(errno == EDOM && strcmp(buffer, text) == 0 && input.cursor == strlen(text));
}

static void real(const char *text, double expected)
{
    line(text);
    double value = 17.0;
    errno = EDOM;
    assert(hpclang_text_input_parse_double(&input, &value) == 0 && value == expected);
    assert(!!signbit(value) == !!signbit(expected));
    assert(errno == EDOM && strcmp(buffer, text) == 0 && input.cursor == strlen(text));
}

static void reject(const char *text, int integer_error, int real_error)
{
    line(text);
    int64_t i = 17;
    double d = 17.0;
    if (integer_error) {
        errno = 0;
        assert(hpclang_text_input_parse_int64(&input, &i) == -1 && errno == integer_error);
        assert(i == 17);
    }
    if (real_error) {
        errno = 0;
        assert(hpclang_text_input_parse_double(&input, &d) == -1 && errno == real_error);
        assert(d == 17.0);
    }
    assert(strcmp(buffer, text) == 0 && input.length == strlen(text) &&
           input.cursor == input.length);
}

int main(void)
{
    integer("0", 0);
    integer("-0", 0);
    integer("+00042", 42);
    integer("9007199254740993", INT64_C(9007199254740993));
    integer("9223372036854775807", INT64_MAX);
    integer("-9223372036854775808", INT64_MIN);
    reject("9223372036854775808", ERANGE, 0);
    reject("-9223372036854775809", ERANGE, 0);
    reject("99999999999999999999999999999999999x", EINVAL, EINVAL);
    const char *invalid[] = {"",    "+",   "-",   ".",         "+.",  "e1",   "1e",    "1e+",
                             "1e-", "--1", "1 2", " 1",        "1 ",  "1,5",  "0x10",  "0x1p2",
                             "nan", "NaN", "inf", "-Infinity", "1+2", "1..0", "1e2.0", "1e2e3"};
    for (unsigned i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i)
        reject(invalid[i], EINVAL, EINVAL);
    reject("1.0", EINVAL, 0);
    reject("1e2", EINVAL, 0);
    real("+00042", 42.0);
    real("-0", -0.0);
    real("-0e99999", -0.0);
    real("0e-99999", 0.0);
    real(".5", 0.5);
    real("1.", 1.0);
    real("-1.25E+2", -125.0);
    real("2.5e-1", 0.25);
    real("1.7976931348623157e308", DBL_MAX);
    real("2.2250738585072014e-308", DBL_MIN);
    real("4.9406564584124654e-324", DBL_TRUE_MIN);
    real("-4.9406564584124654e-324", -DBL_TRUE_MIN);
    reject("1e309", EINVAL, ERANGE);
    reject("-1e309", EINVAL, ERANGE);
    reject("1e-99999", EINVAL, ERANGE);
    reject("1e-324", EINVAL, ERANGE);

    line("1234");
    assert(hpclang_text_input_edit(&input, HPCLANG_INPUT_HOME, 0) == HPCLANG_INPUT_EDITING);
    int64_t at_home = 0;
    assert(hpclang_text_input_parse_int64(&input, &at_home) == 0 && at_home == 1234);
    assert(input.cursor == 0 && input.length == 4 && strcmp(buffer, "1234") == 0);

    /* ACCEPT can be rejected by the application, then corrected in place. */
    line("-1e");
    double value = 9.0;
    assert(hpclang_text_input_edit(&input, HPCLANG_INPUT_ACCEPT, 0) == HPCLANG_INPUT_ACCEPTED);
    assert(hpclang_text_input_parse_double(&input, &value) == -1 && errno == EINVAL);
    struct hpclang_keyboard_event key = {.type = HPCLANG_KEYBOARD_EVENT_DOWN, .key = HPCLANG_KEY_2};
    assert(hpclang_text_input_key(&input, &key) == HPCLANG_INPUT_EDITING);
    assert(hpclang_text_input_parse_double(&input, &value) == 0 && value == -100.0);
    assert(hpclang_text_input_edit(&input, HPCLANG_INPUT_CANCEL, 0) == HPCLANG_INPUT_CANCELLED);
    assert(strcmp(buffer, "-1e2") == 0);

    int64_t whole = 9;
    assert(hpclang_text_input_parse_int64(NULL, &whole) == -1 && errno == EINVAL);
    assert(hpclang_text_input_parse_double(NULL, &value) == -1 && errno == EINVAL);
    assert(hpclang_text_input_parse_int64(&input, NULL) == -1 && errno == EINVAL);
    assert(hpclang_text_input_parse_double(&input, NULL) == -1 && errno == EINVAL);
    line("12");
    input.capacity = input.length;
    assert(hpclang_text_input_parse_int64(&input, &whole) == -1 && errno == EINVAL);
    assert(hpclang_text_input_parse_double(&input, &value) == -1 && errno == EINVAL);
    line("12");
    buffer[1] = '\0';
    assert(hpclang_text_input_parse_int64(&input, &whole) == -1 && errno == EINVAL);
    assert(hpclang_text_input_parse_double(&input, &value) == -1 && errno == EINVAL);
    line("12");
    buffer[input.length] = '3';
    assert(hpclang_text_input_parse_int64(&input, &whole) == -1 && errno == EINVAL);
    assert(hpclang_text_input_parse_double(&input, &value) == -1 && errno == EINVAL);
    assert(whole == 9 && value == -100.0);
    return 0;
}
