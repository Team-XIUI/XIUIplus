#include "button_renderer.hpp"
#include "native_ui_assets.hpp"
#include "control_chrome.hpp"

namespace xiui::ui
{
    auto button_renderer::FaceColor(const theme& theme, const bool highlighted, const button_tone tone) -> D3DCOLOR
    {
        if (tone == button_tone::danger) return highlighted ? 0xFFFF9090 : 0xFFCE6B6B;
        if (tone == button_tone::positive) return highlighted ? 0xFF9BDBAB : 0xFF75B887;
        return highlighted ? theme.colors.control_selected : theme.colors.control;
    }

    auto button_renderer::TextStyle(const theme& theme, const text_style& source, const bool highlighted, const button_tone tone) -> text_style
    {
        auto result = source;
        const uint32_t color = tone == button_tone::danger ? (highlighted ? 0xFFFFFFFF : 0xFFFFE6E6) :
            tone == button_tone::positive ? (highlighted ? 0xFFFFFFFF : 0xFFE6FFEC) : highlighted ? theme.colors.control_selected : theme.colors.text;
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

    auto button_renderer::DrawLabel(const design_system& design, const char* text, const rect& bounds, const text_style& style) -> void
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

    auto button_renderer::Face(const rect& bounds, const bool highlighted, const button_tone tone) const -> void
    {
        if (this->design_.IsXiv1Theme())
        {
            if (bounds.width <= 0 || bounds.height <= 0) return;
            const float radius = std::min(bounds.width, bounds.height) * .5f;
            const float step = std::min(.75f, radius / 5);
            const auto layer = [&](const float inset, const D3DCOLOR color, const float yOffset = 0) {
                this->design_.FillRoundedRect({bounds.x + inset, bounds.y + inset + yOffset,
                    std::max(0.0f, bounds.width - inset * 2), std::max(0.0f, bounds.height - inset * 2)},
                    color, std::max(0.0f, radius - inset));
            };
            layer(0, 0x7005060D, 1);
            layer(0, 0xFF3C4450);
            layer(step, ControlBorderColor(this->design_, highlighted));
            layer(step * 2, 0xFF303844);
            layer(step * 3, highlighted ? 0xFF7F8B9B : 0xFF566272);
            const auto fill = tone == button_tone::danger ? (highlighted ? 0xFF542E45 : 0xFF301C32) :
                tone == button_tone::positive ? (highlighted ? 0xFF294746 : 0xFF142A31) : highlighted ? 0xFF293A54 : 0xFF10132D;
            layer(step * 4, fill);
            if (highlighted)
            {
                const auto clip = this->design_.Clip({bounds.x, bounds.y + bounds.height * .5f, bounds.width, bounds.height * .5f});
                layer(step * 4, tone == button_tone::normal ? 0xFF1C293F : fill);
            }
            return;
        }
        const auto color = FaceColor(this->design_.GetTheme(), highlighted, tone);
        if (this->assets_ != nullptr) this->assets_->DrawControl(this->design_, bounds, highlighted, color);
        else this->design_.FillRoundedRect(bounds, color, 3);
    }

    auto button_renderer::Text(const char* label, const rect& bounds, const text_style& style, const bool highlighted, const button_tone tone) const -> void
    {
        DrawLabel(this->design_, label, bounds, TextStyle(this->design_.GetTheme(), style, highlighted, tone));
    }

    auto button_renderer::Draw(const rect& bounds, const char* label, const text_style& style, const bool highlighted, const button_tone tone) const -> void
    {
        this->Face(bounds, highlighted, tone);
        this->Text(label, bounds, style, highlighted, tone);
    }
}
