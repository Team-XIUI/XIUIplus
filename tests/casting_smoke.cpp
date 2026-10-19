#include "hud_runtime.hpp"
#include "combat/ability_recast.hpp"
#include "ui/recast_icons.hpp"

#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace
{
    auto Check(bool value, const char* message) -> void { if (!value) throw std::runtime_error{message}; }
    int list_calls{};
    auto __fastcall SelectedItem(void*, void*, int32_t index) -> int32_t { ++list_calls; return 100 + index; }

    auto Find(xiui::hud_module& module, const char* label) -> xiui::module_option
    {
        for (size_t i = 0; i < module.OptionCount(); ++i)
            if (std::strcmp(module.Option(i).label, label) == 0) return module.Option(i);
        throw std::runtime_error{label};
    }

    auto CastState(void) -> void
    {
        using namespace xiui;
        player_cast_state cast;
        const native_cast_sample old{0.9f, 240, 24, 0};
        cast.Begin("Cure", 1, false, 1000, old);
        cast.Update(old, 1010);
        Check(cast.Active() && !cast.sampled && cast.progress == 0, "a new cast does not inherit stale native progress");
        cast.Update({0.1f, 240, 216, 0}, 1020);
        cast.Update({0.5f, 240, 120, 0}, 1200);
        Check(cast.sampled && cast.progress == 0.5f, "cast progress follows native samples without a base-time estimate");
        cast.Update({1, 240, 0, 0}, 1400);
        Check(!cast.Active(), "native completion hides the cast");
        cast.Begin("Potion", 4112, true, 2000, {});
        cast.Update({0.5f, 100, 50, 0}, 2100);
        Check(!cast.sampled, "spell progress cannot drive an item cast");
        cast.Update({0.25f, 100, 75, 1}, 2200);
        cast.interrupted = 2300;
        cast.Update({0.8f, 100, 20, 1}, 2500);
        Check(cast.Active() && cast.progress == 0.25f, "interrupted casts freeze their native progress");
        cast.Update({}, 3000);
        Check(!cast.Active(), "interruptions expire after their short display window");
        cast.Begin("Cure", 1, false, 4000, {});
        cast.Update({std::numeric_limits<float>::quiet_NaN(), 100, 80, 0}, 4100);
        cast.Update({}, 5500);
        Check(!cast.Active(), "invalid or stalled native data cannot leave a stuck cast bar");

        player_cast_source source;
        combat_action action{};
        action.actor = 44; action.target_count = 1; action.category = 8; action.parameter = 0x68776163; action.targets[0].parameter = 1;
        source.Observe(action, 55, nullptr, 1000);
        Check(!source.State().Active(), "other players' casts never populate the player cast bar");
        source.Observe(action, 44, nullptr, 1100);
        Check(source.State().Active() && source.State().id == 1 && !source.State().item, "spell start packets select the local player's action");
        action.category = 4; source.Observe(action, 44, nullptr, 1200);
        Check(!source.State().Active(), "spell completion clears the local cast immediately");
        action.category = 9; action.targets[0].parameter = 4112; source.Observe(action, 44, nullptr, 1300);
        Check(source.State().item, "item-use starts retain their independent action type");
        action.parameter = 0x7073; source.Observe(action, 44, nullptr, 1400);
        Check(source.State().interrupted == 1400, "local interruption packets mark the active cast");
        source.HandlePacket(nullptr, 0xB, {}, 1500);
        Check(!source.State().Active(), "zoning removes stale cast information");
    }

    auto CostState(void) -> void
    {
        using namespace xiui;
        action_cost_state state{};
        state.kind = action_cost_kind::spell; state.name = "Cure II"; state.mp_cost = 24; state.player_mp = 23; state.recast_known = true;
        Check(!state.Ready() && !state.EnoughResources(), "insufficient MP cannot display a ready spell");
        state.player_mp = 24;
        Check(state.Ready(), "the exact MP cost is sufficient");
        state.remaining = 2;
        Check(!state.Ready(), "a live cooldown prevents readiness");
        state.remaining = 0; state.recast_known = false;
        Check(!state.Ready(), "missing recast data is not reported as ready");
        state = {}; state.kind = action_cost_kind::weapon_skill; state.name = "Fast Blade"; state.tp_cost = 1000; state.player_tp = 999; state.recast_known = true;
        Check(!state.Ready(), "weapon skills require at least 1000 TP");
        state.player_tp = 1000;
        Check(state.Ready(), "1000 TP enables weapon-skill readiness");
        state.kind = action_cost_kind::ability; state.tp_cost = 200; state.player_tp = 199;
        Check(!state.Ready(), "job-ability TP costs use the modern TP scale");
        state.tp_cost = 0; state.charge_cost = 1;
        state.charges = 1; state.max_charges = 2; state.remaining = 60;
        Check(state.charges == 1 && state.remaining == 60 && state.Ready(), "a partially recharging pool remains usable with a full charge");
        state.charge_cost = 2;
        Check(!state.EnoughResources(), "multi-charge actions require the correct number of charges");
        state.charges = 0;
        Check(state.charges == 0 && state.remaining == 60 && !state.Ready(), "an exact pool multiple shows a full next-charge interval");
        Check(FormatCastTime(2.5f) == "2.5s" && FormatCastTime(61) == "1:01", "cast and recast times preserve short precision and minute boundaries");
    }

    auto ChargeRecasts(void) -> void
    {
        using namespace xiui;
        const auto partial = CalculateChargeRecast(60 * 60, {2, 120});
        Check(partial.charges == 1 && partial.maximum == 2 && partial.remaining == 60 && partial.duration == 60,
            "an exact next-charge boundary keeps a full remaining interval");
        const auto empty = CalculateChargeRecast(120 * 60, {2, 120});
        Check(empty.charges == 0 && empty.remaining == 60, "a fully spent pool waits one charge interval");
        const auto ready = CalculateChargeRecast(0, {5, 240});
        Check(ready.charges == 5 && ready.remaining == 0 && ready.duration == 48,
            "a fully restored stratagem pool keeps its per-charge duration");
        const auto modified = CalculateChargeRecast(30 * 60, {2, 120}, static_cast<int16_t>(0xFFC4));
        Check(modified.charges == 1 && modified.remaining == 30 && modified.duration == 30,
            "native negative Calc2 values reduce the pool duration as signed values");
        Check(CalculateChargeRecast(100, {}).maximum == 0 &&
            CalculateChargeRecast(100, {2, 120}, -120).maximum == 0 &&
            CalculateChargeRecast(100, {2, std::numeric_limits<float>::quiet_NaN()}).maximum == 0,
            "unknown and invalid charge pools remain unavailable");
    }

    auto Commands(void) -> void
    {
        using namespace xiui;
        auto store = settings_store::FromIniText("[commands]\nenabled=1\nposition_initialized=1\n[layout.commands]\nx=100\ny=500\nwidth=112\nheight=168\n");
        command_menu_module module; module.Load(store);
        auto anchor = Find(module, "Window anchor");
        Check(anchor.available && anchor.choice_count == 2 && std::strcmp(anchor.choices[0], "Top") == 0 &&
            std::strcmp(anchor.choices[1], "Bottom") == 0 && !module.AnchorsTop(), "top and bottom options are always available; legacy settings retain bottom anchoring");
        native_ui_state native{}; native.menu_available = native.menu_visible = true; native.width = 112; native.height = 400;
        module.Update(native, 1920, 1080);
        Check(module.State().bounds.y == 268 && module.Layout(true).bottom == 668, "a taller command menu retains its saved bottom edge");
        *anchor.choice = 0;
        native.height = 90; module.Update(native, 1920, 1080);
        Check(module.State().bounds.y == 268 && module.Layout(true).top == 268 && module.Layout(true).anchor_top,
            "a shorter command menu retains the selected top edge");
        native.height = 400; module.Update(native, 1920, 1080);
        Check(module.State().bounds.y == 268, "a taller top-anchored menu grows downward");
        module.Save(store);
        command_menu_module restored; restored.Load(store); restored.Update(native, 1920, 1080);
        Check(restored.AnchorsTop() && restored.Layout(true).enabled && restored.Layout(true).top == 268,
            "command anchor choice and position survive saving");
        Check(!restored.Layout(false).enabled, "hiding XIUI disables native menu movement");
    }
}

namespace xiui
{
    struct casting_test
    {
        static auto Recast(void) -> void
        {
            recast_tracker tracker;
            recast_sample sample{recast_kind::ability, 5};
            sample.name = "Provoke"; sample.remaining = 10;
            tracker.Observe(sample, 1000); tracker.FinishSample(1000);
            Check(tracker.Entries().size() == 1 && tracker.Entries()[0].Progress(6000) == 0.5f,
                "cooldown progress interpolates between authoritative samples");
            sample.remaining = 4;
            tracker.Observe(sample, 7000); tracker.Observe(sample, 7000); tracker.FinishSample(7000);
            Check(tracker.Entries().size() == 1 && tracker.Entries()[0].duration == 10,
                "shared recast IDs remain one row and retain the observed full duration");
            tracker.FinishSample(11000); tracker.FinishSample(11500);
            Check(tracker.Entries()[0].ready && tracker.Entries()[0].ready_at == 11000 &&
                tracker.Entries()[0].Visible(12999, 2) && !tracker.Entries()[0].Visible(13000, 2) && !tracker.Entries()[0].Visible(11000, 0),
                "native readiness retains one brief Ready state without extending its lifetime");
            sample.remaining = 30;
            tracker.Observe(sample, 12000); tracker.FinishSample(12000);
            Check(!tracker.Entries()[0].ready && tracker.Entries()[0].duration == 30 && tracker.Entries()[0].started == 12000,
                "using an ability again starts a fresh cooldown cycle");
            recast_sample spell{recast_kind::spell, 5}; spell.name = "Cure V"; spell.remaining = 5;
            tracker.Observe(spell, 12000); tracker.FinishSample(12000);
            Check(tracker.Entries().size() == 2, "spell and ability IDs have separate identities");
            const auto charge = CalculateChargeRecast(3600, {2, 120});
            sample.id = 195; sample.name = "Quick Draw"; sample.remaining = charge.remaining;
            sample.charges = charge.charges; sample.maximum_charges = charge.maximum; sample.charge_duration = charge.duration;
            tracker.Observe(sample, 13000); tracker.FinishSample(13000);
            const auto& charged = tracker.Entries().back();
            Check(charged.charges == 1 && charged.maximum_charges == 2 && charged.duration == 60 && charged.Remaining(13000) == 60,
                "charge timers show the next charge without reporting a false zero at an exact boundary");
            tracker.Clear(); Check(tracker.Entries().empty(), "clearing session state removes stale cooldowns");

            const auto left = ui::LayoutMeterFill({10, 20, 100, 12}, 0.25f), right = ui::LayoutMeterFill({10, 20, 100, 12}, 0.25f, true);
            Check(left.x == 10 && right.x == 85 && left.width == 25 && right.width == 25,
                "left and right anchored fills retain the same amount of progress");
            Check(FormatRecastTime(59.01f, false) == "1:00" && FormatRecastTime(2.34f, true) == "2.4" && FormatRecastTime(3601, false) == "1:00:01",
                "countdowns round up and support tenths, minutes and hours");
            const auto abilityIcon = ui::RecastIconUV(false, 0, 1), spellIcon = ui::RecastIconUV(true, 4, 0);
            Check(abilityIcon.width > 0 && spellIcon.width > 0 && (abilityIcon.x != spellIcon.x || abilityIcon.y != spellIcon.y),
                "job-specific ability and spell IDs select embedded artwork");

            const auto theme = ui::theme::CreateDefault();
            hud_runtime runtime;
            runtime.recast_.ResetDefaults(theme);
            const auto casting = runtime.ConfigPages()[1];
            Check(casting.SectionCount() == 3 && casting.SectionModule(2) == &runtime.recast_, "Casting includes Recast beside Cast Bar and Cast Cost");
            Check(runtime.HandleCommand("recast") && runtime.config_.module == &runtime.recast_, "the Recast command opens its settings");
            auto& module = runtime.recast_;
            for (size_t index = 0; index < module.OptionCount(); ++index)
            {
                size_t owners{};
                for (size_t section = 0; section < module.ConfigSections().size(); ++section) owners += module.ConfigSectionContains(section, index);
                Check(owners == 1, "every Recast control belongs to one settings topic");
            }
            recast_entry colored{}; colored.expires = 40000;
            const auto farColor = module.Colors(colored, 1000), closeColor = module.Colors(colored, 39000);
            Check(farColor != closeColor, "bar and text colors change as the timer approaches readiness");
            *Find(module, "Display").choice = 1;
            *Find(module, "Anchor").choice = 1;
            *Find(module, "Fill behavior").choice = 1;
            *Find(module, "Maximum rows").slider = 2;
            *Find(module, "Icon size").slider = 64;
            *Find(module, "Spacing").slider = 9;
            *Find(module, "Selected text").choice = 1;
            *Find(module, "Glow").toggle = true;
            *Find(module, "X offset").slider = -7;
            *Find(module, "Ready bottom").color = 0xFFABCDEF;
            module.State().bounds.x = 333;
            auto saved = settings_store::FromIniText(""); module.Save(saved);
            recast_module restored; restored.ResetDefaults(theme); restored.Load(saved);
            Check(*Find(restored, "Display").choice == 1 && *Find(restored, "Anchor").choice == 1 &&
                *Find(restored, "Fill behavior").choice == 1 && *Find(restored, "Glow").toggle && *Find(restored, "X offset").slider == -7 &&
                *Find(restored, "Ready bottom").color == 0xFFABCDEF && restored.State().bounds.x == 333,
                "display, direction, colors, text effects and position survive saving and profile reloads");
            Check(!Find(restored, "Action icons").visible && !Find(restored, "Bar style").visible,
                "text-only mode hides unused icon and bar controls");
            ui::design_system design;
            module_render_context context{design, nullptr, true};
            restored.Render({}, context);
            Check(restored.State().bounds.height == 65, "text-only row height and spacing ignore the saved icon size");
            runtime.recast_source_.Update(nullptr, 1000);
            Check(!runtime.recast_source_.Available() && !runtime.recast_.ShouldRender({}), "unavailable or idle sources create no live timers");
        }

        static auto Selection(void) -> void
        {
            cast_cost_source source;
            std::array<uintptr_t, 32> primitive{};
            std::array<uint8_t, 128> header{};
            std::array<int32_t, 32> list{};
            uintptr_t current = reinterpret_cast<uintptr_t>(primitive.data()), object = reinterpret_cast<uintptr_t>(list.data());
            primitive[1] = reinterpret_cast<uintptr_t>(header.data());
            list[0x30 / 4] = 1; list[0x40 / 4] = 2; list[0x44 / 4] = 2;
            source.menu_slot_ = reinterpret_cast<uintptr_t>(&current);
            source.spell_slot_ = source.ability_slot_ = source.mount_slot_ = reinterpret_cast<uintptr_t>(&object);
            source.spell_item_ = source.ability_item_ = source.mount_item_ = reinterpret_cast<uintptr_t>(&SelectedItem);
            const auto menu = [&](const char* name) { std::memcpy(header.data() + 0x46, name, 16); };
            menu("menu    magic   ");
            Check(source.AvailableMask() == 7 && source.Selection() == std::pair{action_cost_kind::spell, 101}, "native magic selection resolves the highlighted list item");
            menu("menu    ability ");
            Check(source.Selection() == std::pair{action_cost_kind::ability, 101}, "native ability selection uses its own list");
            menu("menu    mount   ");
            Check(source.Selection() == std::pair{action_cost_kind::mount, 101}, "mount selection uses its own getter");
            menu("menu    playermo");
            const auto calls = list_calls;
            Check(source.Selection().first == action_cost_kind::none && list_calls == calls, "unrelated menus never read selection objects");
            menu("menu    magic   ");
            list[0x30 / 4] = -1;
            Check(source.Selection().second == -1 && list_calls == calls, "negative selections never invoke the native getter");
            list[0x30 / 4] = 2;
            Check(source.Selection().second == -1 && list_calls == calls, "out-of-range selections never invoke the native getter");
            list[0x30 / 4] = 0; list[0x40 / 4] = 0;
            Check(source.Selection().second == -1 && list_calls == calls, "empty native lists hide cast cost");
            object = 1;
            Check(source.Selection().second == -1 && list_calls == calls, "invalid selection pointers fail without an unsafe call");
            current = 0;
            Check(source.Selection().first == action_cost_kind::none, "closing the native menu clears selection");
        }

        static auto Modules(void) -> void
        {
            const auto theme = ui::theme::CreateDefault();
            player_module player;
            player.ResetDefaults(theme);
            hud_snapshot snapshot{}; snapshot.mp_max = 0;
            Check(!player.ShowsMP(snapshot), "MP hides automatically for players with no maximum MP");
            const auto two = player.ResourceRows(snapshot);
            Check(two[1].width == 0 && two[0].width == two[2].width && two[2].x == two[0].x + two[0].width + 12 &&
                std::abs(two[2].x + two[2].width - (player.State().bounds.x + player.State().bounds.width - 6)) < 0.001f,
                "HP and TP fill the player row with one normal gap when MP is hidden");
            snapshot.mp_max = 100;
            const auto three = player.ResourceRows(snapshot);
            Check(player.ShowsMP(snapshot) && three[1].width > 0 && three[0].width < two[0].width, "a drained caster retains the MP bar and normal three-column layout");
            snapshot.mp_max = 0;
            *Find(player, "Auto-hide MP").toggle = false;
            Check(player.ShowsMP(snapshot), "the MP auto-hide preference can be disabled");
            auto store = settings_store::FromIniText(""); player.Save(store);
            player_module restored; restored.Load(store);
            Check(restored.ShowsMP(snapshot), "MP visibility preference survives saving");

            hud_runtime runtime;
            runtime.cast_bar_.ResetDefaults(theme); runtime.cast_cost_.ResetDefaults(theme);
            const auto pages = runtime.ConfigPages();
            Check(pages[1].SectionCount() == 3 && pages[1].primary == &runtime.cast_bar_ && pages[1].secondary == &runtime.cast_cost_ && pages[1].tertiary == &runtime.recast_,
                "Casting groups Cast Bar, Cast Cost and Recast settings");
            for (auto* module : {static_cast<hud_module*>(&runtime.cast_bar_), static_cast<hud_module*>(&runtime.cast_cost_)})
                for (size_t index = 0; index < module->OptionCount(); ++index)
                {
                    size_t owners{};
                    for (size_t section = 0; section < module->ConfigSections().size(); ++section) owners += module->ConfigSectionContains(section, index);
                    Check(owners == 1, "every cast-module control belongs to exactly one topic");
                }
            *Find(runtime.cast_bar_, "Enabled").toggle = false;
            *Find(runtime.cast_cost_, "Selected text").choice = 1;
            *Find(runtime.cast_cost_, "Glow").toggle = true;
            runtime.cast_cost_.State().bounds.x = 313;
            runtime.cast_bar_.Save(store); runtime.cast_cost_.Save(store);
            cast_bar_module bar; cast_cost_module cost; bar.ResetDefaults(theme); cost.ResetDefaults(theme); bar.Load(store); cost.Load(store);
            Check(!bar.State().enabled && cost.State().enabled && cost.State().bounds.x == 313 && *Find(cost, "Glow").toggle,
                "module visibility, position and styled text persist independently");
            Check(!runtime.cast_bar_.ShouldRender({}) && !runtime.cast_cost_.ShouldRender({}), "idle modules remain hidden outside edit mode");
        }
    };
}

auto main(int argc, char** argv) -> int
{
    try
    {
        if (argc > 1 && std::strcmp(argv[1], "--charges-only") == 0)
        {
            CostState(); ChargeRecasts();
            std::puts("Resource readiness and shared charge boundaries, modifiers and unavailable pools passed.");
            return 0;
        }
        if (argc > 1 && std::strcmp(argv[1], "--recast-only") == 0)
        {
            xiui::casting_test::Recast();
            std::puts("Recast tracking, charges, colors, anchors, icon lookup, text-only layout, settings and Casting integration passed.");
            return 0;
        }
        if (argc > 1 && std::strcmp(argv[1], "--commands-only") == 0)
        {
            Commands();
            std::puts("Command-menu top/bottom options, resizing and persistence passed.");
            return 0;
        }
        CastState(); CostState(); ChargeRecasts(); Commands(); xiui::casting_test::Selection(); xiui::casting_test::Modules();
        std::puts("Casting state, native selection, resource readiness, charge timers, module settings and Player MP layout passed.");
        return 0;
    }
    catch (const std::exception& error) { std::fprintf(stderr, "Casting check failed: %s\n", error.what()); return 1; }
}
