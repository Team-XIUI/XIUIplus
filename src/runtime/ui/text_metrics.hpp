#pragma once

#include <algorithm>
#include <cstdint>

namespace xiui::ui
{
    struct text_glyph_metrics { float advance{}, offset{}; };

    inline auto TextGlyphMetrics(const uint32_t codepoint, const float naturalAdvance, const float digitAdvance,
        const bool tabularNumbers) -> text_glyph_metrics
    {
        if (tabularNumbers && codepoint >= '0' && codepoint <= '9')
            return {digitAdvance, std::max(0.0f, (digitAdvance - naturalAdvance) * 0.5f)};
        return {naturalAdvance, 0};
    }
}
