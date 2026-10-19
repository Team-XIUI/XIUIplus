#pragma once

#include "design_system.hpp"

#include <algorithm>
#include <cmath>

namespace xiui::ui
{
    inline auto ContourPoint(const rect& bounds, const point normalized, const float inset = 0) -> point
    {
        const point at{bounds.x + normalized.x * bounds.width, bounds.y + normalized.y * bounds.height};
        if (inset == 0) return at;
        const float dx = bounds.x + bounds.width / 2 - at.x, dy = bounds.y + bounds.height / 2 - at.y;
        const float distance = std::sqrt(dx * dx + dy * dy);
        const float amount = distance > 0 ? std::min(inset / distance, 1.0f) : 0;
        return {at.x + dx * amount, at.y + dy * amount};
    }

    inline auto ContourContains(const rect& bounds, const std::span<const point> contour, const point at, const float margin = 0) -> bool
    {
        bool inside{};
        if (contour.size() < 3) return false;
        const float marginSquared = margin * margin;
        auto a = ContourPoint(bounds, contour.back());
        for (const auto normalized : contour)
        {
            const auto b = ContourPoint(bounds, normalized);
            if ((a.y > at.y) != (b.y > at.y) && at.x < a.x + (b.x - a.x) * (at.y - a.y) / (b.y - a.y)) inside = !inside;
            if (margin > 0)
            {
                const float dx = b.x - a.x, dy = b.y - a.y, length = dx * dx + dy * dy;
                const float amount = length > 0 ? std::clamp(((at.x - a.x) * dx + (at.y - a.y) * dy) / length, 0.0f, 1.0f) : 0;
                const float x = at.x - a.x - dx * amount, y = at.y - a.y - dy * amount;
                if (x * x + y * y < marginSquared) return false;
            }
            a = b;
        }
        return inside;
    }
}
