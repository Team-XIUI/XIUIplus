#pragma once

#include "config_layout.hpp"
#include "slider_value_input.hpp"
#include "text_input.hpp"
#include "config_text.hpp"
#include "ui/container_renderer.hpp"
#include "ui/button_renderer.hpp"

namespace xiui::ui
{
    class config_controls
    {
    public:
        explicit config_controls(const design_system& design, const container_renderer* containers = nullptr, const native_ui_assets* assets = nullptr)
            : design_(design), containers_(containers), buttons_(design, assets) {}
        auto SetTextSettings(const config_text_settings& settings) -> void { this->text_settings_ = &settings; }
        auto Style(const text_style& style, bool button = false) const -> text_style;

        auto Button(const rect& bounds, const char* label, const text_style& style, bool highlighted, button_tone tone = button_tone::normal) const -> void;
        auto IconButton(const rect& bounds, icon_glyph icon, bool highlighted) const -> void;
        auto NavigationButton(const rect& bounds, const char* id, const char* label, bool selected, bool hovered = false) const -> void;
        auto NavigationItem(const rect& bounds, const char* label, bool selected, bool hovered = false, bool enabled = true) const -> void;
        auto Segmented(const rect& bounds, std::span<const char* const> labels, size_t selected, float mouseX, float mouseY) const -> void;
        auto ListItem(const rect& bounds, const char* label, const char* accessory, bool selected, bool hovered) const -> void;
        auto Dropdown(const rect& bounds, const char* label, bool open, bool hovered) const -> void;
        auto TextInput(const rect& bounds, const text_input& input, bool invalid = false) const -> void;
        static auto Segment(const rect& bounds, size_t index, size_t count) -> rect;
        auto Panel(const rect& bounds, std::span<const container_fill> fills = {}) const -> void;

        auto Text(const char* text, const rect& bounds, const text_style& style) const -> void;
        auto ButtonText(const char* text, const rect& bounds, const text_style& style, bool highlighted = false) const -> void;
        static auto TextStyle(const text_style& style, bool button = false) -> text_style;
        static auto IconId(const char* id) -> icon_glyph;
        auto Icon(const rect& bounds, const char* id, D3DCOLOR color) const -> void;
        auto Tooltip(const char* text, const rect& anchor, const D3DVIEWPORT8& viewport) const -> void;
        auto Option(const rect& row, const module_option& option, bool hovered, bool activeColor, const slider_value_input* input = nullptr, const text_input* textInput = nullptr, bool dropdownOpen = false) const -> void;

    private:
        const design_system& design_;
        const container_renderer* containers_{};
        button_renderer buttons_;
        const config_text_settings* text_settings_{};
        auto Label(const char* text, const rect& bounds, const text_style& style) const -> void;
        auto NavigationHighlight(const rect& bounds, bool selected, bool hovered) const -> void;

        auto Row(const rect& bounds, const char* label, const char* description, float labelWidth, bool hovered, text_style labelStyle) const -> void;
        auto ToggleRow(const rect& row, const char* label, const char* description, bool enabled, bool hovered) const -> void;
        auto ToggleGroupRow(const rect& row, const module_option& option, bool hovered) const -> void;
        auto ColorRow(const rect& row, const char* label, const char* description, D3DCOLOR color, bool active, bool hovered) const -> void;
        auto SliderRow(const rect& row, const module_option& option, bool hovered, const slider_value_input* input) const -> void;
        auto ChoiceRow(const rect& row, const module_option& option, bool hovered, bool dropdownOpen = false) const -> void;
    };
} // namespace xiui::ui
