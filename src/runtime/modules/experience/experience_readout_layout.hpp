#pragma once

#include "experience_readouts.hpp"
#include "design_system.hpp"

#include <algorithm>
#include <span>

namespace xiui
{
    struct experience_readout_layout
    {
        struct item { ui::rect bounds{}; float scale{1}; size_t row{}; bool divider{}; };
        std::array<item, experience_readout_count> items{};
        std::array<float, 8> row_widths{};
        size_t count{}, rows{};
        float height{};
        bool overflow{};
        ui::rect ellipsis{};
    };

    inline auto LayoutExperienceReadouts(const std::span<const float> widths, const float requestedWidth,
        const float rowHeight, const float rowSpacing, const size_t requestedRows, const float dividerWidth) -> experience_readout_layout
    {
        experience_readout_layout result{};
        const float width = std::max(1.0f, requestedWidth);
        const size_t limit = std::clamp<size_t>(requestedRows, 1, result.row_widths.size());
        float x{};
        size_t row{};
        for (size_t index = 0; index < std::min(widths.size(), result.items.size()); ++index)
        {
            const float measured = std::max(0.0f, widths[index]);
            if (x > 0 && x + dividerWidth + measured > width)
            {
                if (row + 1 >= limit) { result.overflow = true; break; }
                ++row; x = 0;
            }
            const bool divider = x > 0;
            if (divider) x += dividerWidth;
            const float fitted = std::min(measured, width);
            result.items[result.count++] = {{x, row * (rowHeight + rowSpacing), fitted, rowHeight}, measured > 0 ? fitted / measured : 1, row, divider};
            x += fitted;
            result.row_widths[row] = x;
        }
        if (result.count != 0)
        {
            result.rows = row + 1;
            result.height = result.rows * rowHeight + (result.rows - 1) * rowSpacing;
        }
        if (result.overflow)
        {
            const float ellipsisWidth = std::min(16.0f, width);
            const float end = std::max(0.0f, width - ellipsisWidth - 4);
            while (result.count != 0 && result.items[result.count - 1].row == row && result.items[result.count - 1].bounds.x >= end) --result.count;
            if (result.count != 0 && result.items[result.count - 1].row == row)
            {
                auto& last = result.items[result.count - 1];
                const float fitted = std::min(last.bounds.width, end - last.bounds.x);
                if (last.bounds.width > 0) last.scale *= fitted / last.bounds.width;
                last.bounds.width = fitted;
            }
            result.ellipsis = {width - ellipsisWidth, row * (rowHeight + rowSpacing), ellipsisWidth, rowHeight};
            result.row_widths[row] = width;
        }
        return result;
    }
}
