#include "hud/hud_runtime.hpp"
#include "ui/text_metrics.hpp"
#include "notifications/event_packets.hpp"
#include "ui/status_strip.hpp"
#include "modules/experience/experience_readout_layout.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <stdexcept>

namespace xiui
{
    auto RunEnemyLayoutChecks(void) -> bool;
    auto RunVisibilityChecks(void) -> void;
    auto RunStatusDisplayChecks(void) -> void;
    auto RunXIUIStatusTimerChecks(void) -> void;
    auto RunAllSpellStatusChecks(void) -> void;
    auto RunPartyStatusPreviewChecks(void) -> void;
    auto RunPartyTextChecks(void) -> void;
    auto RunStatusModuleChecks(void) -> bool;
}

namespace
{
    auto Check(bool value, const char* message) -> void { if (!value) throw std::runtime_error{message}; }

    auto ActionPacket(void) -> std::vector<uint8_t>
    {
        std::vector<uint8_t> bytes(512);
        size_t position = 40;
        const auto put = [&](uint32_t value, unsigned bits) {
            for (unsigned i = 0; i < bits; ++i, ++position) bytes[position / 8] |= ((value >> i) & 1u) << (position % 8);
        };
        put(77, 32); put(2, 6); put(0, 4); put(4, 4); put(253, 32); put(0, 32);
        const auto result = [&](uint32_t parameter, uint32_t message, bool additional) {
            put(0, 5); put(0, 12); put(0, 7); put(0, 3); put(parameter, 17); put(message, 10); put(0, 31);
            put(additional, 1);
            if (additional) { put(0, 10); put(3, 17); put(160, 10); }
            put(additional, 1);
            if (additional) { put(0, 10); put(10, 14); put(374, 10); }
        };
        put(101, 32); put(2, 4); result(2, 236, false); result(6, 237, true);
        put(102, 32); put(1, 4); result(2, 236, false);
        bytes.resize((position + 7) / 8);
        return bytes;
    }

    auto Progression(void) -> void
    {
        xiui::progression_input input{};
        input.job = 15; input.level = 70; input.exp = 125; input.exp_needed = 5000;
        Check(xiui::SelectProgression(input).kind == xiui::progression_kind::experience, "leveling jobs use normal EXP");
        input.level = 75; input.limit_mode = true; input.limit = 9000; input.merits = 3; input.merit_max = 75;
        auto selected = xiui::SelectProgression(input);
        Check(selected.kind == xiui::progression_kind::limit && selected.current == 9000 && selected.points == 3 && selected.needed == 10000,
            "limit mode selects its real meter and merit balance");
        input.level = 99; input.limit_mode = false; input.capacity = 1234; input.job_points = 19;
        selected = xiui::SelectProgression(input);
        Check(selected.kind == xiui::progression_kind::capacity && selected.current == 1234 && selected.points == 19 && selected.needed == 30000,
            "unmastered level-99 jobs use capacity and job points");
        input.spent = 2100; input.master_breaker = true; input.mastery_job = 15; input.mastery_needed = 50000; input.mastery = 40000; input.master_level = 5;
        selected = xiui::SelectProgression(input);
        Check(selected.kind == xiui::progression_kind::mastery && selected.current == 40000 && selected.level == 5, "unlocked matching mastery job uses exemplar progress");
        input.mastery_job = 3;
        Check(xiui::SelectProgression(input).kind == xiui::progression_kind::capacity, "changing jobs never shows another job's mastery");
        input.mastery_job = 15; input.master_breaker = false;
        Check(xiui::SelectProgression(input).kind == xiui::progression_kind::capacity, "spent JP alone does not unlock mastery");
    }

    auto Combat(void) -> void
    {
        auto packet = ActionPacket();
        const auto decoded = xiui::DecodeCombatAction(packet);
        Check(decoded && decoded->targets[0].result_count == 2 && decoded->targets[0].results[1].additional_parameter == 3 &&
            decoded->targets[0].results[1].spikes_message == 374 && decoded->targets[1].id == 102, "all results, additional effects and spikes survive action decoding");
        for (size_t size = 0; size + 1 < packet.size(); ++size)
            Check(!xiui::DecodeCombatAction(std::span<const uint8_t>{packet}.first(size)), "truncated combat packets never yield partial effects");
        xiui::combat_source source;
        source.Observe(*decoded, nullptr, 1000);
        Check(source.Find(77)->target == 101 && source.Find(101)->statuses.size() == 3 && source.Find(102)->statuses.size() == 1 &&
            source.Find(77)->statuses[0].id == 10, "AoE and subsequent action results update the correct recipients, including reflected statuses");
        const auto& sleep = source.Find(102)->statuses[0];
        Check(sleep.id == 2 && sleep.expires == 61000 && !sleep.uncertain && sleep.estimated, "confirmed mob effects use labeled XIUI+ durations");
        source.ApplyStatus(102, 2, 253, 2, 2000);
        Check(source.Find(102)->statuses[0].observed == 1000 && source.Find(102)->statuses[0].expires == 61000,
            "an ordinary damage message cannot refresh confirmed status evidence");
        source.ApplyStatus(102, 4, 0, 85, 2000);
        Check(source.Find(102)->statuses.size() == 1, "resisted effects do not appear");
        source.ApplyStatus(102, 2, 0, 206, 2000);
        Check(source.Find(102)->statuses.empty(), "wear-off messages remove the exact status");
        source.ApplyStatus(102, 5, 0, 236, 2000);
        source.ApplyStatus(102, 1000, 0, 236, 2000);
        source.Prune(62000);
        Check(source.Find(101)->statuses.size() == 2 && source.Find(102)->statuses.size() == 2,
            "XIUI+ estimates expire independently while unavailable durations remain untimed");
        for (uint32_t id = 1000; id < 1200; ++id) source.ApplyStatus(id, 3, 0, 236, 63000);
        Check(source.Count() == xiui::combat_source::capacity, "combat tracking remains bounded in large encounters");
        xiui::combat_cast cast{"Cure", 1000, 0, 4};
        Check(cast.Active(3000) && std::abs(cast.Ratio(3000) - 0.5f) < 0.001f && !cast.Active(8000), "cast progress and expiry use monotonic time");
        cast.interrupted = 2500;
        Check(cast.Active(2600) && !cast.Active(3400) && std::abs(cast.Ratio(3000) - 0.375f) < 0.001f, "interruption freezes progress and expires");
        source.HandlePacket(nullptr, 0xB, {}, 64000);
        Check(source.Count() == 0, "zoning clears all transient combat information");
    }

    auto Notifications(void) -> void
    {
        std::array<uint8_t, 32> invite{};
        const uint32_t sender = 99; const uint16_t index = 12;
        std::memcpy(invite.data() + 4, &sender, 4); std::memcpy(invite.data() + 8, &index, 2);
        std::memcpy(invite.data() + 12, "Inviter", 7);
        auto decoded = xiui::DecodeInvitation(0xDC, invite);
        Check(decoded && decoded->name == "Inviter" && decoded->sender == 99 && !decoded->alliance, "native party invitations decode bounded sender data");
        invite[11] = 1;
        Check(xiui::DecodeInvitation(0xDC, invite)->alliance && xiui::DecodeInvitation(0x21, std::span<const uint8_t>{invite}.first(12))->index == 12,
            "alliance and trade requests retain their native identities");
        Check(!xiui::DecodeInvitation(0xDC, std::span<const uint8_t>{invite}.first(31)), "truncated invitations are rejected");
        xiui::key_item_tracker keys;
        keys.Seed(606);
        std::array<uint8_t, 64> flags{}; flags[11] = 1u << 6;
        Check(keys.Apply(1, flags, true).empty(), "existing key items never become obtained alerts");
        flags[12] = 1;
        const auto gained = keys.Apply(1, flags, true);
        Check(gained.size() == 1 && gained[0] == 608 && keys.Apply(1, flags, true).empty(), "only new key-item bits generate alerts and duplicates remain silent");
        flags[13] = 1;
        Check(keys.Apply(1, flags, false).empty() && keys.Apply(1, flags, true).empty() && keys.Apply(99, flags, true).empty(), "login synchronization and invalid tables stay silent");
        xiui::notification_center center;
        center.Post({xiui::notification_kind::party_invite, 99, 0, "Inviter invited you"}, 1000);
        center.Post({xiui::notification_kind::gil, 65535, 5, "gil"}, 1000);
        center.Post({xiui::notification_kind::gil, 65535, 7, "gil"}, 1100);
        Check(center.Entries().size() == 2 && center.Entries().back().event.amount == 12, "zero-amount invitations are accepted and gil gains merge normally");
        xiui::notification_module module;
        *module.Option(14).toggle = true; *module.Option(15).choice = 0;
        auto store = xiui::settings_store::FromIniText(""); module.Save(store);
        xiui::notification_module loaded; loaded.Load(store);
        Check(*loaded.Option(14).toggle && *loaded.Option(15).choice == 0, "optional notification containers and anchor directions persist");
        const xiui::ui::rect panel{100, 80, 300, 36};
        const xiui::ui::horizontal_fade right{panel, 0, 1}, left{panel, 1, 0};
        Check(right.At(100) == 0 && right.At(400) == 1 && left.At(100) == 1 && left.At(400) == 0 && right.At(250) == 0.5f,
            "right anchoring fades left and left anchoring fades right across the complete container");
        for (const auto& slice : xiui::ui::LayoutNineSliceFrame(panel, 24, 5))
            for (float x : {slice.bounds.x, slice.bounds.x + slice.bounds.width})
                Check(std::abs(right.At(x) + left.At(x) - 1) < 0.001f, "all nine native frame patches share the continuous global gradient mask");
        const xiui::ui::rect viewport{0, 0, 1920, 1080};
        const auto anchoredRight = xiui::notification_renderer::ScreenBounds(viewport, panel, 0, 0, xiui::ui::text_alignment::right);
        const auto anchoredLeft = xiui::notification_renderer::ScreenBounds(viewport, panel, 0, 0, xiui::ui::text_alignment::left);
        Check(anchoredRight.x + anchoredRight.width == anchoredLeft.x && anchoredLeft.y == anchoredRight.y,
            "direction changes the growth edge while retaining the same screen-space anchor");
    }
}

namespace xiui
{
    struct module_parity_test
    {
        static auto StatusIntegration(void) -> void
        {
            hud_runtime runtime;
            runtime.editing_ = true;
            Check(runtime.HandleCommand("debuffs") && runtime.config_.module == &runtime.debuffs_ && runtime.editing_,
                "status command opens the selected native group");
            const auto page = runtime.ActiveConfigPage();
            Check(page.primary == &runtime.buffs_ && page.secondary == &runtime.debuffs_ && page.SectionCount() == 2,
                "both native status groups are reachable in one config page");
            runtime.editing_ = false;
            auto& player = runtime.party_source_.members_[0];
            player.active = player.buffs_known = true; player.id = 50; player.buff_count = 2;
            player.buffs = {33, 2};
            Check(runtime.BeginControllerFocus(hud_runtime::controller_surface::buffs) && runtime.ControllerStatusBounds().width > 0,
                "native buffs can enter deliberate controller inspection");
            runtime.general_.Visibility().Load(settings_store::FromIniText("[visibility.buffs]\nhide_menu=1\n"));
            runtime.visibility_context_.menu_open = true;
            runtime.UpdateControllerFocus();
            Check(runtime.controller_surface_ == hud_runtime::controller_surface::none,
                "hidden native status groups release controller focus");
            runtime.visibility_context_ = {};
            Check(runtime.BeginControllerFocus(hud_runtime::controller_surface::debuffs), "native debuffs have independent controller entry");
            player.buff_count = 1;
            runtime.UpdateControllerFocus();
            Check(runtime.controller_surface_ == hud_runtime::controller_surface::none,
                "an emptied native status group releases controller focus");
        }

        static auto Readouts(void) -> void
        {
            const auto narrow = ui::TextGlyphMetrics('1', 5, 9, true), wide = ui::TextGlyphMetrics('8', 9, 9, true);
            Check(narrow.advance == wide.advance && narrow.offset == 2 && wide.offset == 0,
                "tabular digits use one shared width and center narrow glyphs in their cells");
            Check(ui::TextGlyphMetrics('1', 5, 9, false).advance == 5 && ui::TextGlyphMetrics('W', 13, 9, true).advance == 13 &&
                ui::TextGlyphMetrics(':', 4, 9, true).offset == 0, "ordinary text keeps its font metrics and tabular digits can be disabled");
            experience_module module;
            const auto theme = ui::theme::CreateDefault();
            module.ApplyTextDefaults(theme);
            const auto current = static_cast<size_t>(experience_readout::current_required);
            const auto remaining = static_cast<size_t>(experience_readout::remaining);
            const auto percent = static_cast<size_t>(experience_readout::percent);
            for (const auto index : {current, remaining, percent}) module.SetReadoutVisible(index, true);
            Check(!module.Settings().show_current && !module.Settings().show_remaining && !module.Settings().show_percent,
                "adding progress readouts moves their matching values out of bar info");
            progression_input input{}; input.job = 1; input.level = 37; input.exp = 500; input.exp_needed = 2000;
            const auto progress = SelectProgression(input);
            module.BuildReadouts(progress, true);
            const auto has = [&](const char* text) {
                return std::any_of(module.readout_values_.begin(), module.readout_values_.end(), [&](const auto& value) { return value.text == text; });
            };
            Check(has("500 / 2000") && has("1500 left") && has("25.0%"), "readout list exposes the same current, remaining and percent values as the bar");
            for (const auto& value : module.readout_values_) Check(value.text.find("Sample:") == std::string::npos, "edit preview has no Sample prefix");
            module.Settings().show_current = true; module.OnOptionChanged(1);
            Check(!module.Settings().readouts[current] && module.Settings().readouts[remaining], "moving one value back to bar info leaves other readouts enabled");
            module.SetReadoutVisible(current, true);

            ui::design_system design;
            const ui::rect meter{100, 200, 300, 30};
            module.Settings().readout_anchor = 2;
            module.Settings().readout_text.offset_x = -45;
            module.Settings().readout_text.offset_y = -13;
            module.Settings().readout_text.placement = 2;
            const auto overlay = module.ReadoutArea(design, meter, true, 16);
            Check(overlay.x == 55 && overlay.y == 194 && overlay.height == 16, "overlay centers the block on the meter and applies both signed offsets");
            module.Settings().readout_anchor = 1;
            const auto above = module.ReadoutArea(design, meter, true, 38);
            Check(above.y + above.height < meter.y, "Above places the complete wrapped block above the meter");
            module.Settings().readout_anchor = 0;
            module.Settings().readout_text.offset_y = 0;
            const auto baseline = module.ReadoutArea(design, meter, false, 16);
            module.Settings().readout_text.offset_y = -40;
            Check(module.ReadoutArea(design, meter, false, 16).y == baseline.y - 40, "negative readout Y is never canceled by automatic spacing");
            const std::array<float, 4> widths{120, 120, 120, 120};
            const auto flow = LayoutExperienceReadouts(widths, 300, 16, 4, 2, 16);
            Check(flow.count == 4 && flow.rows == 2 && flow.height == 36 && flow.row_widths[0] == 256,
                "wrapping reports the complete block size before anchor placement");
            const std::array<float, 3> crowded{140, 140, 140};
            const auto overflow = LayoutExperienceReadouts(crowded, 300, 16, 4, 1, 8);
            Check(overflow.overflow && overflow.count == 2 && overflow.items[1].bounds.x + overflow.items[1].bounds.width < overflow.ellipsis.x,
                "row-limit ellipsis reserves space rather than painting over a readout");
            auto store = settings_store::FromIniText(""); module.Save(store);
            experience_module restored; restored.ApplyTextDefaults(theme); restored.Load(store);
            Check(restored.Settings().readout_anchor == 0 && restored.Settings().readout_text.offset_x == -45 && restored.Settings().readout_text.offset_y == -40 &&
                restored.Settings().readout_text.placement == 2 && restored.Settings().readouts[current] && !restored.Settings().show_current,
                "anchor, shared X/Y, alignment and progress ownership survive settings reload");
            Check(restored.Settings().readout_text.style.top_color == theme.meter_text.top_color && restored.Settings().readout_text.style.stroke.enabled &&
                restored.Settings().readout_text.style.shadow.enabled, "overlay positioning retains shared HUD text effects");
            experience_module legacy; legacy.ApplyTextDefaults(theme); legacy.Load(settings_store::FromIniText("[exp]\nshow_current=1\nshow_remaining=1\nshow_percent=0\njob_readout_version=1\n"));
            Check(legacy.Settings().readout_anchor == 0 && legacy.Settings().show_current && legacy.Settings().show_remaining && !legacy.Settings().show_percent &&
                !legacy.Settings().readouts[current] && !legacy.Settings().readouts[remaining] && !legacy.Settings().readouts[percent],
                "existing profiles retain their bar information and below-bar placement");
            const experience_readout_text countdown{"~10:00 Next", false, experience_readout::eta};
            const experience_readout_text rate{"8,500 EXP/hr", false, experience_readout::rate};
            Check(module.ReadoutWidth(countdown, 90, 1) == 90 && module.ReadoutWidth(countdown, 80, 1) == 90 &&
                module.ReadoutWidth(rate, 125, 1) == 125 && module.ReadoutWidth(countdown, 40, 0.5f) == 45,
                "readout slots retain their width as values shrink and keep independent widths at any text scale");
            size_t stable = module.OptionCount();
            for (size_t index = 0; index < module.OptionCount(); ++index)
                if (const auto option = module.Option(index); option.label && std::strcmp(option.label, "Stable numbers") == 0) stable = index;
            Check(stable < module.OptionCount() && module.ConfigSectionContains(4, stable) && !module.DisplayOption(experience_display_view::layout, stable),
                "Stable numbers is exposed in EXP Text without changing Display controls");
            *module.Option(stable).toggle = false; module.OnOptionChanged(stable);
            Check(module.ReadoutWidth(countdown, 80, 1) == 80, "turning off Stable numbers restores natural readout widths");
            module.Save(store); restored.Load(store);
            Check(!restored.Settings().stable_numbers && legacy.Settings().stable_numbers,
                "Stable numbers persists and defaults on for existing profiles without replacing text styles");
        }

        static auto Settings(void) -> void
        {
            Progression();
            progression_input input{}; input.job = 15; input.level = 99; input.exp = 333; input.exp_needed = 55999;
            input.limit = 9000; input.merits = 4; input.merit_max = 75; input.capacity = 1500; input.job_points = 12;
            Check(SelectProgression(input).kind == progression_kind::capacity && SelectProgression(input, progression_mode::experience).current == 333 &&
                SelectProgression(input, progression_mode::experience).kind == progression_kind::experience, "manual EXP overrides Auto's capacity selection at level 99");
            input.limit_mode = false;
            const auto limit = SelectProgression(input, progression_mode::limit);
            Check(limit.kind == progression_kind::limit && limit.current == 9000 && limit.points == 4 && limit.available,
                "manual Limit Points does not require changing the game's earning mode");
            Check(FormatProgressionInfo(limit, false, false, false, true, false) == "Merits 4/75" &&
                FormatProgressionInfo(limit, true, false, false, false, false) == "9000 / 10000", "point totals and current/required values have independent visibility");
            input.level = 50;
            const auto unavailable = SelectProgression(input, progression_mode::capacity);
            Check(unavailable.kind == progression_kind::capacity && !unavailable.available &&
                FormatProgressionInfo(unavailable, true, true, true, true, true) == "Requires level 99", "unavailable manual modes remain selected and display their unlock requirement");
            input.level = 99; input.spent = 2100; input.master_breaker = true; input.mastery_job = 15;
            input.mastery = 2500; input.mastery_needed = 10000; input.master_level = 3;
            const auto mastery = SelectProgression(input, progression_mode::mastery);
            Check(mastery.available && std::string_view{mastery.Label()} == "EXEMPLAR" &&
                FormatProgressionInfo(mastery, false, false, false, false, true) == "ML 3", "Exemplar mode uses matching mastery data and independent ML visibility");
            input.mastery_job = 3;
            Check(!SelectProgression(input, progression_mode::mastery).available && SelectProgression(input, progression_mode::mastery).current == 0,
                "changing jobs cannot expose stale mastery points from another job");

            const auto find = [](hud_module& module, const char* label) -> size_t {
                for (size_t index = 0; index < module.OptionCount(); ++index)
                    if (std::string_view{module.Option(index).label} == label) return index;
                throw std::runtime_error{label};
            };
            const auto ownership = [](hud_module& module) {
                for (size_t index = 0; index < module.OptionCount(); ++index)
                {
                    size_t owners{};
                    for (size_t section = 0; section < module.ConfigSections().size(); ++section) owners += module.ConfigSectionContains(section, index);
                    Check(owners == 1 && module.Option(index).label != nullptr, "every control belongs to one settings topic");
                }
            };
            const auto legacy = settings_store::FromIniText("[exp]\nshow_current=0\nshow_label=0\ninfo_position=1\ntext_layout_version=1\n[layout.exp]\nx=42\ny=65\nwidth=456\nheight=74\n[text.experience.details]\noffset_x=-7\n[notifications]\nitems=0\nitem_color=8099AABB\n");
            experience_module exp; exp.ApplyTextDefaults(ui::theme::CreateDefault()); exp.Load(legacy);
            Check(exp.Settings().mode == 0 && !exp.Settings().show_points && !exp.Settings().show_master_level && !exp.Settings().show_label &&
                exp.Settings().info_position == 1 && exp.State().bounds.x == 42 && exp.State().bounds.width == 456,
                "legacy EXP layout and visibility survive the settings reorganization");
            const auto modeOption = find(exp, "Progression mode");
            Check(exp.Option(modeOption).choice_count == 5 && exp.ConfigSections().size() == 8, "all progression modes remain available across eight combined EXP topics");
            exp.input_ = input; exp.progression_ready_ = true;
            *exp.Option(modeOption).choice = 1; exp.OnOptionChanged(modeOption);
            Check(exp.Progression().kind == progression_kind::experience && exp.Progression().current == 333, "mode changes apply immediately from the current native data snapshot");
            exp.Settings().show_points = true;
            auto store = settings_store::FromIniText(""); exp.Save(store);
            experience_module restored; restored.ApplyTextDefaults(ui::theme::CreateDefault()); restored.Load(store);
            Check(restored.Settings().mode == 1 && restored.Settings().show_points && !restored.Settings().show_current && restored.State().bounds.width == 456,
                "manual mode, independent totals and existing geometry persist");
            store.WriteInt("exp", "mode", 99); restored.Load(store);
            Check(restored.Settings().mode == 0, "invalid persisted modes recover to Auto");
            ownership(exp);
            const ui::config_layout expLayout{{0, 0, 1100, 600}, 15, false, true};
            for (size_t topic = 0; topic < exp.ConfigSections().size(); ++topic)
            {
                size_t controls{};
                for (size_t index = 0; index < exp.OptionCount(); ++index) controls += exp.ConfigSectionContains(topic, index);
                if (exp.UsesDisplayEditor(topic))
                {
                    const ui::config_layout displayLayout{{0, 0, 1100, 600}, 15, false, true, 40};
                    for (const auto view : {experience_display_view::bar_info, experience_display_view::layout})
                    {
                        size_t visible{};
                        for (size_t index = 0; index < exp.OptionCount(); ++index) visible += exp.DisplayOption(view, index);
                        Check(visible <= static_cast<size_t>(displayLayout.VisibleRows() * 2), "EXP display views fit below their shared tabs");
                    }
                }
                else Check(controls <= static_cast<size_t>(expLayout.VisibleRows() * 2), "combined EXP topics fit the normal config window");
            }

            const auto theme = ui::theme::CreateDefault();
            auto oldReadouts = settings_store::FromIniText("");
            module_text_profile oldText{theme.caption_text}; oldText.style.scale = 0.75f; oldText.offset_x = 7;
            SaveTextProfile(oldReadouts, "text.experience.readouts", oldText);
            oldReadouts.WriteBool("exp.readouts", "job", false);
            for (size_t i = 0; i < experience_readout_count; ++i) oldReadouts.WriteInt("exp.readout_order", experience_readouts[i].key, static_cast<int>(i));
            experience_module corrected; corrected.ApplyTextDefaults(theme); corrected.Load(oldReadouts);
            const auto& readoutStyle = corrected.Settings().readout_text.style;
            Check(readoutStyle.scale == theme.meter_text.scale && readoutStyle.top_color == theme.meter_text.top_color &&
                readoutStyle.bottom_color == theme.meter_text.bottom_color && readoutStyle.stroke.enabled && readoutStyle.shadow.enabled &&
                corrected.Settings().readout_text.offset_x == 7, "legacy muted readouts acquire normal HUD styling without losing their offsets");
            const auto jobReadout = static_cast<size_t>(experience_readout::job);
            Check(corrected.Settings().readouts[jobReadout] && corrected.Settings().order[jobReadout] == 0,
                "the new job/subjob identity is enabled first for the original EXP readout defaults");
            experience_sample identity{42, 100, 30}; identity.progress.job = 1; identity.progress.level = 75;
            identity.sub_job = 13; identity.sub_level = 37; identity.effective_sub_level = 15;
            corrected.source_.tracker.Observe(identity, ::GetTickCount64(), 1700000000);
            corrected.source_.ready = corrected.source_.sync_active = true; corrected.input_ = identity.progress;
            corrected.BuildReadouts(SelectProgression(identity.progress), false);
            Check(corrected.readout_values_.front().text == "Job WAR75 / NIN37 (Sync 30/15)", "job identity includes both actual levels and separate synced levels");
            corrected.Settings().readout_text.style.top_color = 0xFF44AAFF; corrected.Settings().readout_text.style.scale = 0.92f;
            corrected.Settings().readouts[jobReadout] = false; corrected.Save(oldReadouts);
            experience_module customized; customized.ApplyTextDefaults(theme); customized.Load(oldReadouts);
            Check(customized.Settings().readout_text.style.top_color == 0xFF44AAFF && customized.Settings().readout_text.style.scale == 0.92f &&
                !customized.Settings().readouts[jobReadout], "later custom text and hidden job preferences survive the one-time default corrections");

            *exp.Option(find(exp, "Selected readout")).choice = static_cast<int>(experience_readout::food);
            exp.OnOptionChanged(find(exp, "Selected readout"));
            *exp.Option(find(exp, "Show readout")).toggle = true;
            *exp.Option(find(exp, "Position")).slider = 1; exp.OnOptionChanged(find(exp, "Position"));
            *exp.Option(find(exp, "Rate window")).choice = 3;
            *exp.Option(find(exp, "Rate calculation")).choice = 1;
            exp.source_.LoadCharacter(42);
            experience_sample sample{42, 100, 50}; sample.progress.job = 1; sample.progress.level = 50; sample.progress.exp_needed = 7800;
            const auto now = ::GetTickCount64(); exp.source_.tracker.Observe(sample, now, 1700000000);
            exp.source_.tracker.Award(progression_kind::experience, 500, 3, true, now, 1700000000);
            exp.source_.tracker.Reset(now, 1700000000);
            exp.source_.tracker.Observe(sample, now, 1700000000);
            exp.source_.tracker.Award(progression_kind::experience, 250, 0, false, now, 1700000000);
            exp.source_.tracker.Pause(true, now);
            exp.Save(store);
            experience_module sessionRestored; sessionRestored.ApplyTextDefaults(ui::theme::CreateDefault()); sessionRestored.Load(store);
            sessionRestored.source_.LoadCharacter(42);
            Check(sessionRestored.Settings().readouts[static_cast<size_t>(experience_readout::food)] &&
                sessionRestored.Settings().order[static_cast<size_t>(experience_readout::food)] == 0 &&
                sessionRestored.Settings().rate_window == 3 && sessionRestored.Settings().rate_basis == 1,
                "selected readouts, ordering and independent rate preferences persist");
            Check(sessionRestored.source_.tracker.Paused() && sessionRestored.source_.tracker.Totals(progression_kind::experience).received == 250 &&
                sessionRestored.source_.tracker.History().size() == 1 && sessionRestored.source_.tracker.History()[0].totals[0].received == 500,
                "hot reload retains per-character session totals, pause state and completed history");
            sessionRestored.source_.LoadCharacter(99);
            Check(!sessionRestored.source_.tracker.Started() && sessionRestored.source_.tracker.History().empty(), "another character never inherits EXP totals or history");
            sessionRestored.source_.LoadCharacter(42);
            Check(sessionRestored.source_.tracker.Totals(progression_kind::experience).received == 250, "switching back restores the character's own observed session");

            notification_module notices; notices.Load(legacy);
            Check(!*notices.Option(find(notices, "Key items")).toggle && !*notices.Option(find(notices, "Gil")).toggle &&
                !*notices.Option(find(notices, "Treasure pool")).toggle && *notices.Option(find(notices, "Party / alliance invites")).toggle,
                "old item-filter settings preserve currency/key/treasure visibility while invitation settings remain independent");
            const std::array<const char*, 8> labels{"Items", "Experience", "Skill gains", "Party / alliance invites", "Trade requests", "Key items", "Gil", "Treasure pool"};
            const std::array kinds{notification_kind::item, notification_kind::experience, notification_kind::skill, notification_kind::party_invite,
                notification_kind::trade_invite, notification_kind::key_item, notification_kind::gil, notification_kind::treasure};
            for (const auto* label : labels) *notices.Option(find(notices, label)).toggle = true;
            for (size_t i = 0; i < kinds.size(); ++i)
            {
                notices.Center().Clear(); notices.Center().Post({kinds[i], 1, 1, "Example"}, 1000);
                auto* enabled = notices.Option(find(notices, labels[i])).toggle;
                *enabled = false;
                Check(!notices.ShouldRender({}), "disabling one notification type suppresses that type even when every other type is enabled");
                *enabled = true;
                Check(notices.ShouldRender({}), "reenabling a notification type restores its visibility");
                *enabled = i % 2 == 0;
            }
            *notices.Option(find(notices, "Gil color")).color = 0x805599EE;
            notices.Save(store); notification_module saved; saved.Load(store);
            for (size_t i = 0; i < labels.size(); ++i)
                Check(*saved.Option(find(saved, labels[i])).toggle == (i % 2 == 0), "each notification filter persists independently");
            Check(*saved.Option(find(saved, "Gil color")).color == 0x805599EE && *saved.Option(find(saved, "Party invite color")).color == 0x8099AABB,
                "per-event colors preserve alpha and inherit existing tints for older settings");
            Check(notices.ConfigSections().size() == 5, "notification settings organize events, layout, appearance and animation");
            ownership(notices);

            hud_runtime runtime; runtime.config_.bounds = {100, 100, 1100, 600}; runtime.config_.module = &runtime.experience_;
            runtime.config_.section = 0;
            Check(runtime.HasConfigSubmenu() && runtime.GroupOptionCount() == 5, "EXP General groups enabled, mode, live selection and session pause/reset");
            const auto row = runtime.ConfigLayout().OptionRow(1);
            runtime.mouse_x_ = static_cast<int>(row.x + 30); runtime.mouse_y_ = static_cast<int>(row.y + 10);
            Check(runtime.ProcessConfigClick({0, 0, 1600, 900, 0, 1}) && runtime.experience_.Settings().mode == 1,
                "the actual config input path changes EXP mode");
            runtime.config_.module = &runtime.notifications_; runtime.config_.section = 1;
            Check(runtime.GroupOptionCount() == 8 && runtime.HasConfigSubmenu(), "Notifications Events shows all eight supported event switches");
        }

        static auto Run(void) -> void
        {
            Progression(); Combat(); Notifications();
            const auto theme = ui::theme::CreateDefault();
            party_source source; source.player_id_ = 1;
            auto& member = source.members_[0];
            member.id = 1; member.index = 10; member.active = member.in_zone = member.entity_valid = true;
            member.main_job = 3; member.mp = 20;
            party_module party; party.SetSource(&source); party.ApplyTextDefaults(theme); party.state_.bounds = {50, 60, 360, 0}; party.spacing_ = 10;
            const auto layout = party.LayoutRows(1);
            const auto top = layout.bounds.y + party.container_.edge_size + party.container_.padding + layout.leading_space + std::max(14.0f, party.text_[party_module::heading_text].style.scale * 18) + 6;
            Check(party.HitTarget(80, top + 2).first == 1 && party.HitTarget(80, top - 2).first == 0, "party click targeting uses actual row bounds and excludes the heading");
            member.in_zone = false;
            Check(party.HitTarget(80, top + 2).first == 0, "off-zone party entries cannot be targeted");
            enemy_list_module enemies; enemies.ApplyTextDefaults(theme); enemies.state_.bounds = {20, 30, 220, 0};
            enemy_entry enemy{}; enemy.id = 90; enemy.index = 11; enemy.name = "Enemy";
            enemies.source_.entries_.push_back(enemy);
            const auto before = enemies.RowHeight(enemy, 1000);
            combat_source combat; combat.ApplyStatus(90, 3, 0, 236, 1000); enemies.SetCombat(&combat);
            Check(enemies.RowHeight(enemy, 1000) > before && enemies.HitTarget(30, 35, 1000) == std::pair<uint32_t, uint32_t>{90, 11} &&
                enemies.HitTarget(30, 30 + enemies.RowHeight(enemy, 1000) + 1, 1000).first == 0, "enemy status rows reserve space and clicks exclude spacing gaps");
            ui::design_system design; const module_render_context context{design};
            target_module target; target.ApplyTextDefaults(theme);
            target_module::target_view view{}; view.id = 99; view.name = "Target"; view.health = true; view.hp = 80;
            meter_animation_state animation;
            const auto base = target.DrawView(view, {0, 0, 400, 0}, "", animation, 1000, context);
            view.statuses[0] = {3}; view.status_count = 1; view.cast = {"Stone", 0, 0, 4};
            Check(target.DrawView(view, {0, 0, 400, 0}, "", animation, 1000, context) > base, "target casts and status icons extend the content footprint");
            view.health = false; view.status_count = 0; view.cast = {};
            Check(target.DrawView(view, {0, 0, 400, 0}, "", animation, 1000, context) < base, "NPC targets retain their name without an HP row");
            hud_runtime runtime;
            runtime.editing_ = true;
            Check(!runtime.HandleTargetMouse(WM_LBUTTONDOWN, false), "edit mode never runs gameplay click targeting");
            runtime.editing_ = false;
            Check(!runtime.HandleTargetMouse(WM_LBUTTONDOWN, true) && !runtime.HandleTargetMouse(WM_RBUTTONDOWN, false), "blocked clicks and camera buttons pass through");
            runtime.hud_target_pressed_ = true;
            Check(runtime.HandleTargetMouse(WM_MOUSEMOVE, false) && runtime.HandleTargetMouse(WM_LBUTTONUP, false) && !runtime.hud_target_pressed_ &&
                !runtime.HandleTargetMouse(WM_MOUSEMOVE, false), "HUD clicks consume only their matching pointer gesture and release camera movement afterward");
        }
    };
}

int main(int argc, char** argv)
{
    try {
        if (argc > 1 && std::string_view{argv[1]} == "--status-module-only") {
            Check(xiui::RunStatusModuleChecks(), "Native Status Effects module checks failed");
            std::puts("Status module configuration, inheritance and persistence checks passed.");
        } else if (argc > 1 && std::string_view{argv[1]} == "--spell-statuses-only") {
            xiui::RunAllSpellStatusChecks();
            std::puts("All imported spell applications, secondary confidence, message variants and removal checks passed.");
        } else if (argc > 1 && std::string_view{argv[1]} == "--status-timers-only") {
            Combat();
            xiui::RunXIUIStatusTimerChecks();
            std::puts("XIUI+ duration lookup, estimates, real early removal, refresh, native timer distinction and expiry checks passed.");
        } else if (argc > 1 && std::string_view{argv[1]} == "--party-text-only") {
            xiui::RunPartyTextChecks();
            std::puts("Party Sync label, independent TP text, legacy migration, geometry and preset checks passed.");
        } else if (argc > 1 && std::string_view{argv[1]} == "--party-status-preview-only") {
            xiui::RunPartyStatusPreviewChecks();
            std::puts("Party edit-only status samples, countdowns, ordering, geometry and persistence checks passed.");
        } else if (argc > 1 && std::string_view{argv[1]} == "--hud-parity-only") {
            Combat();
            Check(xiui::RunEnemyLayoutChecks(), "Enemy List layout checks failed");
            xiui::RunVisibilityChecks();
            xiui::RunStatusDisplayChecks();
            Check(xiui::RunStatusModuleChecks(), "Native Status Effects module checks failed");
            xiui::module_parity_test::StatusIntegration();
            std::puts("Enemy columns, native status data/presentation and shared visibility checks passed.");
        } else if (argc > 1 && std::string_view{argv[1]} == "--readouts-only") {
            xiui::module_parity_test::Readouts();
            std::puts("Readout overlay, free X/Y, wrapping, progress values, preview labels and persistence checks passed.");
        } else if (argc > 1 && std::string_view{argv[1]} == "--settings-only") {
            xiui::module_parity_test::Settings();
            std::puts("EXP modes, settings navigation, notification filters, colors and persistence checks passed.");
        } else {
            xiui::module_parity_test::Run();
            std::puts("Existing-module progression, combat, targeting, notifications and container fade checks passed.");
        }
        return 0;
    }
    catch (const std::exception& error) { std::fprintf(stderr, "%s\n", error.what()); return 1; }
}
