#pragma once

#include "design_system.hpp"

#include <algorithm>
#include <cmath>

namespace xiui::ui
{
    class config_scrollbar
    {
    public:
        rect track{};

        config_scrollbar(void) = default;
        config_scrollbar(const rect& bounds, const int total, const int page, const int position)
            : track(bounds), total_(std::max(0, total)), page_(std::max(1, page))
        { this->position_ = std::clamp(position, 0, this->Maximum()); }

        auto Maximum(void) const -> int { return std::max(0, this->total_ - this->page_); }
        auto Visible(void) const -> bool { return this->Maximum() > 0 && this->track.width > 0 && this->track.height > 0; }
        auto Thumb(void) const -> rect
        {
            if (!this->Visible()) return {};
            const float height = std::clamp(this->track.height * this->page_ / this->total_, std::min(28.0f, this->track.height), this->track.height);
            const float y = this->track.y + (this->track.height - height) * this->position_ / this->Maximum();
            return {this->track.x, y, this->track.width, height};
        }
        auto PositionAt(const float y, const float grabOffset) const -> int
        {
            const float travel = this->track.height - this->Thumb().height;
            if (!this->Visible() || travel <= 0) return 0;
            const float ratio = std::clamp((y - this->track.y - grabOffset) / travel, 0.0f, 1.0f);
            return static_cast<int>(std::round(ratio * this->Maximum()));
        }
        auto PageAt(const float y) const -> int
        { return std::clamp(this->position_ + (y < this->Thumb().y ? -this->page_ : this->page_), 0, this->Maximum()); }

    private:
        int total_{};
        int page_{1};
        int position_{};
    };
} // namespace xiui::ui
