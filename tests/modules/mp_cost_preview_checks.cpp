#include "modules/castcost/mp_cost_preview.hpp"
#include "ui/meter_skin.hpp"
#include "modules/player/player_module.hpp"
#include "modules/party/party_module.hpp"
#include "modules/general/general_module.hpp"

#include <cmath>
#include <cstring>

auto RunMpCostPreviewChecks(void) -> bool
{
    using namespace xiui;
    const auto close = [](const float actual, const float expected) { return std::abs(actual - expected) < 0.0001f; };
    const auto empty = [](const auto& ranges) { return ranges[0].end <= ranges[0].start; };
    const auto theme = ui::theme::CreateDefault();
    constexpr uint64_t now = 10000;
    selected_spell_mp_cost selected{true, true, 17, 75, now};
    const auto affordable = MPPreviewRanges(selected, 200, 400, 17, now, theme);
    if (!close(affordable[0].start, 0.3125f) || !close(affordable[0].end, 0.5f)) return false;
    selected.cost = 150;
    const auto insufficient = MPPreviewRanges(selected, 100, 400, 17, now, theme);
    if (!close(insufficient[0].start, 0) || !close(insufficient[0].end, 0.25f) ||
        (insufficient[0].bottom & 0xFFFFFF) != 0xFF8C8C) return false;
    if (!empty(MPPreviewRanges(selected, 100, 0, 17, now, theme)) ||
        !empty(MPPreviewRanges(selected, 100, 400, 18, now, theme)) ||
        !empty(MPPreviewRanges(selected, 100, 400, 17, now + 251, theme)) ||
        !empty(MPPreviewRanges(selected, 100, 400, 17, now - 1, theme))) return false;
    selected.cost = 0;
    if (!selected.Fresh(now) || !empty(MPPreviewRanges(selected, 100, 400, 17, now, theme))) return false;
    selected.cost = 75; selected.selected = false;
    if (!empty(MPPreviewRanges(selected, 100, 400, 17, now, theme))) return false;
    selected.selected = true; selected.available = false;
    if (!empty(MPPreviewRanges(selected, 100, 400, 17, now, theme))) return false;

    const ui::rect bounds{10, 20, 204, 12};
    const auto themed = ui::LayoutMeterSkin(bounds, {}).content;
    const auto clipped = ui::LayoutMeterRange(themed, affordable[0], 0.4f);
    if (clipped.x < themed.x || clipped.y < themed.y || clipped.x + clipped.width > themed.x + themed.width * 0.4001f ||
        clipped.y + clipped.height > themed.y + themed.height) return false;
    const auto modern = ui::LayoutMeterRange(bounds, insufficient[0], 1);
    const auto mirrored = ui::LayoutMeterRange(bounds, insufficient[0], 1, true);
    if (!close(modern.width, bounds.width * 0.25f) || !close(mirrored.x + mirrored.width, bounds.x + bounds.width)) return false;

    const auto partyToggle = [](party_module& party) -> bool* {
        for (size_t index = 0; index < party.OptionCount(); ++index)
        {
            const auto option = party.Option(index);
            if (option.label != nullptr && std::strcmp(option.label, "Spell MP cost preview") == 0) return option.toggle;
        }
        return nullptr;
    };
    player_module player; party_module party; general_module general;
    const auto legacy = settings_store::FromIniText("[resources]\nmp_cost_preview=0\n");
    player.Load(legacy); party.Load(legacy);
    auto* partyPreview = partyToggle(party);
    if (partyPreview == nullptr || !*partyPreview || !player.Settings().mp_cost_preview) return false;
    for (const auto& section : general.ConfigSections())
        if (std::strcmp(section.name, "Resources") == 0) return false;
    auto settings = settings_store::FromIniText("");
    player.Settings().mp_cost_preview = false;
    player.Save(settings); party.Save(settings);
    player_module restoredPlayer; party_module restoredParty;
    restoredPlayer.Load(settings); restoredParty.Load(settings);
    if (restoredPlayer.Settings().mp_cost_preview || !*partyToggle(restoredParty)) return false;
    restoredPlayer.Settings().mp_cost_preview = true; *partyToggle(restoredParty) = false;
    restoredPlayer.Save(settings); restoredParty.Save(settings);
    player.Load(settings); party.Load(settings);
    if (!player.Settings().mp_cost_preview || *partyToggle(party)) return false;
    player.ResetDefaults(theme); party.ResetDefaults(theme);
    if (!player.Settings().mp_cost_preview || !*partyToggle(party)) return false;
    return true;
}
