#pragma once

#include "design_system.hpp"

#include <algorithm>

namespace xiui::ui
{
    struct movement_toolbar
    {
        rect bounds{};
        bool scale{true};

        static auto AtTop(const rect& viewport, const bool scale = true) -> movement_toolbar
        {
            const float width = std::min(440.0f, std::max(1.0f, viewport.width - 8));
            return {{viewport.x + (viewport.width - width) * 0.5f, viewport.y + 4, width, 28}, scale};
        }
        auto Button(const size_t index) const -> rect
        {
            if (index == 2 && !this->scale) return {};
            return {this->bounds.x + this->bounds.width - (this->scale ? 110 : 60) + index * 26, this->bounds.y + 3, index == 2 ? 50.0f : 22.0f, 22};
        }
        auto Label(void) const -> rect { return {this->bounds.x + 8, this->bounds.y + 3, std::max(0.0f, this->bounds.width - (this->scale ? 254 : 204)), 22}; }
        auto Coordinates(void) const -> rect { return {this->bounds.x + this->bounds.width - (this->scale ? 242 : 192), this->bounds.y + 3, 128, 22}; }
    };
}
