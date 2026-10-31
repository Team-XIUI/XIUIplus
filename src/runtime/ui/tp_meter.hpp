#pragma once

#include "design_system.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace xiui::ui
{
    struct tp_meter_colors
    {
        bool custom_overlay{};
        D3DCOLOR overlay_top{0xFF4D7480}, overlay_bottom{0xFF83CDEC}, flash{0xFFFFFFFF};
    };

    inline auto Xiv1TPMeterFill(const float ratio, const D3DCOLOR top, const D3DCOLOR bottom, const double seconds, const bool blink) -> meter_fill_layer
    {
        const float progress = std::clamp(ratio, 0.0f, 1.0f);
        const float pulse = blink && progress >= 1.0f / 3 ? .1f * static_cast<float>(.5 - .5 * std::cos(seconds * 6.28318530718)) : 0;
        const auto brighten = [pulse](const D3DCOLOR color) {
            D3DCOLOR result = color & 0xFF000000;
            for (unsigned shift = 0; shift < 24; shift += 8)
            {
                const auto channel = (color >> shift) & 255;
                result |= static_cast<D3DCOLOR>(channel + (255 - channel) * pulse) << shift;
            }
            return result;
        };
        return {progress, brighten(top), brighten(bottom)};
    }

    inline auto TPMeterLayers(const float ratio, const D3DCOLOR top, const D3DCOLOR bottom, const double seconds,
        const bool blink, const tp_meter_colors* colors = nullptr) -> std::array<meter_fill_layer, 3>
    {
        const float progress = std::clamp(ratio, 0.0f, 1.0f) * 3;
        const auto flash = colors != nullptr ? colors->flash : 0xFFFFFFFF;
        const float pulse = blink && progress >= 1 ? 0.14f * static_cast<float>(0.5 - 0.5 * std::cos(seconds * 6.28318530718)) *
            static_cast<float>(flash >> 24) / 255 : 0;
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
            const bool custom = colors != nullptr && colors->custom_overlay && index > 0;
            const float overlayTint = index == 2 ? 0.18f : 0;
            const auto upper = custom ? mix(colors->overlay_top, tints[index], overlayTint) : mix(top, tints[index], tint);
            const auto lower = custom ? mix(colors->overlay_bottom, tints[index], overlayTint) : mix(bottom, tints[index], tint);
            layers[index] = {std::clamp(progress - stage, 0.0f, 1.0f),
                mix(upper, flash, pulse), mix(lower, flash, pulse)};
        }
        return layers;
    }
}
