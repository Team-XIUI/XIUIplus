#pragma once

#include "config_layout.hpp"

namespace xiui::ui
{
    class basic_settings_layout
    {
    public:
        static constexpr size_t control_count = 6;
        static constexpr size_t xivclassic_control = 4, theme_control = 5;
        static constexpr float scale_label_width = 136;
        explicit basic_settings_layout(const rect& bounds) : bounds_(bounds) {}

        auto Expanded(void) const -> bool { return this->bounds_.height >= 480; }
        auto Heading(void) const -> rect { return {this->Left(), this->bounds_.y + 42, this->Width(), 26}; }
        auto Description(void) const -> rect { return {this->Left(), this->bounds_.y + 71, this->Width(), 20}; }
        auto ModeButton(void) const -> rect { return config_layout{this->bounds_}.ModeButton(); }
        auto Preset(const size_t index) const -> rect
        {
            const float width = (this->Width() - 24) / 3;
            const float height = this->Expanded() ? std::clamp(this->bounds_.height - 390, 154.0f, 220.0f) :
                std::clamp(this->bounds_.height - 300, 154.0f, 184.0f);
            return {this->Left() + index * (width + 12), this->bounds_.y + (this->Expanded() ? 100 : 68), width, height};
        }
        auto PresetButton(const size_t index) const -> rect
        { const auto card = this->Preset(index); return {card.x + 14, card.y + card.height - 36, card.width - 28, 26}; }
        auto Preview(const size_t index) const -> rect
        { const auto card = this->Preset(index); return {card.x + 14, card.y + 34, card.width - 28, std::max(0.0f, card.height - 78)}; }
        auto ThemeRow(void) const -> rect
        { const auto card = this->Preset(0); return {this->Left(), card.y + card.height + 8, this->Width(), 26}; }
        auto ThemeChoices(void) const -> rect
        { const auto row = this->ThemeRow(); return {row.x + scale_label_width, row.y, std::min(420.0f, row.width - scale_label_width), row.height}; }
        auto ScaleRow(void) const -> rect
        {
            const auto theme = this->ThemeRow();
            return {this->Left(), theme.y + theme.height + 8, std::min(600.0f, this->Width() - 112), 28};
        }
        auto ScaleTrack(void) const -> rect { return config_layout::SliderTrack(this->ScaleRow(), scale_label_width, true); }
        auto ScaleValue(void) const -> rect { return config_layout::SliderValue(this->ScaleRow(), true); }
        auto ResetButton(void) const -> rect
        { const auto row = this->ScaleRow(); return {row.x + row.width + 16, row.y, 96, 28}; }
        auto FooterBackground(void) const -> rect
        { return config_layout{this->bounds_}.FooterBackground(); }
        auto Control(const size_t index) const -> rect
        {
            if (index < 2) return this->PresetButton(index);
            if (index == 2) return this->ScaleRow();
            if (index == 3) return this->ResetButton();
            if (index == xivclassic_control) return this->PresetButton(2);
            return index == theme_control ? this->ThemeChoices() : rect{};
        }

    private:
        rect bounds_{};
        auto Left(void) const -> float { return this->bounds_.x + 24; }
        auto Width(void) const -> float { return std::max(1.0f, this->bounds_.width - 48); }
    };
}
