#pragma once

#include "native_ui_assets.hpp"

#include <algorithm>
#include <string>

namespace xiui::ui
{
    enum class button_tone : uint8_t { normal, danger };

    class button_renderer
    {
    public:
        button_renderer(const design_system& design, const native_ui_assets* assets = nullptr) : design_(design), assets_(assets) {}

        static auto FaceColor(const theme& theme, const bool highlighted, const button_tone tone = button_tone::normal) -> D3DCOLOR
        {
            if (tone == button_tone::danger) return highlighted ? 0xFFFF9090 : 0xFFCE6B6B;
            return highlighted ? theme.colors.control_selected : theme.colors.control;
        }

        static auto TextStyle(const theme& theme, const text_style& source, const bool highlighted, const button_tone tone = button_tone::normal) -> text_style
        {
            auto result = source;
            const uint32_t color = tone == button_tone::danger ? (highlighted ? 0xFFFFFFFF : 0xFFFFE6E6) : highlighted ? theme.colors.control_selected : theme.colors.text;
            result.top_color = ((color >> 24) * (source.top_color >> 24) / 255) << 24;
            result.bottom_color = ((color >> 24) * (source.bottom_color >> 24) / 255) << 24;
            for (unsigned shift = 0; shift < 24; shift += 8)
            {
                const uint32_t channel = (color >> shift) & 255;
                const uint32_t top = (source.top_color >> shift) & 255;
                const uint32_t bottom = (source.bottom_color >> shift) & 255;
                result.top_color |= channel << shift;
                result.bottom_color |= std::min(255u, channel * bottom / std::max(1u, top)) << shift;
            }
            return result;
        }

        static auto DrawLabel(const design_system& design, const char* text, const rect& bounds, const text_style& style) -> void
        {
            if (text == nullptr || bounds.width <= 4) return;
            std::string label{text};
            const float width = bounds.width - 4;
            if (design.MeasureText(text, style) > width)
            {
                const float suffix = design.MeasureText("...", style);
                while (!label.empty() && design.MeasureText(label.c_str(), style) + suffix > width)
                {
                    size_t last = label.size() - 1;
                    while (last > 0 && (static_cast<unsigned char>(label[last]) & 0xC0) == 0x80) --last;
                    label.resize(last);
                }
                label += "...";
            }
            design.DrawText(label.c_str(), {bounds.x + 2, bounds.y, width, bounds.height}, style);
        }

        auto Face(const rect& bounds, const bool highlighted, const button_tone tone = button_tone::normal) const -> void
        {
            const auto color = FaceColor(this->design_.GetTheme(), highlighted, tone);
            if (this->assets_ != nullptr) this->assets_->DrawControl(this->design_, bounds, highlighted, color);
            else this->design_.FillRoundedRect(bounds, color, 3);
        }

        auto Text(const char* label, const rect& bounds, const text_style& style, const bool highlighted, const button_tone tone = button_tone::normal) const -> void
        {
            DrawLabel(this->design_, label, bounds, TextStyle(this->design_.GetTheme(), style, highlighted, tone));
        }

        auto Draw(const rect& bounds, const char* label, const text_style& style, const bool highlighted, const button_tone tone = button_tone::normal) const -> void
        {
            this->Face(bounds, highlighted, tone);
            this->Text(label, bounds, style, highlighted, tone);
        }

    private:
        const design_system& design_;
        const native_ui_assets* assets_{};
    };
}
