#ifndef HPCLANG_STORAGE_HPP
#define HPCLANG_STORAGE_HPP

#include <hpclang/storage.h>
#include <cerrno>
#include <cstdio>
#include <utility>

namespace hpclang {
namespace detail {

template <typename Resource, int (*Close)(Resource *)> class storage_owner {
  public:
    storage_owner() noexcept = default;
    explicit storage_owner(Resource *resource) noexcept : resource_(resource)
    {
    }
    storage_owner(const storage_owner &) = delete;
    storage_owner &operator=(const storage_owner &) = delete;
    storage_owner(storage_owner &&other) noexcept : resource_(other.release())
    {
    }
    storage_owner &operator=(storage_owner &&other) noexcept
    {
        if (this != &other) {
            reset();
            resource_ = other.release();
        }
        return *this;
    }
    ~storage_owner()
    {
        reset();
    }

    [[nodiscard]] Resource *get() const noexcept
    {
        return resource_;
    }
    explicit operator bool() const noexcept
    {
        return resource_ != nullptr;
    }
    [[nodiscard]] Resource *release() noexcept
    {
        return std::exchange(resource_, nullptr);
    }
    void swap(storage_owner &other) noexcept
    {
        std::swap(resource_, other.resource_);
    }

    /* Closing consumes the handle even on error. Empty close succeeds without
     * changing errno. Check this result before claiming a write was saved. */
    [[nodiscard]] int close() noexcept
    {
        auto *resource = release();
        return resource ? Close(resource) : 0;
    }
    /* Best-effort cleanup discards close errors and preserves errno. For a
     * writable stream, explicitly close/check before reset or move assignment. */
    void reset(Resource *replacement = nullptr) noexcept
    {
        if (resource_ == replacement)
            return;
        int error = errno;
        (void)close();
        resource_ = replacement;
        errno = error;
    }

  private:
    Resource *resource_ = nullptr;
};

} // namespace detail

/* Allocation-free C++17 owners; adoption transfers exclusive close duty.
 * get() borrows until close/reset/move-assignment/destruction. Release before
 * handing close duty back to C. Do not adopt stdin/stdout/stderr or another
 * owner's get(). Successful C opens may be adopted; a failed open can be kept
 * in a temporary owner so the existing one remains unchanged.
 * C availability, media invalidation and filename encoding remain unchanged.
 * exit()/_Exit() do not unwind automatic C++ owners: use ordinary scope exit
 * or explicit checked close. Runtime shutdown is separate from this lifetime. */
using owned_stream = detail::storage_owner<::FILE, ::fclose>;
using owned_stream_directory =
    detail::storage_owner<::hpclang_stream_directory, ::hpclang_stream_directory_close>;

} // namespace hpclang
#endif
