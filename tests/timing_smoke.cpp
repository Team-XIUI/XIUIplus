#include "combat/timing_model.hpp"
#include "ui/combat_timing_display.hpp"
#include "modules/player/player_module.hpp"
#include "modules/target/target_module.hpp"
#include "modules/enemylist/enemy_list_module.hpp"

#include <cmath>
#include <cstdio>
#include <cstring>

namespace
{
    auto Check(bool value, const char* message) -> bool
    {
        if (!value) std::fprintf(stderr, "Timing check failed: %s\n", message);
        return value;
    }
    auto Near(float a, float b) -> bool { return std::abs(a - b) < 0.02f; }

    auto Attack(void) -> bool
    {
        xiui::attack_clock clock;
        clock.Begin(100, 4);
        if (!Check(Near(clock.Predict(100).seconds, 2.8f) && Near(clock.Predict(100).ratio, 0), "first round begins at the Phoenix engage estimate")) return false;
        for (double now : {103.0, 107.0, 111.0, 115.0}) clock.Round(now, 4, 1);
        if (!Check(clock.Predict(116).learned && Near(clock.Predict(116).seconds, 3), "three clean gaps learn the attack interval")) return false;
        clock.Hold(116, 1); clock.Hold(117, 8);
        if (!Check(clock.Predict(120).state == "Disabled" && Near(clock.Predict(120).seconds, 3), "nested disabling and casting holds preserve remaining time")) return false;
        clock.Release(121, 1, 2); clock.Release(122, 8);
        if (!Check(Near(clock.Predict(122).seconds, 5), "cast tail is added once when nested holds end")) return false;
        clock.Round(127, 4, 1);
        clock.AddHold(128, 2);
        if (!Check(Near(clock.Predict(128).seconds, 5), "job abilities and weapon skills add the two-second hold")) return false;
        clock.Change(6, 2, true);
        if (!Check(!clock.Predict(129).learned && clock.Predict(129).state == "Waiting for attack", "effect changes do not reuse an incompatible observed interval")) return false;
        clock.Round(133, 6, 2, true);
        clock.Stall("Too far");
        if (!Check(clock.Predict(134).state == "Too far", "server range/facing failures stay visible until a real round")) return false;
        clock.Round(139, 6, 2, true);
        if (!Check(clock.Predict(152).state == "Waiting for attack", "overdue timers wait instead of inventing another attack")) return false;
        clock.Stop();
        return Check(!clock.Predict(153).active, "disengaging clears the attack display");
    }

    auto Recovery(void) -> bool
    {
        xiui::recovery_clock clock;
        xiui::recovery_sample sample{100, 1000, 0, 1000, 1000, true};
        clock.Update(sample, 100);
        if (!Check(Near(clock.Predict(100).seconds, 21.4f), "first restoring tick has the longer startup delay")) return false;
        sample.mp = 3; clock.Update(sample, 110);
        if (!Check(clock.LastMP() == 0, "passive Refresh outside the tick window cannot move the resting clock")) return false;
        for (double now : {121.4, 130.4, 142.4, 151.4, 160.4})
        {
            sample.hp += 15; sample.mp += 12; sample.tp -= 10; clock.Update(sample, now);
        }
        if (!Check(clock.LastHP() == 15 && clock.LastMP() == 12 && clock.Predict(160.4).learned && Near(clock.Predict(160.4).seconds, 12),
            "observed ticks refine gains and synchronize the 9 / 9 / 12 pattern")) return false;
        if (!Check(clock.SecondsToGoal(160.4, 80) > 0 && clock.SecondsToGoal(160.4, 80) < clock.SecondsToGoal(160.4, 100), "recovery goals produce finite independent estimates")) return false;
        sample.blocked = true; clock.Update(sample, 161);
        if (!Check(clock.Predict(161).state == "Recovery blocked" && clock.SecondsToGoal(161, 100) < 0, "Disease/Plague/Curse suppress invented recovery times")) return false;
        sample.blocked = false; sample.hp = sample.hp_max; sample.mp = sample.mp_max;
        clock.Update(sample, 162); const auto completed = clock.Completion(); clock.Update(sample, 163);
        if (!Check(completed == 1 && clock.Completion() == completed && clock.Full(), "full recovery emits one completion event")) return false;
        sample.resting = false; clock.Update(sample, 164);
        if (!Check(!clock.Predict(164).active, "standing clears recovery timing")) return false;
        sample.resting = true; sample.joined_late = true; sample.hp = 100; sample.mp = 0; clock.Update(sample, 200);
        if (!Check(!clock.Predict(200).known && clock.SecondsToGoal(200, 100) < 0, "loading while resting waits for actual tick evidence")) return false;
        sample.mp += 12; clock.Update(sample, 203);
        return Check(clock.Predict(203).known && clock.LastMP() == 12, "a real tick synchronizes a late-loaded resting timer");
    }

    auto TP(void) -> bool
    {
        xiui::tp_clock clock;
        clock.Reset(50, 2, 1); clock.Update(99, 0); clock.Round(100, 2, 0); clock.Update(100.1, 100);
        const auto estimate = clock.Predict(100, 1000, 4, 2);
        if (!Check(estimate.learned && Near(estimate.per_hit, 50) && estimate.hits == 18 && estimate.rounds > 0 && estimate.seconds > 0,
            "successful hits learn TP gain and predict hits, rounds and time to the goal")) return false;
        clock.Round(104, 2, 100); clock.Contaminate(104); clock.Update(104.1, 350);
        if (!Check(Near(clock.Predict(350, 1000, 4, 2).per_hit, 50), "simultaneous outside TP cannot become learned normal-hit TP")) return false;
        if (!Check(clock.Predict(3000, 3000, 4, 2).rounds == 0, "reaching any configured TP goal produces zero remaining rounds")) return false;
        clock.Reset(75, 1, 2);
        return Check(!clock.Predict(0, 2000, 4, 2).learned && Near(clock.Predict(0, 2000, 4, 2).per_hit, 75), "new gear/jobs discard incompatible TP samples");
    }

    template<typename Module> auto Settings(void) -> bool
    {
        Module module; module.ApplyTextDefaults(xiui::ui::theme::CreateDefault());
        auto store = xiui::settings_store::FromIniText("");
        const auto sections = module.ConfigSections();
        size_t timingOptions{};
        for (size_t index = 0; index < module.OptionCount(); ++index)
        {
            size_t owners{};
            for (size_t topic = 0; topic < sections.size(); ++topic)
                if (module.ConfigSectionContains(topic, index)) { ++owners; timingOptions += topic >= 7; }
            if (!Check(owners > 0, "each existing and new setting remains reachable")) return false;
            auto option = module.Option(index);
            if (option.label && std::strcmp(option.label, "Enemy attack timer") == 0)
            {
                if (!Check(!*option.toggle, "enemy timers start off")) return false;
                *option.toggle = true;
            }
        }
        module.Save(store);
        Module restored; restored.Load(store);
        if (!Check(timingOptions > 0 && restored.OptionCount() == module.OptionCount(), "timing controls persist with the existing module")) return false;
        for (size_t index = 0; index < module.OptionCount(); ++index)
        {
            const auto before = module.Option(index), after = restored.Option(index);
            if (before.toggle && !Check(after.toggle && *before.toggle == *after.toggle, "timer toggles round-trip through normal settings/profiles")) return false;
        }
        return true;
    }

    auto TextAndDefaults(void) -> bool
    {
        auto input = xiui::settings_store::FromIniText("[timing.player]\nheight=nan\noffset_y=inf\n");
        xiui::ui::player_timing_settings settings;
        settings.Load(input);
        if (!Check(settings.attack && settings.resting && settings.sound == 0 && settings.height == 6 && settings.offset_y == 0,
            "player defaults enable timers, keep sound off and reject nonfinite layout values")) return false;
        settings.attack_text.style.shadow.enabled = true;
        settings.attack_text.style.stroke.enabled = true;
        settings.attack_text.style.glow.enabled = true;
        settings.attack_text.style.top_color = 0xFFAA8844;
        settings.attack_text.style.bottom_color = 0xFF112244;
        settings.attack_text.offset_y = -32;
        settings.offset_x = -60; settings.offset_y = -80;
        auto saved = xiui::settings_store::FromIniText(""); settings.Save(saved);
        xiui::ui::player_timing_settings restored; restored.Load(saved);
        return Check(restored.attack_text.style.shadow.enabled && restored.attack_text.style.stroke.enabled && restored.attack_text.style.glow.enabled &&
            restored.attack_text.style.top_color == 0xFFAA8844 && restored.attack_text.style.bottom_color == 0xFF112244 &&
            restored.attack_text.offset_y == -32 && restored.offset_x == -60 && restored.offset_y == -80,
            "timer text retains normal gradients/effects and signed profile/layout offsets");
    }

    auto AttackLayouts(void) -> bool
    {
        xiui::player_module player;
        player.State().bounds = {40, 100, 640, 88};
        player.Settings().tp_style.height = 12;
        auto& settings = player.TimingSettings();
        settings.attack_layout = 2;
        xiui::hud_snapshot snapshot{}; snapshot.mp_max = 100;
        float withMP{};
        for (const uint32_t maximum : {100u, 0u})
        {
            snapshot.mp_max = maximum;
            const auto tp = player.TPMeterBounds(snapshot), mini = player.AttackTimerBounds(snapshot);
            if (!Check(Near(mini.x, tp.x) && Near(mini.width, tp.width) && Near(mini.y + mini.height, tp.y - settings.mini_gap) &&
                Near(mini.height, 3), "mini track matches the actual TP track and sits directly above it")) return false;
            if (maximum) withMP = mini.width;
            else if (!Check(mini.width > withMP, "mini track follows TP when auto-hide MP changes the resource layout")) return false;
        }
        player.State().bounds.width = 520; player.Settings().tp_style.height = 24;
        settings.offset_x = -12; settings.offset_y = -8; settings.mini_height = 5; settings.mini_gap = 7;
        const auto tp = player.TPMeterBounds(snapshot), mini = player.AttackTimerBounds(snapshot);
        if (!Check(Near(mini.x, tp.x - 12) && Near(mini.y, tp.y - 20) && Near(mini.height, 5) && Near(mini.width, tp.width),
            "TP resizing, mini dimensions and signed offsets share live bar geometry")) return false;
        settings.attack_layout = 0;
        const auto below = player.AttackTimerBounds(snapshot);
        settings.attack_layout = 1;
        const auto above = player.AttackTimerBounds(snapshot);
        if (!Check(Near(below.y, player.State().bounds.y + player.State().bounds.height + settings.spacing + settings.offset_y) &&
            Near(above.y + above.height, player.State().bounds.y - settings.spacing + settings.offset_y) && above.width == below.width,
            "full readouts can anchor below or wholly above Player Bar")) return false;
        settings.attack_layout = 2; settings.mini_text = 2;
        auto store = xiui::settings_store::FromIniText(""); player.Save(store);
        xiui::player_module restored; restored.Load(store);
        if (!Check(restored.TimingSettings().attack_layout == 2 && restored.TimingSettings().mini_text == 2 &&
            restored.TimingSettings().mini_height == 5 && restored.TimingSettings().mini_gap == 7, "attack layout and mini preferences persist in module settings/profiles")) return false;
        xiui::ui::player_timing_settings legacy; legacy.Load(xiui::settings_store::FromIniText(""));
        return Check(legacy.attack_layout == 2 && legacy.mini_text == 0 && legacy.mini_gap == 0, "default attack mini track has zero gap and no text");
    }

    auto RecoveryLayouts(void) -> bool
    {
        xiui::player_module player;
        player.State().bounds = {40, 100, 640, 88};
        player.Settings().hp_style.height = 16;
        auto& settings = player.TimingSettings();
        settings.recovery_layout = 2;
        xiui::hud_snapshot snapshot{};
        float withMP{};
        for (const uint32_t maximum : {100u, 0u})
        {
            snapshot.mp_max = maximum;
            const auto hp = player.HPMeterBounds(snapshot), mini = player.RecoveryTimerBounds(snapshot);
            if (!Check(Near(mini.x, hp.x) && Near(mini.width, hp.width) && Near(mini.y + mini.height, hp.y - settings.recovery_mini_gap) &&
                Near(mini.height, 3), "resting mini track attaches to the actual HP track")) return false;
            if (maximum) withMP = mini.width;
            else if (!Check(mini.width > withMP, "resting mini follows HP when MP auto-hides")) return false;
        }
        player.State().bounds.width = 520; player.Settings().hp_style.height = 24;
        settings.offset_x = -12; settings.offset_y = -8; settings.recovery_mini_height = 5; settings.recovery_mini_gap = 7;
        const auto hp = player.HPMeterBounds(snapshot), mini = player.RecoveryTimerBounds(snapshot);
        if (!Check(Near(mini.x, hp.x - 12) && Near(mini.y, hp.y - 20) && Near(mini.height, 5) && Near(mini.width, hp.width),
            "resting mini follows custom HP height, player resizing and signed offsets")) return false;
        settings.recovery_layout = 0;
        const auto below = player.RecoveryTimerBounds(snapshot);
        settings.recovery_layout = 1;
        const auto above = player.RecoveryTimerBounds(snapshot);
        if (!Check(Near(below.y, player.State().bounds.y + player.State().bounds.height + settings.spacing + settings.offset_y) &&
            Near(above.y + above.height, player.State().bounds.y - settings.spacing + settings.offset_y),
            "resting full readout supports both Player Bar edges")) return false;
        settings.recovery_layout = 2; settings.recovery_mini_text = 2;
        settings.attack_layout = 1; settings.mini_height = 8; settings.mini_gap = 11; settings.mini_text = 1;
        auto store = xiui::settings_store::FromIniText(""); player.Save(store);
        xiui::player_module restored; restored.Load(store);
        const auto& saved = restored.TimingSettings();
        if (!Check(saved.recovery_layout == 2 && saved.recovery_mini_text == 2 && saved.recovery_mini_height == 5 && saved.recovery_mini_gap == 7 &&
            saved.attack_layout == 1 && saved.mini_text == 1 && saved.mini_height == 8 && saved.mini_gap == 11,
            "resting and attack mini preferences persist independently in profiles")) return false;
        for (size_t index = 27; index < xiui::ui::player_timing_settings::option_count; ++index)
            if (!Check(xiui::ui::player_timing_settings::RecoveryOption(index), "resting placement controls belong to Recovery")) return false;
        xiui::ui::player_timing_settings legacy; legacy.Load(xiui::settings_store::FromIniText(""));
        return Check(legacy.recovery_layout == 2 && legacy.recovery_mini_text == 0 && legacy.recovery_mini_gap == 0, "default recovery mini track has zero gap and no text");
    }
}

int main(int argc, char** argv)
{
    if (argc == 2 && std::strcmp(argv[1], "--layouts-only") == 0)
    {
        if (!AttackLayouts() || !RecoveryLayouts() || !Settings<xiui::player_module>()) return 1;
        std::puts("Attack/resting placements, HP/TP mini geometry, MP auto-hide, signed offsets and independent settings persistence checks passed.");
        return 0;
    }
    if (!Attack() || !Recovery() || !TP() || !TextAndDefaults() || !AttackLayouts() || !RecoveryLayouts() || !Settings<xiui::player_module>() || !Settings<xiui::target_module>() || !Settings<xiui::enemy_list_module>()) return 1;
    std::puts("Attack holds/learning, Phoenix resting ticks/recovery, TP predictions and module settings checks passed.");
    return 0;
}
