#pragma once

#include "modules/module.hpp"

#include <optional>

namespace xiui::ui
{
    struct element_selector
    {
        static constexpr size_t capacity = 16;
        module_option element{}, scope{};
        size_t option_index{}, scope_index{};
        std::array<const char*, capacity> labels{};
        size_t count{}, selected{};
        bool combined{};
        auto Enabled(void) const -> bool { return this->element.available && (!this->combined || this->scope.available); }
    };

    struct element_selector_layout
    {
        std::array<rect, element_selector::capacity> tabs{};
        size_t count{};
        float height{};

        element_selector_layout(const rect& bounds, std::span<const float> widths)
        {
            float x = bounds.x, y = bounds.y;
            for (const float requested : widths.first(std::min(widths.size(), this->tabs.size())))
            {
                const float width = std::min(std::max(1.0f, bounds.width), requested);
                if (x > bounds.x && x + width > bounds.x + bounds.width) { x = bounds.x; y += 32; }
                this->tabs[this->count++] = {x, y, width, 28};
                x += width + 4;
            }
            if (this->count != 0) this->height = y - bounds.y + 36;
        }

        auto IndexAt(const float x, const float y) const -> std::optional<size_t>
        {
            for (size_t i = 0; i < this->count; ++i)
            {
                const auto& at = this->tabs[i];
                if (x >= at.x && y >= at.y && x < at.x + at.width && y < at.y + at.height) return i;
            }
            return std::nullopt;
        }
    };
}
