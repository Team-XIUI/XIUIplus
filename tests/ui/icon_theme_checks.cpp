#include "modules/general/general_module.hpp"
#include "modules/party/party_module.hpp"
#include "modules/status/status_effects_module.hpp"
#include "ui/icon_themes.hpp"

#include <cstdio>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace
{
    auto Check(const bool value, const char* message) -> void
    { if (!value) throw std::runtime_error{message}; }

    auto Choice(xiui::hud_module& module, const char* label) -> int&
    {
        for (size_t index = 0; index < module.OptionCount(); ++index)
        {
            const auto option = module.Option(index);
            if (option.kind != xiui::module_option_kind::choice || option.choice == nullptr ||
                option.label == nullptr || std::strcmp(option.label, label) != 0) continue;
            for (size_t section = 0; section < module.ConfigSections().size(); ++section)
                if (module.ConfigSectionContains(section, index)) return *option.choice;
        }
        throw std::runtime_error{"icon theme choice is unreachable"};
    }
}

auto RunIconThemeChecks(void) -> bool
{
    using namespace xiui;
    try
    {
        Check(ui::StatusArtTheme(33, 0, 1) == 1 && ui::StatusArtTheme(33, 1, 3) == 0 &&
            ui::StatusArtTheme(33, 3, 1) == 2 && ui::StatusArtTheme(33, 4, 0) == 3,
            "global and explicit list themes select stable catalog entries");
        Check(ui::StatusArtTheme(1000, 0, 3) == 0 && ui::StatusArtTheme(1024, 4, 3) == 0 &&
            ui::StatusArtTheme(33, 0, 99) == 0 && ui::StatusArtTheme(33, 99, 3) == 0 &&
            ui::StatusArtTheme(33, std::numeric_limits<int>::min(), 3) == 0 && ui::JobIconTheme(99) == 0,
            "missing effects and unknown saved choices use native default art");
        Check(ui::PetPortraitIndex("Carbuncle", 0) == 0 && ui::PetPortraitIndex("Cait Sith", 0) == 12 &&
            ui::PetPortraitIndex("Air Spirit", 0) == 16 && ui::PetPortraitIndex("Wind Spirit", 0) == 16 &&
            ui::PetPortraitIndex("Custom Wyvern", 3) == 22 && ui::PetPortraitIndex("CourierCarrie", 1) == 23,
            "native avatar/spirit names and custom-named pet categories resolve real portraits");
        Check(ui::PetPortraitIndex("Custom Automaton", 4) == -1 && ui::PetPortraitIndex("Carbuncle", 2) == -1 &&
            ui::PetPortraitFallbackJob(0) == 15 && ui::PetPortraitFallbackJob(2) == 9 &&
            ui::PetPortraitFallbackJob(3) == 14 && ui::PetPortraitFallbackJob(4) == 18 && ui::PetPortraitFallbackJob(99) == 0,
            "missing pet artwork uses only an appropriate category job fallback");

        ui::game_icon_cache cache;
        cache.SetThemes(3, 3);
        Check(cache.StatusIcon(33).texture == nullptr && cache.JobAtlas() == nullptr,
            "unavailable themed and native textures fall back safely without a device");
        Check(cache.PetPortrait("Carbuncle", 0).texture == nullptr && cache.PetPortrait("Unknown", 4, false).texture == nullptr,
            "pet portrait and disabled fallback remain safe without a device");

        auto saved = settings_store::FromIniText("");
        general_module general;
        Check(general.StatusIconTheme() == 0 && general.JobIconTheme() == 0,
            "Native statuses and XIVParty jobs remain the initial global themes");
        Choice(general, "Status icon theme") = 2;
        Choice(general, "Job icon theme") = 3;
        general.Save(saved);
        general_module loadedGeneral; loadedGeneral.Load(saved);
        Check(loadedGeneral.StatusIconTheme() == 2 && loadedGeneral.JobIconTheme() == 3,
            "global status and job themes persist independently");
        saved.WriteInt("appearance.icons", "status_theme", 91);
        loadedGeneral.Load(saved); loadedGeneral.Save(saved);
        Check(saved.ReadInt("appearance.icons", "status_theme", 0) == 91 &&
            ui::StatusArtTheme(33, 0, loadedGeneral.StatusIconTheme()) == 0,
            "unavailable saved choices survive native fallback");

        ui::status_strip_settings strip;
        strip.size = 24; strip.columns = 3; strip.offset_x = 17;
        const auto before = ui::LayoutStatusStrip(8, {10, 20, 300, 0}, strip);
        strip.icon_theme = 4; strip.Save(saved, "test.strip");
        ui::status_strip_settings loadedStrip; loadedStrip.Load(saved, "test.strip");
        const auto after = ui::LayoutStatusStrip(8, {10, 20, 300, 0}, loadedStrip);
        Check(loadedStrip.icon_theme == 4 && before.bounds.x == after.bounds.x && before.height == after.height &&
            before.columns == after.columns && before.size == after.size,
            "status theme persistence preserves strip geometry");

        status_effects_module buffs, debuffs{true};
        Choice(buffs, "Status icon theme") = 2;
        Choice(debuffs, "Status icon theme") = 4;
        buffs.Save(saved); debuffs.Save(saved);
        status_effects_module loadedBuffs, loadedDebuffs{true};
        loadedBuffs.Load(saved); loadedDebuffs.Load(saved);
        Check(Choice(loadedBuffs, "Status icon theme") == 2 && Choice(loadedDebuffs, "Status icon theme") == 4,
            "standalone Buffs and Debuffs keep independent status themes");

        party_module party;
        Choice(party, "Status icon theme") = 3;
        const auto partyBounds = party.State().bounds;
        party.Save(saved);
        party_module loadedParty; loadedParty.Load(saved);
        Check(Choice(loadedParty, "Status icon theme") == 3 && loadedParty.State().bounds.x == partyBounds.x &&
            loadedParty.State().bounds.width == partyBounds.width, "combined Party theme preserves list placement");
        Choice(loadedParty, "Status order") = 3;
        Choice(loadedParty, "Selected status row") = 0; Choice(loadedParty, "Status icon theme") = 2;
        Choice(loadedParty, "Selected status row") = 1; Choice(loadedParty, "Status icon theme") = 4;
        loadedParty.Save(saved);
        party_module splitParty; splitParty.Load(saved);
        Choice(splitParty, "Selected status row") = 0;
        Check(Choice(splitParty, "Status icon theme") == 2, "Party buff row theme persists independently");
        Choice(splitParty, "Selected status row") = 1;
        Check(Choice(splitParty, "Status icon theme") == 4, "Party debuff row theme persists independently");
        Choice(splitParty, "Status order") = 0;
        Check(Choice(splitParty, "Status icon theme") == 3, "split row changes retain the combined Party preference");
        return true;
    }
    catch (const std::exception& error)
    {
        std::printf("FAIL: icon themes: %s\n", error.what());
        return false;
    }
}
