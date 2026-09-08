#include <array>
#include <concepts>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

volatile int hpclang_cxx_ctor_probe;

struct Probe {
    Probe() { hpclang_cxx_ctor_probe = 42; }
};

static Probe probe;

template <typename T>
concept Addable = requires(T a, T b) {
    { a + b } -> std::convertible_to<T>;
};

consteval int compile_time_answer()
{
    return 42;
}

template <Addable T>
constexpr T add(T a, T b)
{
    return a + b;
}

int main()
{
    static_assert(__cplusplus > 202002L);
    static_assert(compile_time_answer() == 42);
    static_assert(add(20, 22) == 42);

    constexpr std::array<int, 4> expected{10, 20, 30, 40};
    std::span<const int> view(expected);

    std::string value = "hp";
    value += "clang";

    std::vector<int> dynamic{10, 20, 30, 40};
    std::optional<int> answer = add(dynamic[1], 22);
    std::string_view name(value);

    auto sum = [](std::span<const int> values) {
        int total = 0;
        for (int item : values)
            total += item;
        return total;
    };

    return hpclang_cxx_ctor_probe == 42 &&
                   name == "hpclang" &&
                   answer.value_or(0) == 42 &&
                   sum(view) == 100 &&
                   dynamic.size() == 4
               ? 0
               : 1;
}
