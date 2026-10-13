#include <hpclang/object.hpp>
#include <cassert>
#include <cstdint>
#include <type_traits>
#include <utility>

static_assert(!std::is_copy_constructible_v<hpclang::owned_object>);
static_assert(!std::is_copy_assignable_v<hpclang::owned_object>);
static_assert(std::is_nothrow_move_constructible_v<hpclang::owned_object>);
static_assert(std::is_nothrow_move_assignable_v<hpclang::owned_object>);

static std::int64_t integer(const hpclang::owned_object &object)
{
    std::int64_t value = 0;
    assert(hpclang_object_integer_i64(object.get(), &value) == 0);
    return value;
}

int main()
{
    hpclang::owned_object a;
    assert(!a);
    assert(a.receive([](auto **out) { return hpclang_object_create_integer_i64(42, out); }) == 0);
    auto *original = a.get();
    hpclang::owned_object b{std::move(a)};
    assert(!a && b.get() == original && integer(b) == 42);
    b.reset(b.get());
    assert(integer(b) == 42);
    auto &alias = b;
    b = std::move(alias);
    assert(integer(b) == 42);
    a.reset();
    assert(a.receive([](auto **out) { return hpclang_object_create_integer_i64(-7, out); }) == 0);
    a = std::move(b);
    assert(!b && a.get() == original);

    // A real producer failure must preserve the previous owned snapshot.
    errno = 0;
    assert(a.receive([](auto **out) { return hpclang_object_create_string(nullptr, 1, out); }) ==
           -1);
    assert(errno == EFAULT && a.get() == original && integer(a) == 42);
    // A callback that fails after producing a temporary must not leak it.
    assert(a.receive([](auto **out) {
        assert(hpclang_object_create_integer_i64(99, out) == 0);
        errno = EIO;
        return -1;
    }) == -1);
    assert(errno == EIO && integer(a) == 42);
    // Read the old object while constructing its replacement.
    assert(a.receive([&](auto **out) {
        return hpclang_object_create_integer_i64(integer(a) + 1, out);
    }) == 0);
    assert(integer(a) == 43);
    auto *released = a.release();
    assert(!a);
    hpclang::owned_object adopted{released};
    assert(integer(adopted) == 43);
    adopted.swap(a);
    assert(!adopted && integer(a) == 43);
    a.reset();
    a.reset();
    assert(!a);
    assert(adopted.receive(
               [](auto **out) { return hpclang_object_create_integer_i64(INT64_MIN, out); }) == 0);
    assert(integer(adopted) == INT64_MIN);
}
