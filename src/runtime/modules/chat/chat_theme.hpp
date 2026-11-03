#pragma once

#include "modules/module.hpp"
#include "chat/chat_colors.hpp"
#include "chat/chat_text.hpp"

namespace xiui
{
    inline auto ChatRunWeight(const ui::design_system& design, const bool emphasis) -> ui::font_weight
    { return emphasis ? ui::font_weight::bold : design.IsXiv1Theme() ? ui::font_weight::medium : ui::font_weight::regular; }

    inline auto ChatThemeStyle(const ui::design_system& design, const ui::text_style& style, const bool tab = false) -> ui::text_style
    {
        auto result = design.ResolveHudText(style);
        if (design.IsXiv1Theme())
        {
            result.font = style.ThemeFont(tab ? ui::xiv1_italic_font : ui::xiv1_font);
            if (style.theme_text) result.letter_spacing += .4f;
            result.theme_text = false;
        }
        return result;
    }

    inline auto ChatThemeTracking(const ui::text_style& style) -> float
    { return style.letter_spacing * ui::TextPixelScale(style.scale); }

    inline auto ChatInputStyle(const ui::design_system& design, const ui::text_style& style) -> ui::text_style
    {
        auto result = ChatThemeStyle(design, style);
        result.weight = ChatRunWeight(design, false);
        return result;
    }

    inline auto ChatThemeProfile(const ui::design_system& design, module_text_profile profile) -> module_text_profile
    { profile.style = ChatThemeStyle(design, profile.style); return profile; }

    inline auto ChatBodyProfile(const ui::design_system& design, module_text_profile profile) -> module_text_profile
    {
        if (!design.IsXiv1Theme()) return profile;
        const auto brighten = [](const D3DCOLOR color) {
            const auto lift = [](const uint32_t value) { return (value * 3 + 257) / 4; };
            return (color & 0xFF000000) | (lift((color >> 16) & 255) << 16) |
                (lift((color >> 8) & 255) << 8) | lift(color & 255);
        };
        profile.style.top_color = brighten(profile.style.top_color);
        profile.style.bottom_color = brighten(profile.style.bottom_color);
        return profile;
    }

    inline auto DrawThemedChatText(const module_render_context& context, const std::string_view text,
        const ui::rect& bounds, const module_text_profile& profile) -> void
    { DrawChatText(context, text, bounds, ChatThemeProfile(context.Design(), profile)); }

    inline auto DrawChatTab(const module_render_context& context, const ui::rect& bounds, const char* title,
        const bool selected, module_text_profile profile) -> void
    {
        if (!context.Design().IsXiv1Theme()) { context.DrawButton(bounds, title, selected, profile); return; }
        const auto& design = context.Design();
        const float slope = std::min(5.0f, bounds.width * .1f);
        const auto top = selected ? 0xA06B6E69u : 0x653E4544u;
        const auto bottom = selected ? 0x70505049u : 0x32272E2Du;
        design.GradientRect({bounds.x, bounds.y, bounds.width - slope, bounds.height}, {top, top, bottom, bottom});
        design.DrawTriangle(bounds.x + bounds.width - slope, bounds.y, bounds.x + bounds.width - slope, bounds.y + bounds.height,
            bounds.x + bounds.width, bounds.y + bounds.height, bottom);
        design.FillRect({bounds.x, bounds.y, bounds.width - slope, 1}, selected ? 0xA0C8C4AA : 0x507F857A);
        profile.style = ChatThemeStyle(design, profile.style, true);
        profile.style.alignment = ui::text_alignment::center;
        profile = TintTextProfile(profile, selected ? 0xFFF4F1DD : 0xFFBCBCA9);
        context.DrawText(title, bounds, bounds, profile);
    }
}
