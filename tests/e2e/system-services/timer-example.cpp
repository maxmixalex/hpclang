#include <hpclang/stdio.h>
#include <hpclang/time.h>
#include <cerrno>
#include <cstdio>
#include <cstring>

int hpclang_timer_example_main();

namespace {
struct capture
{
    char bytes[128]{};
    unsigned used = 0;
};

int write_byte(void *context, unsigned stream, unsigned char byte)
{
    auto &output = *static_cast<capture *>(context);
    if (stream != 1 || output.used == sizeof output.bytes) {
        errno = EIO;
        return -1;
    }
    output.bytes[output.used++] = static_cast<char>(byte);
    return 0;
}
} // namespace

extern "C" unsigned hpclang_e2e_timer_example_test(void)
{
    capture output;
    hpclang_stdio_callbacks callbacks{write_byte, nullptr, nullptr, &output};
    uint64_t before, after;
    if (hpclang_time_read_us(&before) || hpclang_stdio_bind(&callbacks))
        return 1;
    int status = hpclang_timer_example_main();
    int flush = std::fflush(stdout);
    // Always detach the borrowed context before returning, even on failure.
    int unbind = hpclang_stdio_unbind();
    if (status || flush || unbind || hpclang_time_read_us(&after))
        return 2;
    if (after < before || after - before < 5000000)
        return 3;
    unsigned previous = 0;
    constexpr char suffix[] = " / 5 timer ticks\n";
    constexpr unsigned line_bytes = sizeof suffix;
    for (unsigned offset = 0; offset < output.used; offset += line_bytes) {
        if (output.used - offset < line_bytes || output.bytes[offset] <= '0' ||
            output.bytes[offset] > '5' ||
            std::memcmp(output.bytes + offset + 1, suffix, sizeof suffix - 1))
            return 4;
        unsigned current = static_cast<unsigned>(output.bytes[offset] - '0');
        // Delayed polling may coalesce ticks, but progress must be monotonic.
        if (current <= previous)
            return 5;
        previous = current;
    }
    return previous == 5 ? 0 : 6;
}
