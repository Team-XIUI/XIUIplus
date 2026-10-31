#pragma once

#include "modules/module.hpp"

namespace xiui::ui
{
    inline constexpr float xiv1_resource_height = 12, xiv1_tp_height = 12, xiv1_resource_gap = 5;

    inline auto Xiv1ResourceHeight(const size_t index) -> float
    { return index == 2 ? xiv1_tp_height : xiv1_resource_height; }

    inline auto Xiv1ResourceTrack(const rect& row, const size_t index) -> rect
    { return {row.x, row.y + 1, row.width, Xiv1ResourceHeight(index)}; }

    inline auto Xiv1ResourceLabel(module_text_profile source) -> module_text_profile
    {
        source = TintTextProfile(source, 0xFFE3D2A0);
        source.style.font = xiv1_italic_font;
        source.style.scale *= .8f;
        source.style.alignment = text_alignment::left;
        source.placement = 0;
        source.offset_x = std::clamp(source.offset_x, -1.0f, 8.0f);
        source.offset_y = std::clamp(source.offset_y, -2.0f, 2.0f);
        return source;
    }

    inline auto Xiv1ResourceValue(module_text_profile source, const float baselineOffset, const bool compact = false) -> module_text_profile
    {
        source.style.font = xiv1_number_font;
        source.style.scale *= compact ? 1.0f : 1.3f;
        source.style.alignment = text_alignment::right;
        source.placement = 0;
        source.offset_x = std::clamp(source.offset_x, -8.0f, 8.0f);
        source.offset_y = std::clamp(baselineOffset, -2.0f, 2.0f);
        return source;
    }

    struct xiv1_resource_text_layout { rect label{}, value{}; };

    inline auto LayoutXiv1TPNumber(const rect& row, const float labelWidth, const float valueWidth,
        const float labelHeight, const float valueHeight) -> xiv1_resource_text_layout
    {
        const float right = row.x + std::max(0.0f, row.width - 6);
        const float numberWidth = std::min(valueWidth, std::max(0.0f, right - row.x));
        const float gap = labelWidth > 0 ? 6.0f : 0;
        const float captionWidth = std::min(labelWidth, std::max(0.0f, right - row.x - numberWidth - gap));
        const float center = row.y + row.height * .5f;
        return {{right - numberWidth - gap - captionWidth, center - labelHeight * .5f, captionWidth, labelHeight},
            {right - numberWidth, center - valueHeight * .5f, numberWidth, valueHeight}};
    }

    inline auto LayoutXiv1ResourceText(const rect& track, const float labelHeight, const float valueHeight) -> xiv1_resource_text_layout
    {
        const float lowerRim = track.y + track.height - 1;
        const float labelInset = std::min(6.0f, track.width * .15f), valueInset = std::min(4.0f, track.width * .1f);
        const float valueRightInset = std::min(6.0f, std::max(0.0f, track.width - valueInset));
        return {{track.x + labelInset, lowerRim - labelHeight * .5f, std::max(0.0f, track.width - labelInset * 2), labelHeight},
            {track.x + valueInset, lowerRim + 2 - valueHeight * .5f, std::max(0.0f, track.width - valueInset - valueRightInset), valueHeight}};
    }
}
