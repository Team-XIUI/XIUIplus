#pragma once

#include "design_system.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>

namespace xiui::ui
{
    inline constexpr size_t rounded_rect_corner_steps = 12;
    inline constexpr size_t rounded_rect_points = 4 * (rounded_rect_corner_steps + 1);

    inline auto RoundedRectPoint(const rect& bounds, const float radius, const size_t index, const float inset = 0) -> point
    {
        static const auto directions = [] {
            std::array<point, rounded_rect_points> result{};
            for (size_t i = 0; i < result.size(); ++i)
            {
                const size_t corner = i / (rounded_rect_corner_steps + 1), step = i % (rounded_rect_corner_steps + 1);
                const float angle = (static_cast<float>(corner) - 1 + static_cast<float>(step) / rounded_rect_corner_steps) * std::numbers::pi_v<float> * 0.5f;
                result[i] = {std::cos(angle), std::sin(angle)};
            }
            return result;
        }();
        const float trim = std::min(inset, std::min(bounds.width, bounds.height) * 0.5f);
        const float left = bounds.x + trim, top = bounds.y + trim;
        const float right = bounds.x + bounds.width - trim, bottom = bounds.y + bounds.height - trim;
        const float curve = std::clamp(radius - trim, 0.0f, std::min(right - left, bottom - top) * 0.5f);
        const size_t at = index % rounded_rect_points, corner = at / (rounded_rect_corner_steps + 1);
        const float x = corner < 2 ? right - curve : left + curve;
        const float y = corner == 0 || corner == 3 ? top + curve : bottom - curve;
        return {x + directions[at].x * curve, y + directions[at].y * curve};
    }
}
