#include <hpclang/object.hpp>
#include <cstdint>

int main()
{
    hpclang::owned_object value;
    if (value.receive([](auto **out) { return hpclang_object_create_integer_i64(42, out); }) != 0)
        return 1;

    // The previous snapshot remains alive while the producer reads it.
    if (value.receive([&](auto **out) {
            std::int64_t previous = 0;
            if (hpclang_object_integer_i64(value.get(), &previous) != 0)
                return -1;
            return hpclang_object_create_integer_i64(previous + 1, out);
        }) != 0)
        return 2;

    std::int64_t result = 0;
    return hpclang_object_integer_i64(value.get(), &result) != 0 || result != 43;
}
