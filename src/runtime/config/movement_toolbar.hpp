#pragma once

#include "design_system.hpp"

#include <algorithm>

namespace xiui::ui
{
    struct movement_toolbar
    {
        rect bounds{};

        static auto Above(const rect& module, const rect& viewport) -> movement_toolbar
        {
            const float width = std::min(196.0f, std::max(1.0f, viewport.width - 8)), height = 62;
            const float x = std::clamp(module.x + (module.width - width) * 0.5f, viewport.x + 4,
                viewport.x + std::max(4.0f, viewport.width - width - 4));
            const float y = module.y - height - 8 >= viewport.y + 4 ? module.y - height - 8 : module.y + module.height + 8;
            return {{x, std::clamp(y, viewport.y + 4, viewport.y + std::max(4.0f, viewport.height - height - 4)), width, height}};
        }
        auto Button(const size_t index) const -> rect
        {
            const float width = (this->bounds.width - 16) * 0.5f;
            return {this->bounds.x + 6 + index * (width + 4), this->bounds.y + 5, width, 24};
        }
        auto Coordinates(void) const -> rect { return {this->bounds.x + 6, this->bounds.y + 33, this->bounds.width - 12, 23}; }
    };
}
