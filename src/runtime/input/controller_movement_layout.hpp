#pragma once

#include "design_system.hpp"

#include <algorithm>

namespace xiui
{
    inline auto ClampMovementPosition(ui::rect& bounds, const ui::rect& visual, const D3DVIEWPORT8& viewport) -> void
    {
        bounds.x += std::clamp(visual.x, 0.0f, std::max(0.0f, viewport.Width - visual.width)) - visual.x;
        bounds.y += std::clamp(visual.y, 0.0f, std::max(0.0f, viewport.Height - visual.height)) - visual.y;
    }

    struct controller_movement_layout
    {
        ui::rect bounds{};
        int rows{};

        controller_movement_layout(const ui::rect& config, const D3DVIEWPORT8& viewport, const size_t count)
        {
            const float width = std::min(340.0f, std::max(1.0f, viewport.Width - 16.0f));
            this->rows = std::max(1, std::min({8, static_cast<int>(count), static_cast<int>((viewport.Height - 130.0f) / 28)}));
            const float height = this->rows * 28.0f + 72;
            this->bounds = {std::clamp(config.x + config.width - width - 12, 8.0f, std::max(8.0f, viewport.Width - width - 8)),
                std::clamp(config.y + 42, 8.0f, std::max(8.0f, viewport.Height - height - 8)), width, height};
        }

        auto Row(const int index) const -> ui::rect
        { return {this->bounds.x + 8, this->bounds.y + 32 + index * 28, this->bounds.width - 16, 26}; }
    };
}
