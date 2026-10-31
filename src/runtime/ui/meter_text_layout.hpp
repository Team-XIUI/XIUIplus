#pragma once

#include "meter_skin.hpp"

#include <algorithm>

namespace xiui::ui
{
    enum class meter_text_anchor : uint8_t { automatic, top, center, bottom };

    struct meter_text_layout
    {
        rect label{}, value{}, placement{};
    };

    inline auto LayoutMeterText(const rect& track, float labelHeight, float valueHeight, float labelWidth,
        meter_text_anchor anchor = meter_text_anchor::automatic,
        meter_text_anchor valueAnchor = meter_text_anchor::automatic,
        const meter_skin* skin = nullptr, bool bookends = true) -> meter_text_layout
    {
        const auto placement = skin != nullptr ? LayoutMeterSkin(track, *skin, bookends).frame[1] : track;
        const float padding = std::min(4.0f, std::max(0.0f, placement.width) * 0.5f);
        const float width = std::max(0.0f, placement.width - padding * 2);
        const float valueRightPadding = std::min(6.0f, std::max(0.0f, placement.width - padding));
        meter_text_layout result{
            {placement.x + padding, track.y - labelHeight * 0.5f, std::min(labelWidth, width), labelHeight},
            {placement.x + padding, track.y, std::max(0.0f, placement.width - padding - valueRightPadding), track.height}, placement};
        if (anchor != meter_text_anchor::automatic)
        {
            const float y = track.y + (anchor == meter_text_anchor::bottom ? track.height :
                anchor == meter_text_anchor::center ? track.height * 0.5f : 0.0f);
            result.label.y = y - labelHeight * 0.5f;
            result.value.y = y - valueHeight * 0.5f;
            result.value.height = valueHeight;
        }
        if (valueAnchor != meter_text_anchor::automatic)
        {
            const float y = track.y + (valueAnchor == meter_text_anchor::bottom ? track.height :
                valueAnchor == meter_text_anchor::center ? track.height * 0.5f : 0.0f);
            result.value.y = y - valueHeight * 0.5f;
            result.value.height = valueHeight;
        }
        return result;
    }
} // namespace xiui::ui
