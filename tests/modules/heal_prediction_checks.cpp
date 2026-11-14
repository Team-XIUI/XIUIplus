#include "modules/healing/heal_prediction.hpp"
#include "modules/player/player_module.hpp"
#include "modules/party/party_module.hpp"
#include "modules/target/target_module.hpp"
#include "ui/meter_skin.hpp"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>

auto RunHealPredictionChecks(void) -> bool
{
    using namespace xiui;
    const auto check = [](const bool pass, const char* message) {
        if (!pass) std::fprintf(stderr, "Heal prediction: %s\n", message);
        return pass;
    };
    const auto close = [](float a, float b) { return std::abs(a - b) < .0001f; };
    healing_stats stats{100, 80, 300};
    constexpr uint32_t expected[]{48, 127, 274, 499, 668, 829, 95, 198, 397, 690, 850};
    for (uint32_t spell = 1; spell <= 11; ++spell)
        if (!check(EstimateHeal(spell, stats) == expected[spell - 1], "Cure and Curaga reference estimates")) return false;
    if (!check(EstimateHeal(4, stats, 50) == 748 && EstimateHeal(0, stats) == 0 && EstimateHeal(23, stats) == 0 &&
        EstimateHeal(4, stats, std::numeric_limits<float>::quiet_NaN()) == 0, "potency rounding and unsupported inputs")) return false;
    stats.rapture = true; stats.divine_seal = true;
    if (!check(EstimateHeal(4, stats, 50) == 2244, "native healing buffs multiply the estimate")) return false;

    player_cast_source cast;
    combat_action action{}; action.actor = 123; action.category = 8; action.parameter = 0x6163; action.target_count = 1;
    action.targets[0].id = 456; action.targets[0].parameter = 4;
    cast.Observe(action, 123, nullptr, 1000);
    if (!check(cast.State().target_id == 456 && HealRecipient(cast.State(), 4, 789) == 456, "casting locks the actual recipient after retargeting")) return false;
    action.parameter = 0x7073; cast.Observe(action, 123, nullptr, 1100);
    if (!check(HealRecipient(cast.State(), 4, 789) == 0, "interruption clears the forecast immediately")) return false;
    action.category = 4; action.parameter = 4; cast.Observe(action, 123, nullptr, 1200);
    if (!check(!cast.State().Active() && HealRecipient(cast.State(), 4, 789) == 789 && HealRecipient(cast.State(), 23, 789) == 0,
        "completion releases the lock and non-heals stay hidden")) return false;

    heal_prediction prediction{456, 600, 500, 1000, 50};
    const auto range = HealPreviewRange(prediction, 0xD066FF99);
    const ui::rect track{10, 20, 200, 8};
    const auto forward = ui::LayoutMeterRange(track, range, .5f), reverse = ui::LayoutMeterRange(track, range, .5f, true);
    if (!check(close(range.start, .5f) && close(range.end, 1) && close(forward.x, 110) && close(forward.width, 100) &&
        close(reverse.x, 10) && close(reverse.width, 100), "forecast extends beyond current HP, clips overheal and mirrors direction")) return false;
    const auto existing = ui::LayoutMeterRange(track, {.25f, .75f, 0, 0}, .5f);
    if (!check(close(existing.x, 60) && close(existing.width, 50), "existing MP ranges retain their fill clipping")) return false;
    prediction.maximum = 0;
    if (!check(HealPreviewRange(prediction, 0).end == 0, "unknown max HP suppresses prediction")) return false;
    prediction.maximum = prediction.hp;
    if (!check(HealPreviewRange(prediction, 0).end == 0, "full HP has no useful-healing segment")) return false;

    prediction = {456, 600, 900, 1000, 90};
    const auto useful = HealPreviewRange(prediction, 0xD066FF99), wasted = OverhealPreviewRange(prediction, 0xA0FF80C0);
    const auto pink = ui::LayoutMeterRange(track, wasted, .9f), pinkReverse = ui::LayoutMeterRange(track, wasted, .9f, true);
    if (!check(close(useful.start, .9f) && close(useful.end, 1) && close(wasted.start, .4f) && close(wasted.end, useful.start) &&
        close(pink.width, 100) && close(pinkReverse.x, 30) && close(pinkReverse.width, 100) &&
        wasted.bottom == 0xA0FF80C0 && (wasted.top >> 24) == 0xA0,
        "a 600 heal with 100 HP missing shows 100 useful HP and 500 overheal without covering the useful segment")) return false;
    prediction.amount = 100;
    if (!check(OverhealPreviewRange(prediction, 0).end == 0, "an exact-fill heal produces no pink segment")) return false;
    prediction.amount = 600; prediction.hp = 1000; prediction.percent = 100;
    const auto full = OverhealPreviewRange(prediction, 0xD0FF80C0);
    if (!check(close(full.start, .4f) && close(full.end, 1) && HealPreviewRange(prediction, 0).end == 0,
        "a heal at full HP remains visible entirely as overheal")) return false;
    prediction.amount = UINT32_MAX;
    const auto large = OverhealPreviewRange(prediction, 0xD0FF80C0);
    if (!check(close(large.start, 0) && close(large.end, 1), "large heals remain bounded by the HP bar without integer overflow")) return false;
    prediction.maximum = 0;
    if (!check(OverhealPreviewRange(prediction, 0).end == 0, "unknown maximum HP never fabricates overheal")) return false;
    prediction.maximum = 1000; prediction.hp = 0;
    if (!check(OverhealPreviewRange(prediction, 0).end == 0, "dead targets have no overheal segment")) return false;
    prediction.hp = 1000;
    if (!check(OverhealPreviewRange(prediction, 0, std::numeric_limits<float>::quiet_NaN()).end == 0,
        "invalid displayed ratios suppress overheal geometry")) return false;

    heal_prediction_source source;
    source.Load(settings_store::FromIniText("[healing_prediction]\nbonus=37\n"));
    player_module player; party_module party; target_module target;
    player.SetHealPrediction(&source); party.SetHealPrediction(&source); target.SetHealPrediction(&source);
    const auto theme = ui::theme::CreateDefault();
    player.ResetDefaults(theme); party.ResetDefaults(theme); target.ResetDefaults(theme);
    target.SelectConfigView(3);
    for (auto* module : std::array<hud_module*, 3>{&player, &party, &target})
    {
        size_t found{};
        for (size_t index = 0; index < module->OptionCount(); ++index)
        {
            auto option = module->Option(index);
            if (option.label == nullptr) continue;
            if (std::strcmp(option.label, "Heal prediction") != 0 && std::strcmp(option.label, "Heal prediction color") != 0 &&
                std::strcmp(option.label, "Healing potency bonus (%)") != 0 && std::strcmp(option.label, "Overheal prediction") != 0 &&
                std::strcmp(option.label, "Overheal color") != 0) continue;
            size_t sections{};
            for (size_t section = 0; section < module->ConfigSections().size(); ++section) sections += module->ConfigSectionContains(section, index);
            if (!check(sections == 1 && option.available, "preview controls are reachable in one settings group after reset")) return false;
            ++found;
        }
        if (!check(found == 5, "all three surfaces expose healing, overheal, both colors and shared potency controls")) return false;
    }
    const auto store = settings_store::FromIniText("");
    source.Save(store);
    heal_preview_settings settings; settings.enabled = false; settings.color = 0xA0123456;
    if (!check(settings.overheal && settings.overheal_color == 0xFFFF5FC8, "overheal starts enabled with an opaque saturated pink default")) return false;
    settings.overheal = false; settings.overheal_color = 0x9FFF44AA;
    settings.Save(store, "party");
    heal_preview_settings restored; restored.Load(store, "party");
    if (!check(store.ReadFloat("healing_prediction", "bonus", 0) == 37 && !restored.enabled && restored.color == 0xA0123456 &&
        !restored.overheal && restored.overheal_color == 0x9FFF44AA,
        "shared potency and per-bar appearance persist")) return false;
    restored.Load(settings_store::FromIniText("[party]\nheal_prediction_overheal_color=0xD0FF80C0\n"), "party");
    if (!check(restored.overheal_color == 0xFFFF5FC8, "the former default upgrades to opaque saturated pink")) return false;
    restored.Load(settings_store::FromIniText("[party]\nheal_prediction_overheal_color=0x9FFF44AA\n"), "party");
    if (!check(restored.overheal_color == 0x9FFF44AA, "upgrading preserves customized color and alpha")) return false;
    const auto upgraded = settings_store::FromIniText("");
    settings.overheal_color = 0xD0FF80C0;
    settings.Save(upgraded, "party");
    restored.Load(upgraded, "party");
    if (!check(restored.overheal_color == 0xD0FF80C0, "later color choices survive the one-time migration")) return false;
    source.Update(nullptr, party_source{}, cast, cast_cost_source{}, 1400);
    if (!check(source.Primary(1400) == nullptr, "unavailable native data hides predictions")) return false;
    std::puts("Heal formulas, recipient locking, useful/overheal forecasts and shared settings checks passed.");
    return true;
}
