#pragma once

#include "modules/module.hpp"
#include "combat/combat_source.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace xiui::ui
{
    inline auto StatusStripColumns(const float width) -> size_t { return static_cast<size_t>(std::max(1.0f, std::floor((width + 2) / 22))); }

    inline auto StatusStripHeight(const size_t count, const float width) -> float
    { return count == 0 ? 0 : static_cast<float>((count + StatusStripColumns(width) - 1) / StatusStripColumns(width)) * 34 + 2; }

    inline auto DrawStatusStrip(const std::span<const combat_status> statuses, const rect& bounds, const uint64_t now,
        const module_text_profile& profile, const module_render_context& context) -> void
    {
        const auto columns = StatusStripColumns(bounds.width);
        auto caption = profile;
        caption.placement = 0; caption.offset_x = caption.offset_y = 0;
        caption.style.alignment = text_alignment::center;
        caption.style.scale = std::clamp(caption.style.scale * 0.72f, 0.5f, 0.7f);
        for (size_t i = 0; i < statuses.size(); ++i)
        {
            const auto& status = statuses[i];
            const rect icon{bounds.x + static_cast<float>(i % columns) * 22, bounds.y + static_cast<float>(i / columns) * 34, 20, 20};
            auto* texture = context.Icons() != nullptr ? context.Icons()->Status(status.id) : nullptr;
            if (texture != nullptr) context.Design().DrawTexture(texture, icon, 0xFFFFFFFF);
            else
            {
                char id[12]{}; std::snprintf(id, sizeof(id), "%u", status.id);
                context.DrawText(id, icon, icon, caption);
            }
            char timer[24]{};
            if (status.expires > now)
            {
                const auto seconds = static_cast<unsigned>((status.expires - now + 999) / 1000);
                if (seconds >= 60) std::snprintf(timer, sizeof(timer), "~%um%s", (seconds + 59) / 60, status.uncertain ? "?" : "");
                else std::snprintf(timer, sizeof(timer), "~%u%s", seconds, status.uncertain ? "?" : "");
            }
            else if (status.uncertain) std::snprintf(timer, sizeof(timer), "?");
            if (*timer)
            {
                const rect text{icon.x - 2, icon.y + 20, 24, 14};
                context.DrawText(timer, text, text, caption);
            }
        }
    }
}
