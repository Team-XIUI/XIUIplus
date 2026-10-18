#pragma once

#include "design_system.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <string>
#include <vector>

namespace xiui
{
    enum class config_controller_region : uint8_t { categories, sections, topics, content_tabs, elements, options, profiles, movement, utilities, count };

    struct config_controller_item
    {
        std::string id;
        ui::rect bounds;
        size_t index{};
        std::string help;
    };

    struct config_controller_focus
    {
        config_controller_region region{config_controller_region::categories};
        size_t item{};
        std::string id;
        std::string modal_id;
        std::array<std::string, static_cast<size_t>(config_controller_region::count)> remembered{};
        bool active{}, adjusting{};
        uint8_t modal_item{};
        std::string moving_id;
        ui::rect move_original{}, move_desired{};
        bool moving{}, fine{}, move_collapsed{};
        bool resizing{}, resize_keyboard{};
        ui::rect resize_original{};
        int movement_scroll{};
        D3DVIEWPORT8 viewport{0, 0, 1920, 1080, 0, 1};

        auto Resolve(const std::vector<config_controller_item>& items) -> void
        {
            if (items.empty()) { this->item = 0; this->id.clear(); this->adjusting = false; return; }
            const auto found = std::find_if(items.begin(), items.end(), [&](const auto& value) { return value.id == this->id; });
            if (found == items.end()) this->adjusting = false;
            this->item = found == items.end() ? std::min(this->item, items.size() - 1) : static_cast<size_t>(found - items.begin());
            this->id = items[this->item].id;
        }

        auto Move(const std::vector<config_controller_item>& items, const int amount) -> bool
        {
            this->Resolve(items);
            if (items.empty()) return false;
            const auto previous = this->item;
            this->item = static_cast<size_t>(std::clamp(static_cast<int>(this->item) + amount, 0, static_cast<int>(items.size()) - 1));
            this->id = items[this->item].id;
            return previous != this->item;
        }

        auto MoveGrid(const std::vector<config_controller_item>& items, const int x, const int y) -> bool
        {
            this->Resolve(items);
            if (items.empty()) return false;
            const auto& origin = items[this->item].bounds;
            auto next = this->item;
            float nearest = std::numeric_limits<float>::max(), alignment = nearest;
            for (size_t at = 0; at < items.size(); ++at)
            {
                const auto& bounds = items[at].bounds;
                const float dx = bounds.x - origin.x, dy = bounds.y - origin.y;
                const float forward = x != 0 ? dx * x : dy * y;
                const float across = std::abs(x != 0 ? dy : dx);
                if (forward <= 1 || (x != 0 && across > 1)) continue;
                if (forward < nearest || (forward == nearest && across < alignment))
                { next = at; nearest = forward; alignment = across; }
            }
            const bool changed = next != this->item;
            this->item = next;
            this->id = items[next].id;
            return changed;
        }
    };
}
