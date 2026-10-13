#include <hpclang/display.hpp>
#include <cassert>
#include <type_traits>
#include <utility>

static_assert(!std::is_copy_constructible_v<hpclang::owned_surface>);
static_assert(std::is_nothrow_move_constructible_v<hpclang::owned_surface>);
static_assert(std::is_nothrow_move_assignable_v<hpclang::owned_surface>);

int main()
{
    hpclang::owned_surface invalid{0, 8};
    assert(!invalid);
    hpclang::owned_surface a{8, 8, 16};
    assert(a && a.view().levels() == 16);
    a.view().set_pixel(2, 3, 255);
    auto *original = a.get();
    hpclang::owned_surface b{std::move(a)};
    assert(!a && b.get() == original && b.view().get_pixel(2, 3) == 255);
    hpclang::owned_surface c{4, 4};
    assert(c);
    c = std::move(b);
    assert(!b && c.get() == original && c.view().width() == 8);
    auto &alias = c;
    c = std::move(alias);
    assert(c.get() == original);
    auto *released = c.release();
    assert(!c && released == original);
    hpclang_surface_destroy(released);
    c.reset();
    c = hpclang::owned_surface{3, 2, 4};
    assert(c && c.view().height() == 2);
    c.reset();
    assert(!c);
    hpclang::owned_surface automatic_cleanup{2, 2};
    assert(automatic_cleanup);
}
