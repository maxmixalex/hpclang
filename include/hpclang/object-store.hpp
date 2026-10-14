#ifndef HPCLANG_OBJECT_STORE_HPP
#define HPCLANG_OBJECT_STORE_HPP

#include <hpclang/object-store.h>
#include <cerrno>
#include <memory>

namespace hpclang {
namespace detail {

struct object_store_deleter
{
    void operator()(::hpclang_object_store *store) const noexcept
    {
        int error = errno;
        ::hpclang_object_store_close(store);
        errno = error;
    }
};

struct object_store_iterator_deleter
{
    void operator()(::hpclang_object_store_iterator *iterator) const noexcept
    {
        int error = errno;
        ::hpclang_object_store_iterator_close(iterator);
        errno = error;
    }
};

} // namespace detail

/* Adopt independently owned C results. These allocation-free owners use normal
 * unique_ptr move/get/reset/release/swap semantics. Never adopt a borrowed
 * pointer, including your own get() result. C service availability, raw-byte
 * names and logical path semantics are unchanged. */
using owned_object_store = std::unique_ptr<::hpclang_object_store, detail::object_store_deleter>;

/* The C iterator owns an independent metadata snapshot; it can outlive the
 * store handle used to open it. Destruction closes it without changing errno.
 * Neither owner unwinds on exit()/_Exit(); use scopes or reset before those. */
using owned_object_store_iterator =
    std::unique_ptr<::hpclang_object_store_iterator, detail::object_store_iterator_deleter>;

} // namespace hpclang

#endif
