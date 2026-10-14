#pragma once

#include "design_system.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <span>
#include <utility>

namespace xiui::ui
{
    struct layout_grid_settings
    {
        bool visible{true}, snap_grid{}, snap_elements{true};
        float amount{6}, spacing{20}, opacity{0.25f};
    };

    struct layout_snap_result
    {
        rect bounds{};
        std::optional<float> vertical{}, horizontal{};
    };

    inline auto SnapLayoutBounds(rect moving, const std::span<const rect> targets, const rect& viewport,
        const layout_grid_settings& settings) -> layout_snap_result
    {
        const auto snapAxis = [&](const bool horizontal) -> std::pair<float, std::optional<float>> {
            const float origin = horizontal ? viewport.x : viewport.y;
            const float extent = horizontal ? viewport.width : viewport.height;
            const float size = horizontal ? moving.width : moving.height;
            const float position = std::clamp(horizontal ? moving.x : moving.y, origin, origin + std::max(0.0f, extent - size));
            float best = std::numeric_limits<float>::max(), correction{};
            std::optional<float> guide;
            const auto consider = [&](const float delta, const float line) {
                const float distance = std::abs(delta), result = position + delta;
                if (distance <= settings.amount && distance < best && result >= origin && result <= origin + std::max(0.0f, extent - size))
                { best = distance; correction = delta; guide = line; }
            };
            const auto align = [&](const float start, const float length, const bool adjacent) {
                consider(start - position, start);
                consider(start + length - position - size, start + length);
                consider(start + length * 0.5f - position - size * 0.5f, start + length * 0.5f);
                if (adjacent)
                {
                    consider(start - position - size, start);
                    consider(start + length - position, start + length);
                }
            };
            if (settings.snap_elements)
            {
                for (const auto& target : targets)
                    if (target.width > 0 && target.height > 0)
                        align(horizontal ? target.x : target.y, horizontal ? target.width : target.height, true);
                align(origin, extent, false);
            }
            if (settings.snap_grid && settings.spacing >= 4)
            {
                const float line = origin + std::round((position - origin) / settings.spacing) * settings.spacing;
                consider(line - position, line);
            }
            return {position + correction, guide};
        };
        const auto x = snapAxis(true), y = snapAxis(false);
        moving.x = x.first; moving.y = y.first;
        return {moving, x.second, y.second};
    }
}
