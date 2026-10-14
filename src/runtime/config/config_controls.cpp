#include "config_controls.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace xiui::ui
{
    auto config_controls::Button(const rect& bounds, const char* label, const text_style& style, const bool highlighted, const button_tone tone) const -> void
    {
        this->buttons_.Draw(bounds, label, this->Style(style, true), highlighted, tone);
    }

    auto config_controls::Panel(const rect& bounds, const std::span<const container_fill> fills) const -> void
    {
        if (this->containers_ != nullptr) this->containers_->Draw(this->design_, bounds, {}, fills);
        else
        {
            this->design_.DrawPanel(bounds, this->design_.GetTheme().config_panel);
            for (const auto& fill : fills) this->design_.FillRect(fill.bounds, fill.color);
        }
    }

    auto config_controls::NavigationButton(const rect& bounds, const char* id, const char* label, const bool selected, const bool hovered) const -> void
    {
        const auto& theme = this->design_.GetTheme();
        this->NavigationHighlight(bounds, selected, hovered);
        this->Icon(config_layout::NavigationIcon(bounds), id, selected || hovered ? theme.colors.control_selected : theme.colors.text);
        auto style = selected ? theme.accent_text : theme.body_text;
        style.scale = 0.72f;
        style.alignment = text_alignment::left;
        this->ButtonText(label, config_layout::NavigationLabel(bounds), style, selected || hovered);
    }

    auto config_controls::NavigationItem(const rect& bounds, const char* label, const bool selected, const bool hovered, const bool enabled) const -> void
    {
        this->NavigationHighlight(bounds, enabled && selected, enabled && hovered);
        auto style = this->design_.GetTheme().body_text;
        style.scale = 0.72f;
        style.alignment = text_alignment::left;
        if (!enabled) style.top_color = style.bottom_color = (style.top_color & 0x00FFFFFF) | 0x80000000;
        this->ButtonText(label, {bounds.x + 8, bounds.y, bounds.width - 16, bounds.height}, style, enabled && (selected || hovered));
    }

    auto config_controls::NavigationHighlight(const rect& bounds, const bool selected, const bool hovered) const -> void
    {
        if (selected || hovered)
            this->design_.FillRoundedRect(bounds, (this->design_.GetTheme().colors.control_selected & 0xFFFFFF) | (selected ? 0x44000000 : 0x28000000), 3);
    }

    auto config_controls::Segment(const rect& bounds, const size_t index, const size_t count) -> rect
    {
        if (count == 0 || index >= count) return {};
        const float width = std::max(1.0f, (bounds.width - 4 * (count - 1)) / count);
        return {bounds.x + index * (width + 4), bounds.y, width, bounds.height};
    }

    auto config_controls::Segmented(const rect& bounds, const std::span<const char* const> labels, const size_t selected, const float mouseX, const float mouseY) const -> void
    {
        auto style = this->design_.GetTheme().body_text;
        style.scale = 0.72f; style.alignment = text_alignment::center;
        for (size_t index = 0; index < labels.size(); ++index)
        {
            const auto button = Segment(bounds, index, labels.size());
            const bool hovered = mouseX >= button.x && mouseY >= button.y && mouseX < button.x + button.width && mouseY < button.y + button.height;
            this->Button(button, labels[index], style, selected == index || hovered);
        }
    }

    auto config_controls::ListItem(const rect& bounds, const char* label, const char* accessory, const bool selected, const bool hovered) const -> void
    {
        this->NavigationHighlight(bounds, selected, hovered);
        auto style = this->design_.GetTheme().body_text;
        style.scale = 0.70f; style.alignment = text_alignment::left;
        this->ButtonText(label, {bounds.x + 6, bounds.y, std::max(1.0f, bounds.width - 50), bounds.height}, style, selected || hovered);
        style.scale = 0.64f; style.alignment = text_alignment::right;
        this->ButtonText(accessory, {bounds.x + bounds.width - 42, bounds.y, 36, bounds.height}, style, selected || hovered);
    }

    auto config_controls::TextStyle(const text_style& style, const bool button) -> text_style
    {
        auto plain = style;
        plain.bottom_color = plain.top_color;
        plain.shadow = {true, 1.0f, 1.0f, 0xEE000000, false};
        plain.stroke = {button, 0xF0000000};
        plain.glow.enabled = false;
        plain.sheen.enabled = false;
        return plain;
    }

    auto config_controls::Style(const text_style& style, const bool button) const -> text_style
    {
        auto result = TextStyle(style, button);
        if (this->text_settings_ != nullptr)
        {
            result.font = FontIndex(this->text_settings_->font);
            result.letter_spacing = this->text_settings_->letter_spacing / TextPixelScale(style.scale);
            result.shadow = this->text_settings_->shadow;
            result.stroke = {this->text_settings_->outline == 2 || (button && this->text_settings_->outline == 1), this->text_settings_->outline_color};
            result.glow = this->text_settings_->glow;
            result.sheen = this->text_settings_->sheen;
        }
        return result;
    }

    auto config_controls::Dropdown(const rect& bounds, const char* label, const bool open, const bool hovered) const -> void
    {
        this->buttons_.Face(bounds, open || hovered);
        auto style = this->design_.GetTheme().body_text; style.scale = 0.70f; style.alignment = text_alignment::left;
        this->ButtonText(label, {bounds.x + 7, bounds.y, bounds.width - 28, bounds.height}, style, open || hovered);
        this->design_.DrawIcon({bounds.x + bounds.width - 22, bounds.y + (bounds.height - 16) * 0.5f, 16, 16},
            open ? icon_glyph::chevron_up : icon_glyph::chevron_down,
            open || hovered ? this->design_.GetTheme().colors.control_selected : this->design_.GetTheme().colors.text);
    }

    auto config_controls::IconButton(const rect& bounds, const icon_glyph icon, const bool highlighted) const -> void
    {
        this->buttons_.Face(bounds, highlighted);
        const auto size = std::max(0.0f, std::min({18.0f, bounds.width - 8, bounds.height - 6}));
        this->design_.DrawIcon({bounds.x + (bounds.width - size) * 0.5f, bounds.y + (bounds.height - size) * 0.5f, size, size},
            icon, highlighted ? this->design_.GetTheme().colors.control_selected : this->design_.GetTheme().colors.text);
    }

    auto config_controls::TextInput(const rect& bounds, const text_input& input, const bool invalid) const -> void
    {
        this->buttons_.Face(bounds, input.Focused());
        this->design_.BorderRect(bounds, invalid ? 0xFFFF8080 : this->design_.GetTheme().colors.control_selected);
        auto style = this->design_.GetTheme().body_text; style.scale = 0.82f; style.alignment = text_alignment::left;
        style = this->Style(style);
        const auto& editor = input.Editor();
        const auto& text = editor.Text();
        const rect at{bounds.x + 6, bounds.y, bounds.width - 12, bounds.height};
        size_t begin{};
        while (begin < editor.Caret() && this->design_.MeasureText(text.substr(begin, editor.Caret() - begin).c_str(), style) > at.width - 2)
        { ++begin; while (begin < text.size() && (static_cast<unsigned char>(text[begin]) & 0xC0) == 0x80) ++begin; }
        size_t end = text.size();
        while (end > begin && this->design_.MeasureText(text.substr(begin, end - begin).c_str(), style) > at.width)
        { --end; while (end > begin && (static_cast<unsigned char>(text[end]) & 0xC0) == 0x80) --end; }
        const auto widthAt = [&](size_t position) { return this->design_.MeasureText(text.substr(begin, std::clamp(position, begin, end) - begin).c_str(), style); };
        const auto [selectionBegin, selectionEnd] = editor.Selection();
        if (selectionBegin != selectionEnd) this->design_.FillRect({at.x + widthAt(selectionBegin), at.y + 4, std::min(at.width, widthAt(selectionEnd)) - widthAt(selectionBegin), at.height - 8}, 0x665080D0);
        this->Text(text.substr(begin, end - begin).c_str(), at, style);
        if (input.Focused() && (::GetTickCount64() / 500) % 2 == 0) this->design_.FillRect({at.x + widthAt(editor.Caret()), at.y + 6, 1, at.height - 12}, style.top_color);
    }

    auto config_controls::Label(const char* text, const rect& bounds, const text_style& style) const -> void
    {
        button_renderer::DrawLabel(this->design_, text, bounds, style);
    }

    auto config_controls::Text(const char* text, const rect& bounds, const text_style& style) const -> void
    {
        this->Label(text, bounds, this->Style(style));
    }

    auto config_controls::ButtonText(const char* text, const rect& bounds, const text_style& style, const bool highlighted) const -> void
    {
        this->buttons_.Text(text, bounds, this->Style(style, true), highlighted);
    }

    auto config_controls::IconId(const char* id) -> icon_glyph
    {
        if (id != nullptr && std::strcmp(id, "casting") == 0) return icon_glyph::castbar;
        constexpr const char* names[]{"player", "target", "enemylist", "experience", "notifications", "container", "gamehud", "chat", "chat2", "compass", "world", "commands", "reset", "general", "party", "inventory", "equipment", "extras", "gil", "readycheck", "castbar", "castcost"};
        if (id != nullptr)
            for (size_t i = 0; i < std::size(names); ++i)
                if (std::strcmp(id, names[i]) == 0) return static_cast<icon_glyph>(i);
        return icon_glyph::unknown;
    }

    auto config_controls::Icon(const rect& bounds, const char* id, const D3DCOLOR color) const -> void
    {
        const float size = std::max(0.0f, std::min({22.0f, bounds.width - 8, bounds.height - 4}));
        this->design_.DrawIcon({bounds.x + (bounds.width - size) * 0.5f, bounds.y + (bounds.height - size) * 0.5f, size, size}, IconId(id), color);
    }

    auto config_controls::Tooltip(const char* text, const rect& anchor, const D3DVIEWPORT8& viewport) const -> void
    {
        const auto& theme = this->design_.GetTheme();
        auto style = theme.body_text;
        style.scale = 0.78f;
        style = this->Style(style);
        const auto bounds = config_layout::Tooltip(anchor, std::ceil(this->design_.MeasureText(text, style)) + 20, viewport);
        this->design_.FillRect(bounds, this->design_.GetTheme().colors.chrome | 0xFF000000);
        this->Panel(bounds);
        this->Text(text, {bounds.x + 8.0f, bounds.y, bounds.width - 16.0f, bounds.height}, style);
    }

    auto config_controls::Option(const rect& row, const module_option& option, const bool hovered, const bool activeColor, const slider_value_input* input, const text_input* textInput, const bool dropdownOpen) const -> void
    {
        if (!option.available)
        {
            this->Row(row, option.label, option.description, row.width - 8.0f, hovered, this->design_.GetTheme().caption_text);
            return;
        }
        if (option.kind == module_option_kind::toggle && option.toggle != nullptr && option.submodules != nullptr)
            this->ToggleGroupRow(row, option, hovered);
        else if (option.kind == module_option_kind::toggle && option.toggle != nullptr)
            this->ToggleRow(row, option.label, option.description, *option.toggle, hovered);
        else if (option.kind == module_option_kind::color && option.color != nullptr)
            this->ColorRow(row, option.label, option.description, *option.color, activeColor, hovered);
        else if (option.kind == module_option_kind::slider && option.slider != nullptr)
            this->SliderRow(row, option, hovered, input);
        else if ((option.kind == module_option_kind::choice || option.kind == module_option_kind::dropdown) && option.choice != nullptr)
            this->ChoiceRow(row, option, hovered, dropdownOpen);
        else if (option.kind == module_option_kind::color_override && option.color != nullptr && option.toggle != nullptr)
        {
            const auto source = config_layout::OverrideSource(row), swatch = config_layout::OverrideSwatch(row);
            this->Row(row, option.label, option.description, source.x - row.x - 6, hovered, this->design_.GetTheme().body_text);
            auto style = this->design_.GetTheme().caption_text; style.scale = 0.65f; style.alignment = text_alignment::center;
            this->Button(source, *option.toggle ? "XIUI" : "Game", style, *option.toggle || hovered);
            this->design_.FillRect(swatch, *option.color);
            this->design_.BorderRect(swatch, *option.toggle || activeColor ? this->design_.GetTheme().colors.control_selected : this->design_.GetTheme().colors.border);
        }
        else if (option.kind == module_option_kind::text_input && option.text != nullptr)
        {
            const auto field = config_layout::TextValue(row);
            this->Row(row, option.label, option.description, field.x - row.x - 6, hovered, this->design_.GetTheme().body_text);
            if (textInput != nullptr) this->TextInput(field, *textInput);
            else
            {
                this->buttons_.Face(field, hovered);
                auto style = this->design_.GetTheme().body_text; style.scale = 0.74f; style.alignment = text_alignment::left;
                this->Text(option.text->empty() ? "Click to edit" : option.text->c_str(), {field.x + 4, field.y, field.width - 8, field.height}, style);
            }
        }
        else if (option.kind == module_option_kind::action)
        {
            const auto button = config_layout::ChoiceButton(row);
            this->Row(row, option.label, option.description, button.x - row.x - 6, hovered, this->design_.GetTheme().body_text);
            auto style = this->design_.GetTheme().body_text;
            style.scale = 0.76f;
            style.alignment = text_alignment::center;
            if (option.action_icon != icon_glyph::unknown)
            {
                this->buttons_.Face(button, hovered && option.available);
                const auto color = option.available ? this->design_.GetTheme().colors.text : this->design_.GetTheme().colors.muted;
                this->design_.DrawIcon({button.x + 5, button.y + (button.height - 16) * 0.5f, 16, 16}, option.action_icon, color);
                style.top_color = style.bottom_color = color;
                this->ButtonText(option.action_text, {button.x + 24, button.y, button.width - 27, button.height}, style, hovered && option.available);
            }
            else this->Button(button, option.action_text != nullptr ? option.action_text : "Run", style, hovered);
        }
    }

    auto config_controls::Row(const rect& bounds, const char* label, const char* description, const float labelWidth, const bool hovered, text_style labelStyle) const -> void
    {
        const auto& theme = this->design_.GetTheme();
        if (hovered)
            this->design_.FillRect(bounds, theme.colors.selected);
        labelStyle.scale = 0.78f;
        this->Text(label, {bounds.x + 2, bounds.y + 3, std::max(0.0f, labelWidth - 6), 24}, labelStyle);
        auto descriptionStyle = theme.caption_text;
        descriptionStyle.scale = 0.68f;
        this->Text(description, config_layout::Description(bounds), descriptionStyle);
    }

    auto config_controls::ToggleRow(const rect& row, const char* label, const char* description, const bool enabled, const bool hovered) const -> void
    {
        const auto& theme = this->design_.GetTheme();
        this->Row(row, label, description, row.width - 46.0f, hovered, theme.body_text);
        const rect control{row.x + row.width - 42, row.y + 3, 36, 24};
        auto style = theme.caption_text;
        style.alignment = text_alignment::center;
        style.scale = 0.62f;
        this->Button(control, enabled ? "ON" : "OFF", style, enabled || hovered);
    }

    auto config_controls::ToggleGroupRow(const rect& row, const module_option& option, const bool hovered) const -> void
    {
        this->ToggleRow(row, option.label, "", *option.toggle, hovered);
        const auto& theme = this->design_.GetTheme();
        auto style = theme.caption_text;
        style.scale = 0.62f;
        this->design_.FillRect({row.x + 6, row.y + 27, 1, 7}, theme.colors.muted);
        this->design_.FillRect({row.x + 6, row.y + 33, 6, 1}, theme.colors.muted);
        this->Text(option.submodules, {row.x + 16, row.y + 25, row.width - 20, 14}, style);
        style.scale = 0.58f;
        this->Text(option.description, {row.x + 16, row.y + 40, row.width - 20, 13}, style);
    }

    auto config_controls::ColorRow(const rect& row, const char* label, const char* description, const D3DCOLOR color, const bool active, const bool hovered) const -> void
    {
        const auto& theme = this->design_.GetTheme();
        this->Row(row, label, description, row.width - 46.0f, hovered, active ? theme.accent_text : theme.caption_text);
        const rect swatch{row.x + row.width - 34, row.y + 5, 28, 20};
        this->design_.FillRect(swatch, color);
        this->design_.BorderRect(swatch, active ? theme.colors.accent : theme.colors.border);
    }

    auto config_controls::SliderRow(const rect& row, const module_option& option, const bool hovered, const slider_value_input* input) const -> void
    {
        const auto& theme = this->design_.GetTheme();
        const rect track = config_layout::SliderTrack(row);
        const float controlX = track.x;
        this->Row(row, option.label, option.description, controlX - row.x - 5.0f, hovered, theme.body_text);

        const auto field = config_layout::SliderValue(row);
        auto valueStyle = theme.caption_text;
        valueStyle.scale = 0.68f;
        valueStyle.alignment = text_alignment::center;
        if (input == nullptr)
            this->Button(field, slider_value_input::Format(*option.slider * option.display_scale, option.step * option.display_scale).c_str(), valueStyle, hovered);
        else
        {
            this->buttons_.Face(field, true);
            this->design_.BorderRect(field, input->Invalid() ? 0xFFFF6060 : theme.colors.control_selected);
            valueStyle = this->Style(valueStyle);
            valueStyle.alignment = text_alignment::left;
            const rect textBounds{field.x + 4, field.y, std::max(1.0f, field.width - 8), field.height};
            const auto& editor = input->Editor();
            const auto& text = editor.Text();
            size_t begin{};
            while (begin < editor.Caret() && this->design_.MeasureText(text.substr(begin, editor.Caret() - begin).c_str(), valueStyle) > textBounds.width - 2) ++begin;
            size_t end = text.size();
            while (end > begin && this->design_.MeasureText(text.substr(begin, end - begin).c_str(), valueStyle) > textBounds.width) --end;
            const auto widthAt = [&](const size_t position) { return this->design_.MeasureText(text.substr(begin, std::clamp(position, begin, end) - begin).c_str(), valueStyle); };
            const auto [selectionBegin, selectionEnd] = editor.Selection();
            if (selectionBegin != selectionEnd)
                this->design_.FillRect({textBounds.x + widthAt(selectionBegin), field.y + 3, widthAt(selectionEnd) - widthAt(selectionBegin), field.height - 6}, 0x665080D0);
            this->design_.DrawText(text.substr(begin, end - begin).c_str(), textBounds, valueStyle);
            if ((::GetTickCount64() / 500) % 2 == 0)
                this->design_.FillRect({textBounds.x + widthAt(editor.Caret()), field.y + 4, 1, field.height - 8}, theme.colors.text);
        }

        const float range = std::max(0.001f, option.maximum - option.minimum);
        const float ratio = std::clamp((*option.slider - option.minimum) / range, 0.0f, 1.0f);
        this->design_.FillRect(track, theme.colors.meter_track);
        this->design_.FillRect({track.x, track.y, track.width * ratio, track.height}, theme.colors.accent_dim);
        this->design_.FillRect({track.x + track.width * ratio - 2.0f, track.y - 4.0f, 4.0f, 12.0f}, theme.colors.accent);
    }

    auto config_controls::ChoiceRow(const rect& row, const module_option& option, const bool hovered, const bool dropdownOpen) const -> void
    {
        const auto& theme = this->design_.GetTheme();
        const auto button = config_layout::ChoiceButton(row);
        this->Row(row, option.label, option.description, button.x - row.x - 6, hovered, theme.body_text);

        const int selected = option.choice_count > 0 ? std::clamp(*option.choice, 0, static_cast<int>(option.choice_count) - 1) : 0;
        const char* value = option.choice_count > 0 && option.choices != nullptr ? option.choices[selected] : "";
        auto valueStyle = theme.accent_text;
        valueStyle.scale = 0.76f;
        valueStyle.alignment = text_alignment::center;
        if (option.kind == module_option_kind::dropdown) this->Dropdown(button, value, dropdownOpen, hovered);
        else if (option.choice_count > 1)
        {
            this->buttons_.Face(button, hovered);
            this->ButtonText(value, config_layout::ChoiceLabel(row), valueStyle, hovered);
            const auto color = hovered ? theme.colors.control_selected : theme.colors.text;
            const auto previous = config_layout::ChoicePrevious(row), next = config_layout::ChoiceNext(row);
            this->design_.DrawIcon({previous.x + 3, previous.y + 4, 16, 16}, icon_glyph::arrow_left, color);
            this->design_.DrawIcon({next.x + 3, next.y + 4, 16, 16}, icon_glyph::arrow_right, color);
        }
        else this->Button(button, value, valueStyle, hovered);
    }
} // namespace xiui::ui
