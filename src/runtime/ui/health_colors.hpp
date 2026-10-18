#pragma once

#include <d3d8.h>
#include <cstdint>
#include <algorithm>
#include <cmath>

namespace xiui::ui
{
    constexpr auto HPValueColor(const uint32_t percent) -> D3DCOLOR
    {
        if (percent < 25) return 0xFFFF0000;
        if (percent < 50) return 0xFFFFA500;
        if (percent < 75) return 0xFFFFFF00;
        return 0xFFFFFFFF;
    }

    inline auto HPWarningFill(const D3DCOLOR color, const float ratio) -> D3DCOLOR
    {
        if (!std::isfinite(ratio) || ratio >= 0.75f) return color;
        const auto tint = ratio < 0.25f ? HPValueColor(0) : ratio < 0.5f ? HPValueColor(25) : HPValueColor(50);
        const uint32_t red = (color >> 16) & 255, green = (color >> 8) & 255, blue = color & 255;
        const float sum = static_cast<float>(std::max({red, green, blue}) + std::min({red, green, blue})) * 0.85f;
        // Keep gradient contrast and alpha while introducing a warning hue.
        const float chroma = 255 - std::abs(sum - 255), base = (sum - chroma) / 2;
        auto result = color & 0xFF000000;
        for (const unsigned shift : {0u, 8u, 16u})
            result |= static_cast<uint32_t>(std::clamp(std::round(base + chroma * ((tint >> shift) & 255) / 255), 0.0f, 255.0f)) << shift;
        return result;
    }
}
