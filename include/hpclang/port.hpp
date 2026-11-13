#ifndef HPCLANG_PORT_HPP
#define HPCLANG_PORT_HPP

#include <hpclang/port.h>
#include <cerrno>
#include <memory>

namespace hpclang {
namespace detail {
struct port_iterator_deleter
{
    void operator()(::hpclang_port_iterator *iterator) const noexcept
    {
        int error = errno;
        ::hpclang_port_iterator_close(iterator);
        errno = error;
    }
};
} // namespace detail

/* Adopt an independently owned C metadata snapshot. This allocation-free owner
 * uses unique_ptr move/get/reset/release/swap semantics; never adopt get().
 * Opening retains the C service's context checks and Port 2 recovery behavior.
 * Raw-byte names and library identifiers are unchanged. Destruction preserves
 * errno. exit()/_Exit() do not unwind automatic owners: close before calling.
 * To preserve an existing iterator on open failure, open a candidate and swap
 * only after success; reset(open(...)) deliberately replaces even on failure. */
using owned_port_iterator = std::unique_ptr<::hpclang_port_iterator, detail::port_iterator_deleter>;
} // namespace hpclang

#endif
