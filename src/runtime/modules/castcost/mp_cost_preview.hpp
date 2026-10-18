#pragma once

#include "cast_cost_source.hpp"
#include "modules/module.hpp"

#include <algorithm>
#include <array>

namespace xiui
{
    inline auto MPPreviewRanges(const selected_spell_mp_cost& preview, const uint32_t current, const uint32_t maximum,
        const uint32_t playerId, const uint64_t now, const ui::theme& theme) -> std::array<ui::meter_range, 1>
    {
        if (!preview.Fresh(now) || preview.player_id != playerId || maximum == 0 || preview.cost == 0) return {};
        const auto available = std::min(current, maximum);
        const auto remaining = available - std::min(available, preview.cost);
        const auto color = current >= preview.cost ? theme.colors.accent : D3DCOLOR{0xFFFF8C8C};
        return {{{static_cast<float>(remaining) / maximum, static_cast<float>(available) / maximum,
            MeterGradientTop(color), (color & 0xFFFFFF) | 0xD0000000}}};
    }
} // namespace xiui
