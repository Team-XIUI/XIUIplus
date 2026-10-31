#pragma once

#include "design_system.hpp"

#include <algorithm>

namespace xiui::ui
{
    struct meter_texture
    {
        IDirect3DTexture8* texture{};
        rect uv{};
        texture_style style{};
        float texture_width{1}, texture_height{1};
    };

    struct meter_skin
    {
        meter_texture fill{}, background{};
        float cap_ratio{0.5f}, cap_gap{1.0f};
        bool xiv1{};
        meter_texture left_cap{}, right_cap{}, rim{};
    };

    struct meter_geometry
    {
        std::array<rect, 3> frame{};
        rect content{};
    };

    inline auto LayoutMeterTrack(const rect& bounds, const meter_style& style) -> rect
    {
        const float height = style.height > 0 ? std::clamp(style.height, 1.0f, 80.0f) : std::max(1.0f, bounds.height - 2);
        return {bounds.x, bounds.y + (bounds.height - height) * 0.5f, bounds.width, height};
    }

    inline auto LayoutMeterFill(const rect& content, const float ratio, const bool fromRight = false) -> rect
    {
        const float width = content.width * std::clamp(ratio, 0.0f, 1.0f);
        return {content.x + (fromRight ? content.width - width : 0), content.y, width, content.height};
    }

    inline auto LayoutMeterRange(const rect& content, const meter_range& range, const float fillRatio, const bool fromRight = false) -> rect
    {
        const float end = std::clamp(range.end, 0.0f, (range.beyond_fill ? 1.0f : std::clamp(fillRatio, 0.0f, 1.0f)));
        const float start = std::clamp(range.start, 0.0f, end);
        return {content.x + content.width * (fromRight ? 1 - end : start), content.y, content.width * (end - start), content.height};
    }

    inline auto LayoutMeterStrip(const rect& content, const float height, const float inset) -> rect
    {
        const float top = std::clamp(inset, 0.0f, std::max(0.0f, content.height - 1));
        return {content.x, content.y + top, content.width, std::min(std::max(1.0f, height), std::max(0.0f, content.height - top))};
    }

    inline auto LayoutMeterSkin(const rect& bounds, const meter_skin& skin, bool bookends = true) -> meter_geometry
    {
        const float width = std::max(0.0f, bounds.width), height = std::max(0.0f, bounds.height);
        const float cap = bookends ? std::clamp(height * skin.cap_ratio, 0.0f, width * 0.5f) : 0;
        const float gap = bookends ? std::clamp(skin.cap_gap, 0.0f, (width - cap * 2) * 0.5f) : 0;
        const float bodyWidth = std::max(0.0f, width - (cap + gap) * 2);
        const float inset = skin.xiv1 ? std::min(height * (3.0f / 14), std::max(0.0f, (height - 1) * .5f)) : 0;
        return {{{{bounds.x, bounds.y, cap, height}, {bounds.x + cap + gap, bounds.y, bodyWidth, height},
            {bounds.x + width - cap, bounds.y, cap, height}}},
            {bounds.x + cap + gap, bounds.y + inset, bodyWidth, std::max(0.0f, height - inset * 2)}};
    }

    inline auto DrawXiv1MeterFrame(const design_system& design, const rect& bounds, const meter_skin& skin, const bool bookends, const float opacity) -> void
    {
        const auto geometry = LayoutMeterSkin(bounds, skin, bookends);
        const auto draw = [&](const meter_texture& image, const rect& target) {
            auto paint = image.style;
            for (auto& color : paint.colors)
                color = (color & 0xFFFFFF) | (static_cast<uint32_t>((color >> 24) * std::clamp(opacity, 0.0f, 1.0f)) << 24);
            design.DrawTexture(image.texture, target, image.uv, paint);
        };
        draw(skin.rim, geometry.frame[1]);
        if (bookends)
        {
            draw(skin.left_cap, geometry.frame[0]);
            draw(skin.right_cap, geometry.frame[2]);
        }
    }
}
