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
        meter_texture frame{}, fill{}, background{};
        float cap_ratio{0.5f}, top_inset{0.125f}, bottom_inset{0.125f}, cap_gap{1.0f};
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

    inline auto LayoutMeterSkin(const rect& bounds, const meter_skin& skin, bool bookends = true) -> meter_geometry
    {
        const float width = std::max(0.0f, bounds.width), height = std::max(0.0f, bounds.height);
        const float cap = bookends ? std::clamp(height * skin.cap_ratio, 0.0f, width * 0.5f) : 0;
        const float gap = bookends ? std::clamp(skin.cap_gap, 0.0f, (width - cap * 2) * 0.5f) : 0;
        const float bodyWidth = std::max(0.0f, width - (cap + gap) * 2);
        const float top = std::clamp(height * skin.top_inset, 0.0f, height);
        const float bottom = std::clamp(height * skin.bottom_inset, 0.0f, height - top);
        return {{{{bounds.x, bounds.y, cap, height}, {bounds.x + cap + gap, bounds.y, bodyWidth, height},
            {bounds.x + width - cap, bounds.y, cap, height}}},
            {bounds.x + cap + gap, bounds.y + top, bodyWidth, height - top - bottom}};
    }
}
