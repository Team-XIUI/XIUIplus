#pragma once

#include "config_controls.hpp"

#include <array>
#include <string_view>
#include <vector>

namespace xiui::ui
{
    class share_key_view
    {
    public:
        auto Reset(void) -> void { this->lines_.clear(); this->width_ = -1; this->first_ = 0; }
        auto Scroll(int lines) -> void { this->first_ = std::clamp(this->first_ + lines, 0, this->Maximum()); }
        auto Page(int direction) -> void { this->Scroll(direction * this->visible_); }
        auto Home(void) -> void { this->first_ = 0; }
        auto End(void) -> void { this->first_ = this->Maximum(); }
        auto First(void) const -> int { return this->first_; }
        auto Lines(void) const -> const std::vector<std::string>& { return this->lines_; }

        auto Reflow(std::string_view key, float width, int visible, const std::array<float, 128>& advances, float spacing = 0) -> void
        {
            this->width_ = width;
            this->visible_ = std::max(1, visible);
            this->lines_.clear();
            size_t begin{};
            float used{};
            for (size_t i = 0; i < key.size(); ++i)
            {
                const auto character = static_cast<unsigned char>(key[i]);
                const float advance = advances[character < advances.size() ? character : '?'];
                if (i > begin && used + spacing + advance > width)
                {
                    this->lines_.emplace_back(key.substr(begin, i - begin));
                    begin = i;
                    used = 0;
                }
                used += (i > begin ? spacing : 0) + advance;
            }
            if (begin < key.size()) this->lines_.emplace_back(key.substr(begin));
            this->Scroll(0);
        }

        auto Draw(const design_system& design, const config_controls& controls, const rect& bounds, std::string_view key, bool hovered, bool invalid = false) -> void
        {
            const auto& theme = design.GetTheme();
            design.FillRoundedRect(bounds, (theme.colors.chrome & 0x00FFFFFF) | 0xE0000000, 4);
            design.BorderRect(bounds, invalid ? 0xFFFF8080 : hovered ? theme.colors.control_selected : theme.colors.border);
            const rect content{bounds.x + 12, bounds.y + 10, std::max(1.0f, bounds.width - 32), std::max(1.0f, bounds.height - 20)};
            auto style = theme.body_text;
            style.scale = 0.74f;
            style.alignment = text_alignment::left;
            style = controls.Style(style);
            const float spacing = style.letter_spacing * TextPixelScale(style.scale);
            const int visible = std::max(1, static_cast<int>(content.height / line_height));
            if (this->width_ != content.width || this->visible_ != visible || this->font_ != style.font || this->spacing_ != spacing)
            {
                std::array<float, 128> advances{};
                for (size_t i = 32; i < advances.size(); ++i)
                {
                    const char glyph[]{static_cast<char>(i), '\0'};
                    advances[i] = std::max(1.0f, design.MeasureText(glyph, style));
                }
                this->font_ = style.font; this->spacing_ = spacing;
                this->Reflow(key, content.width, visible, advances, spacing);
            }
            if (key.empty())
            {
                style.top_color = style.bottom_color = theme.colors.muted;
                design.DrawText("Paste an XIUI share key", {content.x, content.y, content.width, line_height}, style);
                return;
            }
            for (int row = 0; row < this->visible_ && this->first_ + row < static_cast<int>(this->lines_.size()); ++row)
                design.DrawText(this->lines_[this->first_ + row].c_str(),
                    {content.x, content.y + row * line_height, content.width, line_height}, style);
            if (this->Maximum() > 0)
            {
                const float height = std::max(10.0f, content.height * this->visible_ / static_cast<float>(this->lines_.size()));
                const float y = content.y + (content.height - height) * this->first_ / this->Maximum();
                design.FillRoundedRect({bounds.x + bounds.width - 9, y, 3, height}, theme.colors.muted, 1);
            }
        }

    private:
        static constexpr float line_height = 18;
        std::vector<std::string> lines_{};
        float width_{-1};
        int font_{-1};
        float spacing_{};
        int first_{}, visible_{1};
        auto Maximum(void) const -> int { return std::max(0, static_cast<int>(this->lines_.size()) - this->visible_); }
    };
}
