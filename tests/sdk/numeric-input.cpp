#include <hpstdio.h>
#define HPCLANG_HPCONIO_NO_AUTOINIT
#include <hpconio.h>
#include <hpsys.h>
#include <hpkeyb49.h>
#include <hpclang/stdio.h>
#include <cerrno>
#include <climits>
#include <type_traits>

static_assert(std::is_same_v<decltype(readlonglong()), LONGLONG>);
static_assert(std::is_same_v<decltype(readulonglong()), ULONGLONG>);

static const unsigned char *input;
static int read_byte(void *)
{
    return *input ? *input++ : HPCLANG_STDIO_EOF;
}

int main()
{
    input = reinterpret_cast<const unsigned char *>(
        "-2147483648\n4294967295\nAbCd\n-9223372036854775808\n18446744073709551615\n1.25\n"
        "ABCDEF\nXY\bZ\n");
    hpclang_stdio_callbacks callbacks{};
    callbacks.read = read_byte;
    if (hpclang_stdio_bind(&callbacks))
        return 1;
    errno = 0;
    bool valid = readint() == INT_MIN && readuint() == UINT_MAX && readhex() == 0xabcd &&
                 readlonglong() == LLONG_MIN && readulonglong() == ULLONG_MAX &&
                 readdouble() == 1.25 && !errno;
    char first[4], second[4];
    static_assert(std::is_same_v<decltype(gets(first)), char *>);
    valid = valid && gets(first) == first && !strcmp(first, "ABC") && _gets(second) == second &&
            !strcmp(second, "XZ");
    int detached = hpclang_stdio_unbind();
    return valid && !detached ? 0 : 2;
}
