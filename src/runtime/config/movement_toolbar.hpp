#pragma once

#include "design_system.hpp"

#include <algorithm>

namespace xiui::ui
{
    struct movement_toolbar
    {
        rect bounds{};

        static auto AtTop(const rect& viewport) -> movement_toolbar
        {
            const float width = std::min(320.0f, std::max(1.0f, viewport.width - 8));
            return {{viewport.x + (viewport.width - width) * 0.5f, viewport.y + 4, width, 28}};
        }
        auto Button(const size_t index) const -> rect
        {
            return {this->bounds.x + this->bounds.width - 54 + index * 26, this->bounds.y + 3, 22, 22};
        }
        auto Label(void) const -> rect { return {this->bounds.x + 8, this->bounds.y + 3, std::max(0.0f, this->bounds.width - 198), 22}; }
        auto Coordinates(void) const -> rect { return {this->bounds.x + this->bounds.width - 186, this->bounds.y + 3, 128, 22}; }
    };
}
