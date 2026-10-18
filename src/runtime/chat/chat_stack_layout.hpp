#pragma once

#include "design_system.hpp"

#include <algorithm>
#include <array>

namespace xiui
{
    inline auto ChatStackOffsets(const ui::rect& first, const ui::rect& second,
        const float firstHeight, const float secondHeight) -> std::array<float, 2>
    {
        if (std::min(first.x + first.width, second.x + second.width) <= std::max(first.x, second.x)) return {};
        if (first.y + first.height <= second.y)
            return {std::clamp(second.height - secondHeight, 0.0f, second.height), 0.0f};
        if (second.y + second.height <= first.y)
            return {0.0f, std::clamp(first.height - firstHeight, 0.0f, first.height)};
        return {};
    }
}
