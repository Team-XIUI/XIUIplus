#pragma once

#include "design_system.hpp"

#include <algorithm>
#include <string>

namespace xiui::ui
{
    class native_ui_assets;
    enum class button_tone : uint8_t { normal, danger, positive };

    class button_renderer
    {
    public:
        button_renderer(const design_system& design, const native_ui_assets* assets = nullptr) : design_(design), assets_(assets) {}

        static auto FaceColor(const theme& theme, const bool highlighted, const button_tone tone = button_tone::normal) -> D3DCOLOR;

        static auto TextStyle(const theme& theme, const text_style& source, const bool highlighted, const button_tone tone = button_tone::normal) -> text_style;

        static auto DrawLabel(const design_system& design, const char* text, const rect& bounds, const text_style& style, bool marquee = false) -> void;

        auto Face(const rect& bounds, const bool highlighted, const button_tone tone = button_tone::normal) const -> void;

        auto Text(const char* label, const rect& bounds, const text_style& style, const bool highlighted, const button_tone tone = button_tone::normal, std::optional<bool> marquee = std::nullopt) const -> void;

        auto Draw(const rect& bounds, const char* label, const text_style& style, const bool highlighted, const button_tone tone = button_tone::normal, std::optional<bool> marquee = std::nullopt) const -> void;

    private:
        const design_system& design_;
        const native_ui_assets* assets_{};
    };
}
