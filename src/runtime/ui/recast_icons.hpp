#pragma once

#include "design_system.hpp"
#include "resources/recast_icons.hpp"

#include <algorithm>

namespace xiui::ui
{
    inline auto RecastIconUV(const bool spell, const uint32_t id, const uint8_t job) -> rect
    {
        const auto find = [](const uint32_t key) {
            const auto& icons = resources::recast_icons;
            const auto found = std::lower_bound(icons.begin(), icons.end(), key, [](const auto& icon, uint32_t value) { return icon.key < value; });
            return found != icons.end() && found->key == key ? &*found : nullptr;
        };
        const resources::recast_icon* icon = !spell && job != 0 ? find(id | (uint32_t{job} << 16)) : nullptr;
        if (icon == nullptr) icon = find(id | (spell ? 0x1000000 : 0));
        if (icon == nullptr) icon = find(spell ? 0x1FFFFFF : 0xFFFFFF);
        if (icon == nullptr) return {};
        return {static_cast<float>((icon->index % 56) * 36 + 2) / resources::recast_icon_width,
            static_cast<float>((icon->index / 56) * 36 + 2) / resources::recast_icon_height,
            32.0f / resources::recast_icon_width, 32.0f / resources::recast_icon_height};
    }
}
