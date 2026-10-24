#include <hpclang/storage.hpp>
#include <cassert>
#include <cstring>
#include <type_traits>

static_assert(!std::is_copy_constructible_v<hpclang::owned_stream>);
static_assert(!std::is_copy_assignable_v<hpclang::owned_stream_directory>);
static_assert(std::is_nothrow_move_constructible_v<hpclang::owned_stream>);
static_assert(std::is_nothrow_move_assignable_v<hpclang::owned_stream_directory>);

/* Uses the real FatFs/namespace fixture, including independent directory
 * cursors and stale-media close errors, not a mocked wrapper implementation. */
extern "C" void test_stream_directory_cpp()
{
    hpclang::owned_stream_directory root{hpclang_stream_directory_open("/")};
    assert(root);
    auto *original = root.get();
    hpclang::owned_stream_directory moved{std::move(root)};
    assert(!root && moved.get() == original);
    root.reset(hpclang_stream_directory_open("/"));
    assert(root && root.get() != original);
    errno = EDOM;
    root = std::move(moved);
    assert(!moved && root.get() == original && errno == EDOM);
    auto &alias = root;
    root = std::move(alias);
    root.reset(root.get());
    assert(root.get() == original);
    hpclang::owned_stream_directory missing{hpclang_stream_directory_open("/MISSING")};
    assert(!missing && errno == ENOENT);
    if (missing)
        root.swap(missing);
    assert(root.get() == original && errno == ENOENT);
    hpclang_stream_directory_entry entry{};
    assert(hpclang_stream_directory_read(root.get(), &entry) == 1);
    assert(!std::strcmp(entry.name, "STAMP.TXT"));
    auto *released = root.release();
    assert(!root && released == original);
    hpclang::owned_stream_directory adopted{released};
    assert(adopted.close() == 0 && !adopted);
    errno = EIO;
    assert(adopted.close() == 0 && errno == EIO);
    {
        hpclang::owned_stream_directory scoped{hpclang_stream_directory_open("/")};
        assert(scoped);
        errno = EDOM;
    }
    assert(errno == EDOM);
}

extern "C" int test_stream_directory_stale_cpp(hpclang_stream_directory *stale)
{
    hpclang::owned_stream_directory owner{stale};
    int result = owner.close();
    assert(result == -1 && errno == EBADF && !owner);
    assert(owner.close() == 0 && errno == EBADF);
    return result;
}
