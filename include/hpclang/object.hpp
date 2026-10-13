#ifndef HPCLANG_OBJECT_HPP
#define HPCLANG_OBJECT_HPP

#include <hpclang/object.h>
#include <cerrno>
#include <utility>

namespace hpclang {

/* Exclusive ownership of an independent calculator-object snapshot.
 * get() borrows until reset, successful receive, move-assignment or destruction.
 * No native stack/store ownership is implied. */
class owned_object {
  public:
    owned_object() noexcept = default;
    /* Adopt a snapshot whose ownership has been transferred by a C API. */
    explicit owned_object(::hpclang_object *object) noexcept : object_(object)
    {
    }
    owned_object(const owned_object &) = delete;
    owned_object &operator=(const owned_object &) = delete;
    owned_object(owned_object &&other) noexcept : object_(other.release())
    {
    }
    owned_object &operator=(owned_object &&other) noexcept
    {
        if (this != &other)
            reset(other.release());
        return *this;
    }
    ~owned_object()
    {
        reset();
    }

    ::hpclang_object *get() const noexcept
    {
        return object_;
    }
    explicit operator bool() const noexcept
    {
        return object_ != nullptr;
    }
    ::hpclang_object *release() noexcept
    {
        return std::exchange(object_, nullptr);
    }
    void reset(::hpclang_object *replacement = nullptr) noexcept
    {
        if (object_ != replacement)
            ::hpclang_object_release(std::exchange(object_, replacement));
    }
    void swap(owned_object &other) noexcept
    {
        std::swap(object_, other.object_);
    }

    /* Call a C-style producer with an initially null output. It must return
     * zero and an independently owned snapshot on success, or a nonzero error.
     * Failure preserves this owner and errno; any produced temporary is released.
     * The callback must not throw or transfer this owner's existing snapshot.
     * This also permits reading from get() while building its replacement. */
    template <typename Producer> int receive(Producer &&producer) noexcept
    {
        owned_object candidate;
        int result = std::forward<Producer>(producer)(&candidate.object_);
        int error = errno;
        if (result == 0)
            swap(candidate);
        candidate.reset();
        errno = error;
        return result;
    }

  private:
    ::hpclang_object *object_ = nullptr;
};

} // namespace hpclang

#endif
