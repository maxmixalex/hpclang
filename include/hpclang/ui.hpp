#ifndef HPCLANG_UI_HPP
#define HPCLANG_UI_HPP

#include <cerrno>
#include <hpclang/ui.h>

namespace hpclang::ui {

/* Borrows a context which must outlive this guard. Do not begin/abort/commit
 * the same declaration through the C API while a guard owns it. No allocation,
 * exceptions or implicit commit: an early return aborts the candidate. */
class declaration {
  public:
    explicit declaration(hpclang_ui *context) noexcept
        : context_(::hpclang_ui_begin(context) == 0 ? context : nullptr)
    {
    }
    declaration(const declaration &) = delete;
    declaration &operator=(const declaration &) = delete;
    ~declaration()
    {
        if (context_)
            ::hpclang_ui_abort(context_);
    }
    explicit operator bool() const noexcept
    {
        return context_ != nullptr;
    }
    [[nodiscard]] int commit() noexcept
    {
        if (!context_) {
            errno = EINVAL;
            return -1;
        }
        auto *context = context_;
        context_ = nullptr;
        return ::hpclang_ui_commit(context);
    }

  private:
    hpclang_ui *context_;
};

/* Lexical container guard. Close all scopes before committing the declaration.
 * A failed scope is false; the C core also records its error on the candidate.
 * Guards are intentionally nonmovable, preserving lexical nesting order. */
class scope {
  public:
    scope(hpclang_ui *context, uint32_t key, hpclang_ui_kind kind, uint16_t style = 0) noexcept
        : scope_(::hpclang_ui_scope_begin(context, key, kind, style))
    {
    }
    scope(const scope &) = delete;
    scope &operator=(const scope &) = delete;
    ~scope()
    {
        close();
    }
    explicit operator bool() const noexcept
    {
        return scope_.open != 0;
    }
    void close() noexcept
    {
        ::hpclang_ui_scope_close(&scope_);
    }

  private:
    hpclang_ui_scope scope_;
};

} // namespace hpclang::ui
#endif
