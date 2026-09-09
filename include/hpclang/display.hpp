#ifndef HPCLANG_DISPLAY_HPP
#define HPCLANG_DISPLAY_HPP

#include <cstdint>

#include <hpclang/display.h>

namespace hpclang {

class surface_ref {
public:
    explicit surface_ref(hpclang_surface *surface) noexcept
        : surface_(surface)
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

    [[nodiscard]] std::uint8_t get_pixel(
        std::int32_t x,
        std::int32_t y) const noexcept
    {
        return ::hpclang_surface_get_pixel(surface_, x, y);
    }

    void set_pixel(
        std::int32_t x,
        std::int32_t y,
        std::uint8_t intensity) const noexcept
    {
        ::hpclang_surface_set_pixel(surface_, x, y, intensity);
    }

    void draw_line(
        std::int32_t x0,
        std::int32_t y0,
        std::int32_t x1,
        std::int32_t y1,
        std::uint8_t intensity) const noexcept
    {
        ::hpclang_surface_draw_line(
            surface_, x0, y0, x1, y1, intensity);
    }

    void draw_rect(
        std::int32_t x,
        std::int32_t y,
        std::int32_t width,
        std::int32_t height,
        std::uint8_t intensity) const noexcept
    {
        ::hpclang_surface_draw_rect(
            surface_, x, y, width, height, intensity);
    }

    void blit_from(
        surface_ref source,
        std::int32_t destination_x,
        std::int32_t destination_y,
        std::int32_t source_x,
        std::int32_t source_y,
        std::int32_t width,
        std::int32_t height) const noexcept
    {
        ::hpclang_surface_blit(
            surface_, destination_x, destination_y,
            source.surface_, source_x, source_y, width, height);
    }

    void fill_rect(
        std::int32_t x,
        std::int32_t y,
        std::int32_t width,
        std::int32_t height,
        std::uint8_t intensity) const noexcept
    {
        ::hpclang_surface_fill_rect(
            surface_, x, y, width, height, intensity);
    }

private:
    hpclang_surface *surface_;
};

[[nodiscard]] inline surface_ref display_surface() noexcept
{
    return surface_ref{::hpclang_display_surface()};
}

} // namespace hpclang

#endif
