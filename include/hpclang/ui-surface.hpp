#ifndef HPCLANG_UI_SURFACE_HPP
#define HPCLANG_UI_SURFACE_HPP

#include <hpclang/ui-surface.h>
#include <cerrno>
#include <memory>

namespace hpclang {
namespace detail {
struct ui_surface_deleter
{
    void operator()(::hpclang_ui_surface *renderer) const noexcept
    {
        int error = errno;
        ::hpclang_ui_surface_destroy(renderer);
        errno = error;
    }
};
} // namespace detail

/* Adopt a successful C renderer creation; no extra allocation is performed.
 * Styles are copied by C, but fonts remain borrowed and must outlive the owner.
 * get() borrows; release() transfers destruction responsibility back to C.
 * Never adopt another owner's get() or reset to your own get(). Do not destroy,
 * reset or replace this owner inside its measure/render/canvas callbacks.
 * The UI context and display session remain independently owned. Normal scope
 * exit destroys the renderer and preserves errno; exit()/_Exit() do not unwind
 * automatic owners. To preserve a renderer on creation failure, create a
 * candidate owner, check it, then swap only after success. */
using owned_ui_surface = std::unique_ptr<::hpclang_ui_surface, detail::ui_surface_deleter>;

} // namespace hpclang
#endif
