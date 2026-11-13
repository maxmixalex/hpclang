#include <hpclang/ui-surface.hpp>
#include <cassert>
#include <type_traits>
#include <utility>

struct hpclang_ui_surface
{
    unsigned destroys;
};
extern "C" void hpclang_ui_surface_destroy(hpclang_ui_surface *renderer)
{
    assert(renderer && renderer->destroys++ == 0);
    errno = EIO; // Cleanup must not hide the operation that failed.
}
static_assert(!std::is_copy_constructible_v<hpclang::owned_ui_surface>);
static_assert(std::is_nothrow_move_constructible_v<hpclang::owned_ui_surface>);
static_assert(std::is_nothrow_move_assignable_v<hpclang::owned_ui_surface>);

static void early_return(hpclang_ui_surface *renderer)
{
    hpclang::owned_ui_surface owner{renderer};
    errno = ENOSPC;
}

int main()
{
    hpclang_ui_surface first{}, second{}, third{};
    {
        hpclang::owned_ui_surface a{&first};
        hpclang::owned_ui_surface b{std::move(a)};
        assert(!a && b.get() == &first && !first.destroys);
        hpclang::owned_ui_surface c{&second};
        errno = ENOMEM;
        c = std::move(b);
        assert(!b && c.get() == &first && second.destroys == 1 && errno == ENOMEM);
        hpclang::owned_ui_surface failed;
        if (failed)
            c.swap(failed);
        assert(c.get() == &first && !first.destroys);
        auto *released = c.release();
        assert(!c && released == &first && !first.destroys);
        a.reset(released);
        errno = EINVAL;
    }
    assert(first.destroys == 1 && second.destroys == 1 && errno == EINVAL);
    early_return(&third);
    assert(third.destroys == 1 && errno == ENOSPC);
}
