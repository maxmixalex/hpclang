#ifndef HPCLANG_DISPLAY_HPP
#define HPCLANG_DISPLAY_HPP

#include <cstdint>
#include <span>
#include <utility>

#include <hpclang/display.h>

namespace hpclang {

class surface_ref {
  public:
    explicit surface_ref(hpclang_surface *surface) noexcept : surface_(surface)
    {
    }

    [[nodiscard]] std::uint32_t width() const noexcept
    {
        return ::hpclang_surface_width(surface_);
    }

    [[nodiscard]] std::uint32_t height() const noexcept
    {
        return ::hpclang_surface_height(surface_);
    }

    [[nodiscard]] std::uint32_t levels() const noexcept
    {
        return ::hpclang_surface_levels(surface_);
    }

    void clear(std::uint8_t intensity = 0u) const noexcept
    {
        ::hpclang_surface_clear(surface_, intensity);
    }

    [[nodiscard]] std::uint8_t get_pixel(std::int32_t x, std::int32_t y) const noexcept
    {
        return ::hpclang_surface_get_pixel(surface_, x, y);
    }

    void set_pixel(std::int32_t x, std::int32_t y, std::uint8_t intensity) const noexcept
    {
        ::hpclang_surface_set_pixel(surface_, x, y, intensity);
    }

    void draw_line(std::int32_t x0, std::int32_t y0, std::int32_t x1, std::int32_t y1,
                   std::uint8_t intensity) const noexcept
    {
        ::hpclang_surface_draw_line(surface_, x0, y0, x1, y1, intensity);
    }

    void draw_rect(std::int32_t x, std::int32_t y, std::int32_t width, std::int32_t height,
                   std::uint8_t intensity) const noexcept
    {
        ::hpclang_surface_draw_rect(surface_, x, y, width, height, intensity);
    }

    void draw_circle(std::int32_t center_x, std::int32_t center_y, std::int32_t radius,
                     std::uint8_t intensity) const noexcept
    {
        ::hpclang_surface_draw_circle(surface_, center_x, center_y, radius, intensity);
    }

    void fill_circle(std::int32_t center_x, std::int32_t center_y, std::int32_t radius,
                     std::uint8_t intensity) const noexcept
    {
        ::hpclang_surface_fill_circle(surface_, center_x, center_y, radius, intensity);
    }

    void draw_polygon(std::span<const ::hpclang_point> points,
                      std::uint8_t intensity) const noexcept
    {
        ::hpclang_surface_draw_polygon(surface_, points.data(),
                                       static_cast<std::uint32_t>(points.size()), intensity);
    }

    void fill_polygon(std::span<const ::hpclang_point> points,
                      std::uint8_t intensity) const noexcept
    {
        ::hpclang_surface_fill_polygon(surface_, points.data(),
                                       static_cast<std::uint32_t>(points.size()), intensity);
    }

    void blit_from(surface_ref source, std::int32_t destination_x, std::int32_t destination_y,
                   std::int32_t source_x, std::int32_t source_y, std::int32_t width,
                   std::int32_t height) const noexcept
    {
        ::hpclang_surface_blit(surface_, destination_x, destination_y, source.surface_, source_x,
                               source_y, width, height);
    }

    void fill_rect(std::int32_t x, std::int32_t y, std::int32_t width, std::int32_t height,
                   std::uint8_t intensity) const noexcept
    {
        ::hpclang_surface_fill_rect(surface_, x, y, width, height, intensity);
    }

  private:
    hpclang_surface *surface_;
};

/* Owns an offscreen image. LCD pages remain borrowed surface_ref objects.
 * Construction failure is an empty owner, following the C constructor contract.
 * Views and get() borrow the image until reset/destruction/move-assignment. */
class owned_surface {
  public:
    owned_surface() noexcept = default;
    owned_surface(std::uint32_t width, std::uint32_t height, std::uint32_t levels = 2) noexcept
        : surface_(::hpclang_surface_create(width, height, levels))
    {
    }
    owned_surface(const owned_surface &) = delete;
    owned_surface &operator=(const owned_surface &) = delete;
    owned_surface(owned_surface &&other) noexcept : surface_(std::exchange(other.surface_, nullptr))
    {
    }
    owned_surface &operator=(owned_surface &&other) noexcept
    {
        if (this != &other) {
            reset();
            surface_ = std::exchange(other.surface_, nullptr);
        }
        return *this;
    }
    ~owned_surface()
    {
        reset();
    }
    explicit operator bool() const noexcept
    {
        return surface_ != nullptr;
    }
    [[nodiscard]] hpclang_surface *get() const noexcept
    {
        return surface_;
    }
    [[nodiscard]] surface_ref view() const noexcept
    {
        return surface_ref{surface_};
    }
    void reset() noexcept
    {
        ::hpclang_surface_destroy(std::exchange(surface_, nullptr));
    }
    /* Transfer destruction responsibility to a C owner. */
    [[nodiscard]] hpclang_surface *release() noexcept
    {
        return std::exchange(surface_, nullptr);
    }

  private:
    hpclang_surface *surface_ = nullptr;
};

[[nodiscard]] inline surface_ref display_surface() noexcept
{
    return surface_ref{::hpclang_display_surface()};
}

} // namespace hpclang

#endif
