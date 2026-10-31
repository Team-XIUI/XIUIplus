#pragma once

#include <algorithm>
#include <cmath>

namespace xiui::ui
{
    inline constexpr float text_base_pixels = 15;
    inline constexpr float text_min_scale = 8 / text_base_pixels, text_max_scale = 38 / text_base_pixels;
    inline auto TextPixelSize(const float scale) -> int
    { return std::clamp(static_cast<int>(std::lround(std::isfinite(scale) ? scale * text_base_pixels : text_base_pixels)), 6, 96); }
    inline auto TextPixelScale(const float scale) -> float { return TextPixelSize(scale) / text_base_pixels; }
    inline constexpr int text_max_raster_pixels = 96 * 6;
    inline auto ScaledTextPixelSize(const float scale, const float hudScale) -> int
    { return std::clamp(static_cast<int>(std::lround(TextPixelSize(scale) * hudScale)), 1, text_max_raster_pixels); }
}
