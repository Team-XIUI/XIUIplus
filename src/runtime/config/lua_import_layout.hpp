#pragma once
#include "config_layout.hpp"

namespace xiui::ui
{
    class lua_import_layout
    {
    public:
        lua_import_layout(const rect& config, const D3DVIEWPORT8& viewport)
        {
            const float width = std::min(760.0f, std::max(1.0f, viewport.Width - 16.0f));
            const float height = std::min(620.0f, std::max(1.0f, viewport.Height - 16.0f));
            this->bounds_ = {std::clamp(config.x + (config.width - width) * 0.5f, 8.0f, std::max(8.0f, viewport.Width - width - 8)),
                std::clamp(config.y + (config.height - height) * 0.5f, 8.0f, std::max(8.0f, viewport.Height - height - 8)), width, height};
            this->scale_ = std::min(1.0f, std::max(0.5f, (height - 100) / 520));
        }
        auto Bounds() const -> rect { return this->bounds_; }
        auto Title() const -> rect { return {this->bounds_.x + 20, this->bounds_.y + 10, this->bounds_.width - 64, 28}; }
        auto Close() const -> rect { return {this->bounds_.x + this->bounds_.width - 36, this->bounds_.y + 12, 22, 22}; }
        auto PathLabel() const -> rect { return this->Row(90, 18); }
        auto NameLabel() const -> rect { return this->Row(202, 18); }
        auto Status() const -> rect { return {this->bounds_.x + 20, this->bounds_.y + this->bounds_.height - 82, this->bounds_.width - 40, 24}; }
        auto Control(const size_t index) const -> rect
        {
            const auto width = this->bounds_.width;
            if (index == 0 || index == 1) return {this->bounds_.x + width - (index == 0 ? 246 : 140), this->bounds_.y + this->bounds_.height - 46, index == 0 ? 96.0f : 120.0f, 30};
            if (index == 10 || index == 11) return {this->bounds_.x + 20 + (index - 10) * 130, this->Y(50), 122, 28};
            if (index == 2) return {this->bounds_.x + 20, this->Y(158), 100, 30};
            if (index == 3) return {this->bounds_.x + width - 120, this->Y(114), 100, 30};
            if (index == 4) return this->Row(224, 30);
            if (index == 5) { auto value = this->Row(114, 30); value.width -= 110; return value; }
            if (index == 6) return {this->bounds_.x + 130, this->Y(158), 36, 30};
            if (index == 7) return {this->bounds_.x + width - 56, this->Y(158), 36, 30};
            if (index == 8 || index == 9 || index == 13)
            { auto value = this->Row(266, 26); value.width = (width - 56) / 3; value.x += (index == 13 ? 2 : index - 8) * (value.width + 8); return value; }
            if (index == 12) return {this->bounds_.x + 20, this->Y(304), width - 40, std::max(18.0f, this->Status().y - this->Y(304) - 10)};
            return {};
        }
        auto Selection() const -> rect { auto value = this->Control(6); value.x += 44; value.width = this->Control(7).x - value.x - 8; return value; }
    private:
        rect bounds_{};
        float scale_{1};
        auto Y(const float y) const -> float { return this->bounds_.y + 44 + (y - 44) * this->scale_; }
        auto Row(const float y, const float height) const -> rect { return {this->bounds_.x + 20, this->Y(y), this->bounds_.width - 40, height}; }
    };
}
