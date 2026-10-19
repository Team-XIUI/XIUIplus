#pragma once

#include "chat_token.hpp"
#include "modules/module.hpp"
#include "ui/native_ui_assets.hpp"

#include <algorithm>

namespace xiui
{
    namespace chat_text_detail
    {
        template<typename Visitor>
        auto Runs(const std::string_view text, Visitor&& visitor) -> void
        {
            size_t begin{};
            for (size_t at = 0; at < text.size();)
            {
                const int cap = ChatCapAt(text, at);
                if (cap < 0) { ++at; continue; }
                if (at > begin) visitor(text.substr(begin, at - begin), -1);
                visitor(text.substr(at, 3), cap);
                at += 3; begin = at;
            }
            if (begin < text.size()) visitor(text.substr(begin), -1);
        }
    }

    inline auto PlainChatText(const std::string_view text) -> std::string
    {
        std::string result;
        chat_text_detail::Runs(text, [&result](const auto run, const int cap) {
            if (cap < 0) result += run;
            else result += cap == 0 ? '{' : '}';
        });
        return result;
    }

    inline auto MeasureChatText(const ui::design_system& design, const std::string_view text, const ui::text_style& style) -> float
    {
        float width{};
        bool first = true;
        chat_text_detail::Runs(text, [&](const auto run, const int cap) {
            if (!first) width += style.letter_spacing * style.scale;
            width += cap >= 0 ? 8 * style.scale : design.MeasureText(std::string{run}.c_str(), style);
            first = false;
        });
        return width;
    }

    inline auto DrawChatText(const module_render_context& context, const std::string_view text, const ui::rect& bounds, const module_text_profile& profile) -> void
    {
        if (!profile.visible) return;
        if (text.find(chat_cap_open) == std::string_view::npos && text.find(chat_cap_close) == std::string_view::npos)
        { context.DrawText(std::string{text}.c_str(), bounds, bounds, profile); return; }
        auto box = bounds;
        auto draw = profile;
        box.x += profile.offset_x; box.y += profile.offset_y;
        if (profile.placement != 0)
        {
            box.x += 4; box.width = std::max(0.0f, box.width - 8);
            draw.style.alignment = profile.placement == 1 ? ui::text_alignment::left : profile.placement == 2 ? ui::text_alignment::center : ui::text_alignment::right;
        }
        const float width = MeasureChatText(context.Design(), text, draw.style);
        const float alignment = draw.style.alignment == ui::text_alignment::center ? 0.5f : draw.style.alignment == ui::text_alignment::right ? 1.0f : 0.0f;
        float x = box.x + std::max(0.0f, box.width - width) * alignment;
        draw.placement = 0; draw.offset_x = draw.offset_y = 0; draw.style.alignment = ui::text_alignment::left;
        const float scale = draw.style.scale;
        const float capY = box.y + std::max(0.0f, (box.height - 18 * scale) * 0.5f) + scale;
        bool first = true;
        chat_text_detail::Runs(text, [&](const auto run, const int cap) {
            if (!first) x += draw.style.letter_spacing * scale;
            const float available = std::max(0.0f, box.x + box.width - x);
            if (cap >= 0)
            {
                if (available >= 8 * scale)
                {
                    auto* assets = context.NativeAssets();
                    if (assets == nullptr || !assets->DrawAutoTranslateCap(context.Design(), cap != 0, x, capY, scale, static_cast<float>(draw.style.top_color >> 24) / 255))
                    { const ui::rect fallback{x, box.y, available, box.height}; context.DrawText(cap == 0 ? "{" : "}", fallback, fallback, draw); }
                }
                x += 8 * scale;
            }
            else
            {
                const ui::rect at{x, box.y, available, box.height};
                context.DrawText(std::string{run}.c_str(), at, at, draw);
                x += context.Design().MeasureText(std::string{run}.c_str(), draw.style);
            }
            first = false;
        });
    }
}
