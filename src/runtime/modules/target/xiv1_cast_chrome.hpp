#pragma once

#include "modules/module.hpp"
#include "ui/game_icon_cache.hpp"
#include "ui/text_fit.hpp"

namespace xiui
{
    inline auto DrawXiv1Cast(const module_render_context& context, const ui::rect& bounds, const char* name,
        float ratio, D3DCOLOR top, D3DCOLOR bottom, ui::meter_style meter, module_text_profile text,
        const ui::game_icon_view& icon = {}) -> void
    {
        const auto& design = context.Design();
        const float size = std::min(52.0f, std::max(20.0f, bounds.height));
        const ui::rect medal{bounds.x + 17, bounds.y, size, size};
        const ui::rect art{medal.x + size * .19f, medal.y + size * .2f, size * .61f, size * .61f};
        if (icon.texture != nullptr) design.DrawTextureDisc(icon.texture, art, icon.uv, 0xFFFFFFFF);
        else design.DrawIcon(art, ui::icon_glyph::castbar, 0xFFE2DDB7);
        if (context.Containers() != nullptr) context.Containers()->DrawXiv1ActionMedallion(design, medal);
        text.style.alignment = ui::text_alignment::left;
        const ui::rect label{medal.x + size + 7, bounds.y + size * .25f,
            std::max(0.0f, bounds.width - size - 25), design.TextHeight(text.style)};
        const auto fitted = ui::FitText(design, name, label.width, design.ResolveHudText(text.style));
        context.DrawText(fitted.c_str(), label, label, text);
        meter.height = 6; meter.bookends = true;
        const ui::rect track{bounds.x + 27, bounds.y + size - 7, std::max(1.0f, bounds.width - 27), 8};
        context.DrawBar(track, ratio, top, bottom, 0, meter);
    }
}
