#pragma once

#include "design_system.hpp"

#include <algorithm>

namespace xiui::ui
{
    enum config_resize_edge : uint8_t { resize_left = 1, resize_right = 2, resize_top = 4, resize_bottom = 8 };

    inline auto ResizeConfigWindow(const rect& original, const uint8_t edges, const float dx, const float dy,
        const float minimumWidth, const float minimumHeight, const D3DVIEWPORT8& viewport) -> rect
    {
        const float left = static_cast<float>(viewport.X), top = static_cast<float>(viewport.Y);
        const float right = left + viewport.Width, bottom = top + viewport.Height;
        const float minWidth = std::min(minimumWidth, static_cast<float>(viewport.Width));
        const float minHeight = std::min(minimumHeight, static_cast<float>(viewport.Height));
        auto result = original;
        if ((edges & resize_left) != 0)
        {
            const float fixed = original.x + original.width;
            result.x = std::clamp(original.x + dx, left, std::max(left, fixed - minWidth));
            result.width = fixed - result.x;
        }
        else if ((edges & resize_right) != 0)
            result.width = std::clamp(original.width + dx, minWidth, std::max(minWidth, right - original.x));
        if ((edges & resize_top) != 0)
        {
            const float fixed = original.y + original.height;
            result.y = std::clamp(original.y + dy, top, std::max(top, fixed - minHeight));
            result.height = fixed - result.y;
        }
        else if ((edges & resize_bottom) != 0)
            result.height = std::clamp(original.height + dy, minHeight, std::max(minHeight, bottom - original.y));
        return result;
    }
}
