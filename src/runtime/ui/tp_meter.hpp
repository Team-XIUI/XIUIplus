#pragma once

#include "design_system.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace xiui::ui
{
    inline auto TPMeterLayers(const float ratio, const D3DCOLOR top, const D3DCOLOR bottom, const double seconds,
        const bool blink) -> std::array<meter_fill_layer, 3>
    {
        const float progress = std::clamp(ratio, 0.0f, 1.0f) * 3;
        const float pulse = blink && progress >= 1 ? 0.14f * static_cast<float>(0.5 - 0.5 * std::cos(seconds * 6.28318530718)) : 0;
        const auto mix = [](const D3DCOLOR color, const D3DCOLOR tint, const float amount) {
            D3DCOLOR result = color & 0xFF000000;
            for (uint32_t shift = 0; shift < 24; shift += 8)
            {
                const auto channel = (color >> shift) & 0xFF;
                const auto target = static_cast<float>((tint >> shift) & 0xFF);
                result |= static_cast<uint32_t>(std::lround(channel + (target - channel) * amount)) << shift;
            }
            return result;
        };
        std::array<meter_fill_layer, 3> layers{};
        constexpr D3DCOLOR tints[]{0xFFFFFFFF, 0xFFB5F3E2, 0xFFE2D4FF};
        for (size_t index = 0; index < layers.size(); ++index)
        {
            const float stage = static_cast<float>(index), tint = stage * 0.18f;
            layers[index] = {std::clamp(progress - stage, 0.0f, 1.0f),
                mix(mix(top, tints[index], tint), 0xFFFFFFFF, pulse), mix(mix(bottom, tints[index], tint), 0xFFFFFFFF, pulse)};
        }
        return layers;
    }
}
