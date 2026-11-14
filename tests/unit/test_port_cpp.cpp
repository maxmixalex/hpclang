#include <hpclang/port.hpp>
#include <cassert>
#include <cstring>
#include <type_traits>
#include <utility>

static_assert(!std::is_copy_constructible_v<hpclang::owned_port_iterator>);
static_assert(!std::is_copy_assignable_v<hpclang::owned_port_iterator>);
static_assert(std::is_nothrow_move_constructible_v<hpclang::owned_port_iterator>);
static_assert(std::is_nothrow_move_assignable_v<hpclang::owned_port_iterator>);

// The C fixture supplies PORTZERO; invalidation removes the original mapping.
extern "C" void test_port_cpp(void (*invalidate)())
{
    hpclang::owned_port_iterator first{hpclang_port_iterator_open(0)};
    assert(first);
    auto *original = first.get();
    hpclang::owned_port_iterator second{std::move(first)};
    assert(!first && second.get() == original);
    first.reset(hpclang_port_iterator_open(0));
    assert(first && first.get() != original);
    first = std::move(second);
    assert(!second && first.get() == original);
    {
        hpclang::owned_port_iterator candidate{hpclang_port_iterator_open(3)};
        assert(!candidate && errno == EINVAL);
        if (candidate)
            first.swap(candidate);
    }
    assert(first.get() == original && errno == EINVAL);
    invalidate();
    hpclang_port_entry entry{};
    entry.size = sizeof entry;
    entry.version = HPCLANG_PORT_ENTRY_VERSION;
    assert(hpclang_port_iterator_read(first.get(), &entry) == 1);
    assert(entry.kind == HPCLANG_PORT_BACKUP && entry.name_length == 8);
    assert(std::memcmp(entry.name, "PORTZERO", 8) == 0);
    assert(hpclang_port_iterator_read(first.get(), &entry) == 0);
    auto *released = first.release();
    assert(!first && released == original);
    errno = EDOM;
    {
        hpclang::owned_port_iterator adopted{released};
    }
    assert(errno == EDOM);
    first.reset();
    assert(errno == EDOM);
}
