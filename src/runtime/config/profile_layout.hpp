#pragma once

#include "config_layout.hpp"

namespace xiui::ui
{
    class profile_dialog_layout
    {
    public:
        explicit profile_dialog_layout(const rect& config, const D3DVIEWPORT8& viewport, bool extended = false)
            : extended_(extended)
        {
            const float width = std::min(extended ? 560.0f : 420.0f, std::max(1.0f, viewport.Width - 16.0f));
            const float height = std::min(extended ? 384.0f : 218.0f, std::max(1.0f, viewport.Height - 16.0f));
            this->bounds_ = {std::clamp(config.x + (config.width - width) * 0.5f, 8.0f, std::max(8.0f, viewport.Width - width - 8)),
                std::clamp(config.y + (config.height - height) * 0.5f, 8.0f, std::max(8.0f, viewport.Height - height - 8)), width, height};
        }
        auto Bounds(void) const -> rect { return this->bounds_; }
        auto Title(void) const -> rect { return this->Row(12, 28, 36); }
        auto Help(void) const -> rect { return this->Row(42, 22); }
        auto NameLabel(void) const -> rect { return this->Row(this->extended_ ? 76.0f : 65.0f, 18); }
        auto Field(void) const -> rect { return this->Row(this->extended_ ? 98.0f : 87.0f, 32); }
        auto Cancel(void) const -> rect
        {
            if (!this->extended_) return {this->bounds_.x + this->bounds_.width - 202, this->bounds_.y + this->bounds_.height - 43, 86, 28};
            return {this->bounds_.x + this->bounds_.width - 250, this->bounds_.y + this->bounds_.height - 50, 96, 32};
        }
        auto Create(void) const -> rect
        {
            if (!this->extended_) return {this->bounds_.x + this->bounds_.width - 108, this->bounds_.y + this->bounds_.height - 43, 90, 28};
            return {this->bounds_.x + this->bounds_.width - 144, this->bounds_.y + this->bounds_.height - 50, 120, 32};
        }
        auto Close(void) const -> rect { return {this->bounds_.x + this->bounds_.width - 36, this->bounds_.y + 12, 22, 22}; }
        auto SourceLabel(void) const -> rect { return this->Row(144, 18, 106); }
        auto Source(void) const -> rect { return this->Row(170, std::max(18.0f, this->bounds_.height - 288)); }
        auto Browse(void) const -> rect { const auto source = this->Source(); return {source.x + 12, source.y + 12, 112, 28}; }
        auto Paste(void) const -> rect { return {this->bounds_.x + this->bounds_.width - 144, this->bounds_.y + 138, 120, 26}; }
        auto ImportSource(size_t index) const -> rect { auto value = this->SourceLabel(); value.y -= 6; value.x += index * 120; value.width = 112; value.height = 26; return value; }
        auto Status(void) const -> rect { return this->Row(this->bounds_.height - 110, 22); }
        auto Error(void) const -> rect { return this->Row(this->extended_ ? this->bounds_.height - 86 : 125.0f, 28); }
    private:
        rect bounds_{};
        bool extended_{};
        auto Row(float y, float height, float reserve = 0) const -> rect
        {
            const float inset = this->extended_ ? 24.0f : 18.0f;
            return {this->bounds_.x + inset, this->bounds_.y + y, this->bounds_.width - 2 * inset - reserve, height};
        }
    };
}
