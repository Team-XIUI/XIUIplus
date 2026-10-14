#pragma once

#include "attributions.hpp"
#include "config_layout.hpp"

#include <cmath>
#include <optional>

namespace xiui::ui
{
    class contributor_list_layout
    {
    public:
        explicit contributor_list_layout(const config_layout& layout, const int scroll = 0)
            : bounds_(layout.ContentArea()), scroll_(scroll) {}

        auto Header(const size_t group) const -> rect
        { return {this->bounds_.x, this->GroupTop(group), this->bounds_.width, header_height}; }

        auto Member(const size_t index) const -> rect
        {
            const auto group = xiui_contributors[index].group;
            size_t preceding{};
            for (size_t i = 0; i < index; ++i) preceding += xiui_contributors[i].group == group;
            const float width = std::max(1.0f, (this->bounds_.width - column_gap) * 0.5f);
            return {this->bounds_.x + (preceding % 2) * (width + column_gap),
                this->GroupTop(static_cast<size_t>(group)) + header_height + (preceding / 2) * member_height, width, member_height};
        }

        auto Visible(const rect& row) const -> bool
        { return row.y >= this->bounds_.y && row.y + row.height <= this->bounds_.y + this->bounds_.height; }

        auto MemberAt(const float x, const float y) const -> std::optional<size_t>
        {
            for (size_t index = 0; index < xiui_contributors.size(); ++index)
            {
                const auto row = this->Member(index), button = config_layout::ChoiceButton(row);
                if (xiui_contributors[index].url != nullptr && this->Visible(row) &&
                    x >= button.x && y >= button.y && x < button.x + button.width && y < button.y + button.height) return index;
            }
            return std::nullopt;
        }

        auto MaximumScroll(void) const -> int
        {
            float height{};
            for (size_t group = 0; group < std::size(contributor_groups); ++group)
                height += header_height + ((GroupSize(group) + 1) / 2) * member_height + (group == 0 ? 0 : group_gap);
            return static_cast<int>(std::ceil(std::max(0.0f, height - this->bounds_.height) / scroll_step));
        }

    private:
        static constexpr float header_height = 28, member_height = 44, group_gap = 12, column_gap = 10, scroll_step = 22;
        rect bounds_{};
        int scroll_{};

        static auto GroupSize(const size_t group) -> size_t
        {
            return static_cast<size_t>(std::count_if(xiui_contributors.begin(), xiui_contributors.end(),
                [group](const auto& member) { return static_cast<size_t>(member.group) == group; }));
        }

        auto GroupTop(const size_t group) const -> float
        {
            float y = this->bounds_.y - this->scroll_ * scroll_step;
            for (size_t index = 0; index < group; ++index)
                y += header_height + ((GroupSize(index) + 1) / 2) * member_height + group_gap;
            return y;
        }
    };
}
