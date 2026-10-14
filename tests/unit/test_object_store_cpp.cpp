#include <hpclang/object-store.hpp>
#include <hpclang/object.hpp>
#include <cassert>
#include <cstring>
#include <type_traits>
#include <utility>

static_assert(!std::is_copy_constructible_v<hpclang::owned_object_store>);
static_assert(!std::is_copy_assignable_v<hpclang::owned_object_store_iterator>);
static_assert(std::is_nothrow_move_constructible_v<hpclang::owned_object_store>);
static_assert(std::is_nothrow_move_assignable_v<hpclang::owned_object_store_iterator>);

/* Called with the real C service's HOME/A and HOME/SUB/TXT fixture installed. */
extern "C" void test_object_store_cpp()
{
    hpclang::owned_object_store home{hpclang_object_store_home()};
    assert(home);
    auto *original = home.get();
    hpclang::owned_object_store moved{std::move(home)};
    assert(!home && moved.get() == original);
    home.reset(hpclang_object_store_home());
    assert(home && home.get() != original);
    home = std::move(moved); // Closes the previously owned HOME handle.
    assert(!moved && home.get() == original);

    hpclang::owned_object_store child{
        hpclang_object_store_open_child(home.get(), reinterpret_cast<const uint8_t *>("SUB"), 3)};
    assert(child);
    auto *child_before = child.get();
    {
        hpclang::owned_object_store candidate{hpclang_object_store_open_child(
            home.get(), reinterpret_cast<const uint8_t *>("MISSING"), 7)};
        assert(!candidate && errno == ENOENT);
        if (candidate)
            child.swap(candidate);
    }
    assert(child.get() == child_before && errno == ENOENT);
    hpclang::owned_object_store_iterator entries{hpclang_object_store_iterator_open(home.get())};
    assert(entries);
    auto *snapshot = entries.get();
    hpclang::owned_object_store_iterator transferred{std::move(entries)};
    assert(!entries && transferred.get() == snapshot);
    entries.reset(hpclang_object_store_iterator_open(child.get()));
    assert(entries);
    entries = std::move(transferred);
    assert(!transferred && entries.get() == snapshot);
    home.reset();

    // The child is a logical path, not a pointer borrowed from its parent.
    hpclang::owned_object text;
    assert(text.receive([&](auto **out) {
        return hpclang_object_store_get(child.get(), reinterpret_cast<const uint8_t *>("TXT"), 3,
                                        out);
    }) == 0);
    assert(hpclang_object_kind(text.get()) == HPCLANG_OBJECT_STRING);
    child.reset();
    unsigned seen = 0;
    for (;;) {
        hpclang_object_store_entry entry{};
        entry.size = sizeof entry;
        entry.version = HPCLANG_OBJECT_STORE_ENTRY_VERSION;
        int result = hpclang_object_store_iterator_read(entries.get(), &entry);
        assert(result >= 0);
        if (!result)
            break;
        if (entry.name_length == 1 && entry.name[0] == 'A')
            seen |= 1;
        if (entry.name_length == 3 && std::memcmp(entry.name, "SUB", 3) == 0)
            seen |= 2;
    }
    assert(seen == 3);
    auto *released = entries.release();
    assert(!entries && released == snapshot);
    errno = EIO;
    {
        hpclang::owned_object_store_iterator adopted{released};
        hpclang::owned_object_store scoped{hpclang_object_store_home()};
        assert(scoped);
        errno = EIO;
    }
    assert(errno == EIO);
}
