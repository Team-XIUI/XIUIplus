#include "combat/skillchain_source.hpp"
#include "modules/actionbars/actionbars_module.hpp"
#include "modules/magicburst/magic_burst_module.hpp"
#include "modules/phantomroll/phantom_roll_module.hpp"

#include <cstdio>
#include <cstring>
#include <stdexcept>

namespace
{
    auto Check(const bool value, const char* message) -> void { if (!value) throw std::runtime_error{message}; }
    auto Action(const uint32_t id, const uint32_t target = 100, const uint32_t category = 3, const uint32_t message = 103) -> xiui::combat_action
    {
        xiui::combat_action action{}; action.actor = 1; action.category = category; action.parameter = id; action.target_count = 1;
        action.targets[0].id = target; action.targets[0].result_count = 1; action.targets[0].results[0].message = message;
        return action;
    }
    auto ObservedPacket(void) -> std::vector<uint8_t>
    {
        std::vector<uint8_t> bytes(128); size_t position{40};
        const auto write = [&](uint32_t value, const unsigned bits) {
            for (unsigned i = 0; i < bits; ++i, ++position) if (value & (1u << i)) bytes[position / 8] |= static_cast<uint8_t>(1u << (position % 8));
        };
        write(1,32); write(1,6); write(0,4); write(3,4); write(999,32); write(0,32);
        write(100,32); write(1,4); write(0,5); write(42,12); write(0,7); write(0,3);
        write(1000,17); write(103,10); write(0,31); write(1,1); write(7,10); write(500,17); write(288,10); write(0,1);
        bytes.resize((position + 7) / 8); return bytes;
    }

    auto CheckSharedPacket(void) -> void
    {
        auto bytes = ObservedPacket();
        const xiui::combat_action_packet packet{bytes};
        const auto* action = &packet.Action();
        Check(action->has_value() && action->value().actor == 1, "the shared packet decodes a valid action");
        std::fill(bytes.begin(), bytes.end(), 0);
        Check(&packet.Action() == action && packet.Action()->actor == 1, "later consumers reuse the immutable decoded action");
        const xiui::combat_action_packet invalid{bytes};
        Check(!invalid.Action() && !invalid.Action(), "invalid packets remain invalid for every consumer");
    }

    auto CheckCombatSettings(void) -> void
    {
        using namespace xiui;
        const auto legacy = settings_store::FromIniText("[magicburst]\nrules=1\n[phantomroll]\nrules=2\n");
        combat_settings settings;
        settings.Load(legacy);
        Check(settings.Rules() == server_rules::horizon, "shared rules retain the legacy Magic Burst migration precedence");
        settings.Load(settings_store::FromIniText("[phantomroll]\nrules=2\n"));
        Check(settings.Rules() == server_rules::retail, "standalone legacy roll rules do not change the runtime combat default");

        const auto profile = settings_store::FromIniText("[actionbars]\nskillchain_rules=2\n[magicburst]\nrules=1\n[phantomroll]\nrules=0\n");
        settings.Load(profile);
        actionbars_module bars;
        phantom_roll_module rolls;
        bars.SetCombatSettings(&settings);
        rolls.SetCombatSettings(&settings);
        bars.Load(profile);
        rolls.Load(profile);
        Check(bars.SkillchainRules() == server_rules::phoenix && rolls.Rules() == server_rules::phoenix,
            "both modules use the canonical runtime rules without overwriting them during load");

        const auto choice = [](hud_module& module, const char* label) -> int* {
            for (size_t index = 0; index < module.OptionCount(); ++index)
            {
                const auto option = module.Option(index);
                if (option.label != nullptr && std::strcmp(option.label, label) == 0) return option.choice;
            }
            return nullptr;
        };
        auto* barRules = choice(bars, "Skillchain rules");
        auto* rollRules = choice(rolls, "Rules");
        Check(barRules != nullptr && barRules == rollRules, "both configuration surfaces edit the shared settings adapter");
        *barRules = ServerRulesIndex(server_rules::horizon);
        Check(rolls.Rules() == server_rules::horizon, "Action Bars edits reach Phantom Roll immediately");
        *rollRules = ServerRulesIndex(server_rules::phoenix);
        Check(bars.SkillchainRules() == server_rules::phoenix, "Phantom Roll edits reach Action Bars immediately");

        auto saved = settings_store::FromIniText("");
        bars.Save(saved);
        rolls.Save(saved);
        Check(saved.ReadInt("actionbars", "skillchain_rules", -1) == -1 && saved.ReadInt("phantomroll", "rules", -1) == -1,
            "bound modules leave shared persistence to the runtime owner");
        settings.Save(saved);
        Check(saved.ReadInt("actionbars", "skillchain_rules", -1) == 2 && saved.ReadInt("phantomroll", "rules", -1) == 2,
            "shared capture retains the canonical key and compatible roll mirror");
        combat_settings restored;
        restored.Load(settings_store::FromIniText(saved.ToIniText()));
        Check(restored.Rules() == server_rules::phoenix, "shared rules survive preference snapshot round trips");

        const auto theme = ui::theme::CreateDefault();
        bars.ResetDefaults(theme);
        rolls.ResetDefaults(theme);
        Check(bars.SkillchainRules() == server_rules::phoenix && rolls.Rules() == server_rules::phoenix,
            "module resets preserve the shared owner and its value");
        settings.ResetDefaults();
        Check(bars.SkillchainRules() == server_rules::retail && rolls.Rules() == server_rules::retail,
            "the runtime owner resets shared rules once for every consumer");

        phantom_roll_module standalone;
        standalone.Load(legacy);
        Check(standalone.Rules() == server_rules::phoenix, "standalone roll modules retain their legacy preferences");
        standalone.Save(saved);
        Check(saved.ReadInt("phantomroll", "rules", -1) == 2, "standalone roll preferences still save normally");

        settings.Load(settings_store::FromIniText("[actionbars]\nskillchain_rules=-1\n"));
        Check(settings.Rules() == server_rules::retail, "negative saved rules clamp to Retail");
        settings.Load(settings_store::FromIniText("[actionbars]\nskillchain_rules=99\n"));
        Check(settings.Rules() == server_rules::phoenix, "high saved rules retain the existing Phoenix clamp");
        *settings.RulesChoice() = -4;
        Check(settings.Rules() == server_rules::retail && *settings.RulesChoice() == 0,
            "the settings adapter normalizes an invalid choice before exposing it again");
    }
}

auto RunSkillchainChecks(void) -> bool
{
    using namespace xiui;
    try
    {
        CheckSharedPacket();
        CheckCombatSettings();
        skillchain_source source;
        source.Observe(Action(2), 1000);
        Check(source.Find(100, 1000) != nullptr && !source.Find(100, 1000)->Burst(1000) &&
            source.Predict(100, 3, 1, {}, 1001).chain == skillchain_property::impaction,
            "an observed opener predicts a compatible closer without inventing a burst window");
        Check(!source.Predict(101, 3, 1, {}, 1001).Active() && !source.Predict(100, 3, 999, "Unknown weapon skill", 1001).Active(),
            "other targets and unknown properties cannot borrow a prediction");
        const auto packet = ObservedPacket();
        const auto decoded = DecodeCombatAction(packet);
        Check(decoded && decoded->targets[0].results[0].additional_message == 288 &&
            !DecodeCombatAction(std::span{packet}.first(packet.size() - 1)), "the production decoder retains observed skillchain messages and rejects truncation");
        source.Observe(*decoded, 2000);
        const auto* light = source.Find(100, 2000);
        Check(light && light->Burst(2000) && light->chain == skillchain_property::light && light->depth == 1 &&
            light->opens == 5500 && light->closes == 10800, "server-observed Light opens the shared estimated burst and continuation windows");
        Check(source.Predict(100, 4, 144, {}, 2001, 0).burst && !source.Predict(100, 4, 159, {}, 2001, 3).burst &&
            !source.Predict(100, 3, 10, {}, 5499).Active() && source.Predict(100, 3, 10, {}, 5500).chain == skillchain_property::light,
            "Light offers its four burst elements immediately while respecting the continuation delay");
        auto repeated = Action(10); repeated.targets[0].result_count = 2;
        repeated.targets[0].results[1].additional_message = 288;
        source.Observe(repeated, 6000);
        const auto* terminal = source.Find(100, 6000);
        Check(terminal && terminal->chain == skillchain_property::light2 && terminal->depth == 2 &&
            !source.Predict(100, 3, 10, {}, 9500).Active() && source.Predict(100, 4, 144, {}, 9500, 0).burst,
            "a skillchain on a later multi-hit result advances once and terminal Light keeps only its burst window");
        source.Prune(13800); Check(source.Count() == 0, "expiry removes burst and action hints at the same deadline");

        source.SetLocalBuffs(1, 0);
        source.Observe(Action(529, 100, 4, 2), 20000);
        Check(source.Count() == 0, "physical Blue Magic needs a chain-enabling buff to open a resonation");
        source.SetLocalBuffs(1, 2);
        source.Observe(Action(529, 100, 4, 2), 20000);
        Check(source.Find(100, 20000) && source.Predict(100, 3, 17, {}, 20001).chain == skillchain_property::scission,
            "Chain Affinity lets a known Blue Magic property open the same tracker");
        source.Observe(Action(529, 101, 4, 2), 20001);
        Check(!source.Find(101, 20001), "a consumed one-shot Chain Affinity buff does not open a second chain");
        source.SetLocalBuffs(1, 4);
        source.Observe(Action(885, 102, 4, 2), 20002, 3);
        Check(source.Find(102, 20002) && source.Predict(102, 3, 6, {}, 20003).chain == skillchain_property::liquefaction,
            "known Helix II spells use Immanence even though their IDs are above the Blue Magic range");
        source.SetLocalBuffs(1, 4);
        source.Observe(Action(1, 103, 4, 2), 20004, 6);
        Check(!source.Find(103, 20004), "Immanence cannot turn an unknown/healing spell element into a guessed opener");

        source.Clear(); source.Observe(Action(3), 30000);
        Check(!source.Predict(100, 3, 37, {}, 30001).Active(), "Retail uses the pinned Retail property set");
        source.SetRules(server_rules::horizon); Check(source.Count() == 0, "changing rules invalidates existing observations");
        source.Observe(Action(3), 30000);
        Check(source.Predict(100, 3, 37, {}, 30001).chain == skillchain_property::transfixion,
            "Horizon applies its property overlay before making the shared prediction");
        source.Observe(Action(999), 30002); Check(source.Count() == 0, "an unknown successful weapon skill clears stale resonation properties");
        source.Observe(Action(2), 30003); source.Observe(Action(1, 100, 3, 6), 30004);
        Check(source.Count() == 0, "observed target death clears its chain");
        for (uint32_t id = 1; id <= skillchain_source::capacity + 8; ++id) source.Observe(Action(2, 1000 + id), 40000 + id);
        Check(source.Count() == skillchain_source::capacity && !source.Find(1001, 40136), "tracked targets have a bounded capacity with oldest-first eviction");
        source.Update(nullptr, 50000); Check(source.Count() == 0 && source.Target() == 0, "logout/unavailable client state clears tracking identity");

        auto store = settings_store::FromIniText("[magicburst]\nenabled=0\npredictions=1\nrules=1\nopacity=0.6\n[layout.magicburst]\nx=123\ny=234\nwidth=160\n[text.magicburst.timer]\noffset_x=9\noffset_y=-4\n");
        magic_burst_module module; const auto theme = ui::theme::CreateDefault(); module.ApplyTextDefaults(theme); module.SetSource(&source); module.Load(store);
        actionbars_module bars; bars.ApplyTextDefaults(theme); bars.Load(store);
        Check(!module.State().enabled && bars.SkillchainPredictionsEnabled() && bars.SkillchainRules() == server_rules::horizon,
            "Action Bars preserves legacy prediction preferences independently of the hidden burst HUD");
        size_t predictionIndex = bars.OptionCount(), rulesIndex = bars.OptionCount(), hudIndex = bars.OptionCount();
        for (size_t index = 0; index < bars.OptionCount(); ++index)
        {
            const auto option = bars.Option(index);
            if (option.label && std::strcmp(option.label, "Action predictions") == 0) predictionIndex = index;
            if (option.label && std::strcmp(option.label, "Skillchain rules") == 0) rulesIndex = index;
            if (option.label && std::strcmp(option.label, "Magic Burst HUD") == 0) hudIndex = index;
        }
        Check(predictionIndex < bars.OptionCount() && rulesIndex < bars.OptionCount() && hudIndex < bars.OptionCount(),
            "Action Bars exposes predictions, rules and a burst HUD settings shortcut");
        auto* predictions = bars.Option(predictionIndex).toggle; auto* rules = bars.Option(rulesIndex).choice;
        for (size_t view = 0; view <= action_bar_count; ++view)
        {
            bars.SelectConfigView(view);
            const auto section = bars.ConfigSections().size() - 1;
            Check(std::strcmp(bars.ConfigSections()[section].name, "Skillchains") == 0 &&
                bars.ConfigSectionContains(section, predictionIndex) && bars.ConfigSectionContains(section, rulesIndex) &&
                bars.ConfigSectionContains(section, hudIndex) && bars.Option(predictionIndex).toggle == predictions && bars.Option(rulesIndex).choice == rules,
                "every hotbar and Crossbar route to the same shared Skillchains controls");
        }
        bars.OnOptionChanged(hudIndex);
        Check(bars.TakeMagicBurstSettingsRequest() && !bars.TakeMagicBurstSettingsRequest(), "the shared HUD shortcut activates once");
        *predictions = false; *rules = 0; bars.Save(store);
        actionbars_module restored; restored.Load(store);
        Check(!restored.SkillchainPredictionsEnabled() && restored.SkillchainRules() == server_rules::retail && store.ReadBool("magicburst", "predictions", false),
            "canonical Action Bars settings override retained legacy keys on subsequent loads");
        module.Save(store);
        magic_burst_module loaded; loaded.ApplyTextDefaults(theme); loaded.Load(store);
        Check(loaded.State().bounds.x == 123 && loaded.State().bounds.y == 234 && loaded.State().bounds.width == 160 &&
            store.ReadFloat("text.magicburst.timer", "offset_x", 0) == 9 && store.ReadFloat("text.magicburst.timer", "offset_y", 0) == -4,
            "burst display position and independent text settings survive normal profile round trips");
        module.ResetDefaults(theme);
        bars.ResetDefaults(theme);
        Check(module.State().enabled && bars.SkillchainPredictionsEnabled() && bars.SkillchainRules() == server_rules::retail && module.State().bounds.x == 123,
            "reset restores feature defaults while retaining the movement anchor");
        size_t profiles{}; module.VisitTextSettings([&](ui::text_style&) { ++profiles; });
        Check(profiles == 3, "global typography visits all three burst text profiles");
        std::puts("Skillchain and Magic Burst checks passed."); return true;
    }
    catch (const std::exception& error) { std::fprintf(stderr, "Skillchains: %s\n", error.what()); return false; }
}
