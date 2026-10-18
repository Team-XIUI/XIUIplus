#include "ui/status_strip.hpp"
#include "ui/status_classification.hpp"
#include "modules/party/party_module.hpp"
#include "modules/target/target_module.hpp"
#include "combat/status_durations.hpp"

#include <cmath>
#include <stdexcept>
#include <set>

namespace
{
    auto Check(const bool value, const char* message) -> void
    { if (!value) throw std::runtime_error{message}; }
}

namespace xiui
{
    struct status_display_test
    {
        static auto Timers(void) -> void
        {
            Check(XIUIStatusSeconds(4, 58, 4, 236) == 120 && XIUIStatusSeconds(4, 254, 5, 236) == 180 &&
                XIUIStatusSeconds(4, 249, 34, 230) == 180, "XIUI supplies Paralyze, Blind and Blaze Spikes durations");
            Check(XIUIStatusSeconds(6, 682, 149, 127) == 30 && XIUIStatusSeconds(3, 170, 148, 242) == 120 &&
                XIUIStatusSeconds(13, 523, 16, 236) == 15 && XIUIStatusSeconds(0, 0, 2, 160) == 25,
                "ability and weapon IDs stay separate; per-effect pet overrides and additional effects retain XIUI values");
            combat_source source;
            ui::status_strip_settings settings;
            source.ApplyStatus(100, 4, 58, 236, 1000);
            const auto paralyze = source.Find(100)->statuses.front();
            Check(paralyze.estimated && paralyze.expires == 121000 && ui::StatusTimerText(paralyze, 1000, settings) == "2:00",
                "confirmed Paralyze keeps estimate metadata with a plain countdown label");
            source.ApplyStatus(100, 4, 58, 75, 5000);
            source.ApplyStatus(100, 4, 58, 85, 6000);
            source.ApplyStatus(100, 4, 58, 2, 7000);
            Check(source.Find(100)->statuses.front().observed == 1000 && source.Find(100)->statuses.front().expires == paralyze.expires,
                "no-effect, resist and damage messages cannot restart an estimate");
            source.ApplyStatus(100, 4, 0, 204, 39000);
            Check(source.Find(100)->statuses.empty(), "the captured 38-second wear-off clears Paralyze before its estimated deadline");
            source.ApplyStatus(100, 4, 58, 236, 40000);
            source.ApplyStatus(100, 4, 58, 236, 41000);
            Check(source.Find(100)->statuses.size() == 1 && source.Find(100)->statuses.front().expires == 161000,
                "a later confirmed application refreshes its own estimate without duplicating the effect");
            source.ApplyStatus(101, 34, 249, 230, 50000);
            source.ApplyStatus(101, 1000, 0, 236, 50000);
            source.Prune(230000);
            Check(source.Find(101)->statuses.size() == 1 && source.Find(101)->statuses.front().id == 1000 &&
                source.Find(101)->statuses.front().expires == 0, "estimated mob buffs expire while effects absent from XIUI remain untimed");
            Check(ui::StatusTimerText(source.Find(101)->statuses.front(), 230000, settings) == "?" &&
                ui::StatusTimerText({4, 0, 0, 51000}, 50000, settings) == "1", "unknown markers and exact native countdowns stay distinct from estimates");
        }

        static auto PartyText(void) -> void
        {
            const auto store = settings_store::FromIniText("");
            party_module party;
            auto values = party.text_[3];
            values.style.font = 1; values.style.scale = 1.25f; values.offset_x = 7; values.offset_y = 9;
            values.style.top_color = 0xFFAACC88; values.style.bottom_color = 0xFF445566;
            values.style.stroke.enabled = true; values.style.glow.enabled = true;
            SaveTextProfile(store, "text.party.values", values);
            auto sync = party.text_[4]; sync.offset_x = 13;
            SaveTextProfile(store, "text.party.details", sync);
            party.Load(store);
            const auto& migrated = party.text_[party_module::tp_text];
            Check(migrated.style.font == values.style.font && migrated.style.scale == values.style.scale && migrated.offset_x == 7 && migrated.offset_y == 9 &&
                migrated.style.top_color == values.style.top_color && migrated.style.bottom_color == values.style.bottom_color &&
                migrated.style.stroke.enabled && migrated.style.glow.enabled, "legacy TP copies the complete loaded Values presentation without changing its appearance");
            const auto& migratedMP = party.text_[party_module::mp_text];
            Check(migratedMP.style.font == values.style.font && migratedMP.style.scale == values.style.scale && migratedMP.offset_x == 7 && migratedMP.offset_y == 9 &&
                migratedMP.style.top_color == values.style.top_color && migratedMP.style.bottom_color == values.style.bottom_color &&
                migratedMP.style.stroke.enabled && migratedMP.style.glow.enabled, "legacy MP copies the complete loaded Values presentation without changing its appearance");
            const auto selector = party.Option(26);
            Check(selector.choice_count == 11 && std::string_view{selector.choices[3]} == "HP values" && std::string_view{selector.choices[4]} == "Sync" &&
                std::string_view{selector.choices[9]} == "TP values" && std::string_view{selector.choices[10]} == "MP values" &&
                party.text_[4].offset_x == 13, "Sync retains its legacy details section and HP/TP indices remain stable as independent MP is appended");
            bool visitedTP{}, visitedMP{};
            party.VisitTextSettings([&](ui::text_style& style) {
                if (&style == &party.text_[party_module::tp_text].style) visitedTP = true;
                if (&style == &party.text_[party_module::mp_text].style) visitedMP = true;
            });
            Check(visitedTP && visitedMP, "independent TP and MP profiles participate in shared global text controls");
            party.text_[party_module::tp_text].style.scale = 1.5f; party.text_[party_module::tp_text].offset_y = 21;
            party.text_[party_module::mp_text].style.scale = 1.75f; party.text_[party_module::mp_text].offset_y = 17;
            party.Save(store);
            values.style.scale = 0.6f; SaveTextProfile(store, "text.party.values", values);
            party_module loaded; loaded.Load(store);
            Check(store.ReadInt("party", "tp_text_version", 0) == 1 && loaded.text_[party_module::tp_text].style.scale == 1.5f &&
                store.ReadInt("party", "mp_text_version", 0) == 1 && loaded.text_[party_module::mp_text].style.scale == 1.75f &&
                loaded.text_[party_module::tp_text].offset_y == 21 && loaded.text_[party_module::mp_text].offset_y == 17 && loaded.text_[3].style.scale == 0.6f,
                "versioned migration preserves independent HP, MP and TP customization");
            loaded.text_[3].style.scale = 0.6f;
            loaded.text_[party_module::mp_text].style.scale = 0.6f;
            for (int layout = 0; layout < 3; ++layout)
                for (int display = 0; display < 2; ++display)
                {
                    loaded.layout_ = layout; loaded.tp_display_ = display;
                    loaded.text_[party_module::tp_text].style.scale = 0.6f; loaded.text_[party_module::tp_text].offset_y = 0;
                    const auto compactEntry = loaded.LayoutEntry({0, 0, 320, 0}, true, {true, true, true, true});
                    const auto noTP = loaded.ResourceBarHeight(true, false);
                    loaded.text_[party_module::tp_text].style.scale = 2; loaded.text_[party_module::tp_text].offset_y = 24;
                    Check(loaded.LayoutEntry({0, 0, 320, 0}, true, {true, true, true, true}).height > compactEntry.height &&
                        loaded.ResourceBarHeight(true, false) == noTP, "TP bars and number-only layouts reserve their own text size and offsets without changing rows where TP is absent");
                }
            loaded.text_[party_module::tp_text].style.scale = 0.6f; loaded.text_[party_module::tp_text].offset_y = 0;
            for (int layout = 0; layout < 3; ++layout)
            {
                loaded.layout_ = layout;
                loaded.text_[party_module::mp_text].style.scale = 0.6f; loaded.text_[party_module::mp_text].offset_y = 0;
                const auto compactMP = loaded.LayoutEntry({0, 0, 320, 0}, true, {true, true, true, true});
                const auto noMP = loaded.ResourceBarHeight(false, true);
                loaded.text_[party_module::mp_text].style.scale = 2; loaded.text_[party_module::mp_text].offset_y = 24;
                Check(loaded.LayoutEntry({0, 0, 320, 0}, true, {true, true, true, true}).height > compactMP.height &&
                    loaded.ResourceBarHeight(false, true) == noMP, "MP text size and offsets affect its measured layouts without changing rows where MP is absent");
            }
            loaded.ApplyPreset(0);
            Check(loaded.text_[party_module::tp_text].style.scale == 0.867f && loaded.text_[party_module::mp_text].style.scale == 0.867f &&
                loaded.bar_widths_[0] == 152 && loaded.bar_widths_[1] == 92 &&
                loaded.bar_widths_[2] == 28 && loaded.bar_positions_[1] == party_module::position_right,
                "Classic retains compact independent TP sizing and its established HP/MP/TP geometry");
            loaded.ApplyPreset(1);
            Check(loaded.text_[0].offset_x == 4 && loaded.text_[0].offset_y == 8 && loaded.cast_placement_ == party_module::cast_name &&
                loaded.bar_offsets_x_[3] == 0 && loaded.bar_offsets_y_[3] == 0, "XIUI casting inherits the name anchor without duplicate offsets");
        }

        static auto Preview(void) -> void
        {
            party_source source;
            source.player_id_ = 100;
            auto& player = source.members_[0];
            player.id = 100; player.active = player.in_zone = player.buffs_known = true;
            player.name = "Actual Player"; player.hp = 900; player.hp_percent = 100;
            player.buff_count = 13; player.buffs = {42, 43, 44, 46, 47, 48, 49, 50, 88, 89, 170, 434, 522};
            player.buff_expires[0] = ::GetTickCount64() + 90000;
            const auto original = player;
            party_module party; party.SetSource(&source);
            Check(!party.status_frame_ && !party.status_timers_ && !party.split_statuses_[0].timers &&
                !party.split_statuses_[1].timers && party.split_statuses_[0].size == 12 && party.split_statuses_[1].size == 12,
                "Party statuses default to twelve-pixel icons without containers or countdowns");
            Check(!party.status_preview_ && party.PreviewMember(1).buff_count == 0,
                "ordinary party preview contains no implicit sample status effects");
            const auto normalPreview = party.DisplayMembers(6, true, false);
            Check(normalPreview[0].buff_count == 13 && normalPreview[1].buff_count == 0,
                "full party preview preserves real effects without adding a second fabricated buff row");
            party.preview_ = false; party.status_preview_ = true;
            Check(party.DisplayCount(true) == 1 && party.StatusPreview(true) && !party.StatusPreview(false),
                "status preview works independently of the full six-member preview and is edit-only");
            const auto samples = party.DisplayMembers(1, false, true);
            const auto groups = party.StatusGroups(samples[0]);
            Check(samples[0].id == original.id && samples[0].name == original.name && samples[0].hp == original.hp &&
                groups[0].count == 4 && groups[1].count == 4 && samples[0].buff_expires[7] > ::GetTickCount64(),
                "status preview copies the shown member and supplies mixed effects with sample countdowns");
            const auto live = party.DisplayMembers(1, false, false);
            Check(live[0].buffs == original.buffs && live[0].buff_expires == original.buff_expires &&
                player.buffs == original.buffs && player.buff_expires == original.buff_expires,
                "sample effects never change the native source or normal gameplay display");

            party.status_columns_ = 2; party.status_row_spacing_ = 7; party.title_ = false;
            for (auto& settings : party.split_statuses_)
            { settings.columns = 2; settings.timers = false; settings.timer_placement = 0; }
            ui::design_system design;
            const module_render_context previewContext{design, nullptr, true};
            const auto snapshot = hud_snapshot{};
            for (int order = 0; order < 4; ++order)
            {
                party.status_order_ = order;
                const auto layout = party.LayoutRows(1, false, true);
                party.Render(snapshot, previewContext);
                Check(party.state_.bounds.height == layout.bounds.height && party.VisualBounds(true).height == layout.bounds.height + 16 &&
                    party.PlacementBounds(true).height == layout.bounds.height, "sample rendering, layout, edit and placement bounds use the same preview geometry");
                Check(party.status_hits_.size() == 8 && party.status_hits_[0].status.id == (order == 1 ? 3 : 33) &&
                    party.status_hits_[1].status.id == (order == 0 ? 3 : order == 1 ? 4 : 40),
                    "status samples follow native, debuffs-first, buffs-first and split display ordering");
                Check(!party.status_timers_ && !party.split_statuses_[0].timers && !party.split_statuses_[1].timers,
                    "sample timer visibility does not change saved gameplay countdown preferences");
            }
            const auto plainRows = party.SplitStatusLayouts({0, 0, 300, 0}, {}, {4, 4});
            for (auto& settings : party.split_statuses_) settings.timers = true;
            const auto timerRows = party.SplitStatusLayouts({0, 0, 300, 0}, {}, {4, 4});
            Check(timerRows[0].strip.height > plainRows[0].strip.height && timerRows[1].frame.y >= timerRows[0].bottom + party.split_status_gap_,
                "split previews reserve countdown height only when their timer switches are enabled");
            for (auto& settings : party.split_statuses_) settings.timers = false;
            party.split_statuses_[0].enabled = false;
            party.Render(snapshot, previewContext);
            Check(party.status_hits_.size() == 4 && std::all_of(party.status_hits_.begin(), party.status_hits_.end(),
                [](const auto& hit) { return ui::IsDebuffStatus(hit.status.id); }), "hiding Buff row shows only debuffs in the Party preview");
            const auto debuffsOnly = party.SplitStatusLayouts({0, 0, 300, 0}, {}, {4, 4});
            Check(debuffsOnly[0].strip.count == 0 && debuffsOnly[1].frame.y == 0,
                "a hidden buff row leaves no empty space before the debuffs");
            party.split_statuses_[0].enabled = true;
            const auto sampleHeading = party.LayoutRows(1, false, true);
            const auto headingHeight = party_module::LineHeight(party.text_[party_module::heading_text]) + 6;
            const auto paddingHeight = 2 * (party.container_.edge_size + party.container_.padding + party.SelectionPadding().y);
            Check(std::abs(sampleHeading.bounds.height - sampleHeading.heights[0] - paddingHeight - headingHeight) < 0.01f,
                "status preview reserves its identifying heading even when the normal party heading is hidden");

            party.SetSource(nullptr);
            Check(party.DisplayCount(true) == 1 && party.DisplayCount(false) == 0 && party.DisplayMembers(1, false, true)[0].buff_count == 8,
                "status preview supplies one dummy member when no real party entries are available");
            party.preview_ = true;
            const auto full = party.DisplayMembers(party.DisplayCount(true), true, true);
            Check(party.DisplayCount(true) == 6 && full[1].buff_count == 8 && !full[5].in_zone && full[5].buff_count == 0,
                "full party preview keeps six rows while only in-zone samples receive status effects");
            const auto store = settings_store::FromIniText("");
            party.Save(store);
            party_module loaded; loaded.Load(store);
            Check(loaded.status_preview_ && loaded.Option(party_module::status_preview_option).toggle == &loaded.status_preview_ &&
                loaded.ConfigSectionContains(party_module::status_section, party_module::status_preview_option),
                "the appended Party status-preview control persists and is reachable in Status Effects");
            party_module legacy;
            legacy.Load(settings_store::FromIniText("[status.party.buffs]\nsize=10\n[status.party.debuffs]\nsize=16\n"));
            Check(legacy.split_statuses_[0].size == 12 && legacy.split_statuses_[1].size == 16,
                "the old ten-pixel Party default migrates while custom sizes are retained");
            legacy.split_statuses_[0].size = 10; legacy.Save(store); loaded.Load(store);
            Check(loaded.split_statuses_[0].size == 10, "later icon-size choices survive the one-time default migration");
        }

        static auto Run(void) -> void
        {
            Timers();
            constexpr uint32_t utc = 1800000000;
            constexpr uint32_t ticks = (utc - 0x3C307D70u) * 60u;
            Check(NativeStatusDeadline(ticks + 90 * 60, utc, 1000) == 91000, "native status expiry converts 60Hz timestamps to monotonic milliseconds");
            Check(NativeStatusDeadline(0, utc, 1000) == 0 && NativeStatusDeadline(0x7FFFFFFF, utc, 1000) == 0 &&
                NativeStatusDeadline(ticks - 1, utc, 1000) == 0 && NativeStatusDeadline(ticks + 60, 0, 1000) == 0,
                "missing, hidden, expired and unclocked timers remain unknown");

            ui::status_strip_settings settings{};
            settings.size = 28; settings.spacing = 3; settings.columns = 2; settings.limit = 3;
            settings.offset_x = 9; settings.offset_y = 7; settings.placement = 2;
            const ui::rect area{20, 40, 100, 0};
            const auto layout = ui::LayoutStatusStrip(7, area, settings);
            Check(layout.count == 3 && layout.overflow == 4 && layout.columns == 2 && layout.Icon(2).y > layout.Icon(0).y,
                "scaled icons and overflow share the configured wrapped layout");
            Check(layout.bounds.x + layout.bounds.width == area.x + area.width + settings.offset_x && layout.bounds.y == 47,
                "status alignment and offsets use measured coordinates");
            const auto timedHeight = layout.height;
            settings.row_spacing = 11;
            const auto spaced = ui::LayoutStatusStrip(7, area, settings);
            Check(spaced.Icon(2).y - spaced.Icon(0).y == layout.Icon(2).y - layout.Icon(0).y + 9 && spaced.Icon(1).x == layout.Icon(1).x,
                "row spacing changes wrapped rows independently of horizontal icon spacing");
            module_text_profile timerText{ui::theme::CreateDefault().caption_text};
            const auto timerBefore = ui::StatusTimerBounds(layout.Icon(0), settings, timerText);
            timerText.offset_x = 4; timerText.offset_y = -9;
            const auto timerMoved = ui::StatusTimerBounds(layout.Icon(0), settings, timerText);
            Check(timerMoved.x == timerBefore.x + 4 && timerMoved.y == timerBefore.y - 9 && layout.Icon(0).y == 47,
                "timer text X/Y offsets move only countdown labels and preserve icon positions");
            settings.timer_placement = 2;
            const auto overlay = ui::LayoutStatusStrip(7, area, settings);
            timerText.offset_x = timerText.offset_y = 0;
            const auto overlayText = ui::StatusTimerBounds(overlay.Icon(0), settings, timerText);
            Check(overlay.height < timedHeight && overlayText.y >= overlay.Icon(0).y && overlayText.y + overlayText.height <= overlay.Icon(0).y + overlay.Icon(0).height,
                "over-icon timers draw within the icon and release the extra countdown row height");
            settings.timer_placement = 1;
            const auto above = ui::LayoutStatusStrip(7, area, settings);
            const auto aboveText = ui::StatusTimerBounds(above.Icon(0), settings, timerText);
            Check(aboveText.y >= above.bounds.y && aboveText.y + aboveText.height <= above.Icon(0).y && above.Icon(2).y >= above.Icon(0).y + above.Icon(0).height,
                "above-icon timers reserve space before icons and preserve wrapping");
            settings.timers = false;
            Check(ui::LayoutStatusStrip(7, area, settings).height < timedHeight, "hidden timers release their row space");
            std::array<combat_status, 7> effects{}; effects[0].id = 33;
            const auto hitLayout = ui::LayoutStatusStrip(effects.size(), area, settings);
            const auto at = hitLayout.Icon(0);
            const auto hit = ui::StatusStripHit(effects, area, settings, at.x + 1, at.y + 1);
            Check(hit && hit->status.id == 33 && !ui::StatusStripHit(effects, area, settings, area.x - 1, area.y - 1),
                "status hit regions match rendered scaled icons");
            settings.enabled = false;
            Check(ui::StatusStripHeight(effects.size(), area.width, settings) == 0, "hidden status strips reserve no space");

            const auto store = settings_store::FromIniText("");
            settings.enabled = true; settings.opacity = 0.45f; settings.Save(store, "status.test");
            ui::status_strip_settings restored{}; restored.Load(store, "status.test");
            Check(restored.size == 28 && restored.columns == 2 && restored.limit == 3 && !restored.timers &&
                restored.timer_placement == 1 && restored.row_spacing == 11 && std::abs(restored.opacity - 0.45f) < 0.001f, "status geometry, row spacing and timer placement preferences persist");
            ui::status_strip_settings legacy; legacy.Load(settings_store::FromIniText("[status.test]\nspacing=7\n"), "status.test");
            Check(legacy.row_spacing == 7, "old status settings preserve their previous vertical spacing");

            combat_source combat;
            combat.ApplyStatus(100, 2, 253, 236, 1000);
            combat.ApplyStatus(100, 3, 230, 2, 2000);
            Check(combat.Find(100)->statuses.size() == 1 && combat.Find(100)->statuses[0].expires == 61000 && combat.Find(100)->statuses[0].estimated,
                "confirmed effects use XIUI estimates without inferring secondary statuses from damage");
            settings.timers = true;
            combat.ApplyStatus(101, 1000, 0, 236, 2000);
            Check(ui::StatusTimerText(combat.Find(100)->statuses[0], 3000, settings) == "58" &&
                ui::StatusTimerText(combat.Find(101)->statuses[0], 3000, settings) == "?" &&
                ui::StatusTimerText({33}, 3000, settings).empty(), "observed unknown durations and hidden native timers stay distinct");
            settings.uncertainty = false;
            Check(ui::StatusTimerText(combat.Find(101)->statuses[0], 3000, settings).empty(), "unknown markers can be hidden without discarding effect state");

            Check(ui::IsDebuffStatus(1) && ui::IsDebuffStatus(3) && !ui::IsDebuffStatus(33) && !ui::IsDebuffStatus(173) &&
                !ui::IsDebuffStatus(251) && !ui::IsDebuffStatus(1000), "weakness and poison group as debuffs; haste, dread spikes, food and unknown effects remain visible with buffs");
            party_member member{};
            member.id = 100; member.in_zone = true; member.buff_count = 5;
            member.buffs = {33, 3, 40, 1, 251}; member.buff_expires = {10000, 11000, 12000, 13000, 14000};
            party_module party;
            party.status_columns_ = 2; party.status_spacing_ = 3; party.status_row_spacing_ = 12;
            const auto combined = party.StatusLayout({0, 0, 300, 0}, {0, 0, 300, 100}, 4);
            Check(party.StatusIconBounds(combined, 2).y - party.StatusIconBounds(combined, 0).y == party.buff_size_ + 12,
                "combined Party rows use their independent vertical spacing");
            for (int mode = 0; mode < 4; ++mode)
            {
                party.status_order_ = mode;
                std::set<std::string> labels;
                size_t sizes{}, rowSpacings{};
                for (size_t index = 0; index < party.OptionCount(); ++index)
                {
                    if (!party.ConfigSectionContains(party_module::status_section, index)) continue;
                    const auto option = party.Option(index);
                    Check(option.label != nullptr && labels.insert(option.label).second, "Party status modes expose no duplicate controls");
                    sizes += std::string_view{option.label} == "Status icon size";
                    rowSpacings += std::string_view{option.label} == "Status row spacing";
                }
                Check(sizes == 1 && rowSpacings == 1, "every Party status mode has exactly one size and one row-spacing control");
            }
            const auto groups = party.StatusGroups(member);
            Check(groups[0].count == 3 && groups[1].count == 2 && groups[0].effects[1].id == 40 && groups[1].effects[1].id == 1 &&
                groups[1].effects[1].expires == 13000, "party grouping preserves source order and paired native countdowns");
            party.status_order_ = 3;
            party.split_statuses_[0].size = 24; party.split_statuses_[0].columns = 2;
            party.split_statuses_[1].size = 16; party.split_statuses_[1].columns = 1;
            party.split_statuses_[1].offset_y = 6;
            const auto rows = party.SplitStatusLayouts({200, 100, 300, 0}, {200, 100, 300, 100}, {3, 2});
            Check(rows[0].strip.size == 24 && rows[1].strip.size == 16 && rows[1].frame.y >= rows[0].bottom + party.split_status_gap_,
                "independent split party rows reserve their full scaled geometry and gap");
            party.Save(store);
            party_module loaded; loaded.Load(store);
            Check(loaded.status_order_ == 3 && loaded.split_statuses_[0].size == 24 && loaded.split_statuses_[1].columns == 1,
                "party split mode and separate row customizations persist");
            Check(loaded.status_row_spacing_ == 12 && loaded.status_spacing_ == 3,
                "combined Party row and icon spacing persist independently while split mode is selected");
            const auto compactSettings = settings_store::FromIniText("[party]\nstatus_order=3\nstatus_placement=0\nstatus_icon_version=1\n"
                "[status.party.buffs]\nsize=10\ntimers=0\n[status.party.debuffs]\nsize=10\ntimers=0\n");
            party_module compact; compact.Load(compactSettings);
            const auto compactRows = compact.SplitStatusLayouts({200, 100, 300, 0}, {200, 100, 300, 100}, {3, 2});
            Check(compactRows[0].strip.size == 10 && compactRows[1].strip.size == 10 &&
                compactRows[0].frame.x < 200 && compactRows[1].frame.x < 200 &&
                ui::StatusTimerText({33, 0, 0, 10000}, 1000, compact.split_statuses_[0]).empty(),
                "compact Party split rows retain ten-pixel icons beside each entry with timers disabled");
            ui::status_strip_settings standalone;
            standalone.Load(settings_store::FromIniText("[status.test]\nsize=10\n"), "status.test");
            Check(standalone.size == 12, "other status strips retain their existing minimum icon size");
            for (int group = 0; group < 2; ++group)
            {
                party.selected_status_group_ = group;
                for (size_t option = 0; option < ui::status_strip_settings::option_count; ++option)
                {
                    const auto index = party_module::status_options_start + 5 + option;
                    const auto control = party.Option(index);
                    Check(control.available && control.label != nullptr && party.ConfigSectionContains(party_module::status_section, index),
                        "both split party rows expose every shared status setting");
                }
            }

            const std::vector<ui::status_tooltip> duplicates{
                {{33, 0, 0, 10000, false}, {10, 20, 18, 18}, 100},
                {{33, 0, 0, 20000, false}, {30, 20, 18, 18}, 100},
                {{40, 0, 0, 30000, false}, {50, 20, 18, 18}, 100}};
            target_module target;
            target.status_hits_ = duplicates;
            Check(target.BeginControllerStatus() && target.MoveControllerStatus(1) && target.ControllerStatusBounds().x == 30,
                "target inspection can focus the second copy of a native effect");
            auto selected = ui::StatusHitIndex(target.status_hits_, target.controller_status_entity_, target.controller_status_id_, target.controller_status_occurrence_);
            Check(selected && target.status_hits_[*selected].status.expires == 20000 && target.MoveControllerStatus(1) &&
                target.MoveControllerStatus(-1) && target.ControllerStatusBounds().x == 30,
                "target duplicate focus retains the second copy's timer in both navigation directions");
            target.status_hits_.erase(target.status_hits_.begin());
            Check(target.ControllerStatusBounds().width == 0, "a removed duplicate invalidates target inspection instead of selecting the wrong copy");

            party_source source;
            source.player_id_ = 100; source.members_[0] = member; source.members_[0].active = true;
            party.SetSource(&source); party.status_hits_ = duplicates;
            Check(party.BeginControllerBrowse(100) && party.BeginControllerStatus() && party.MoveControllerStatus(1) &&
                party.ControllerStatusBounds().x == 30, "party inspection can focus the second copy of a native effect");
            selected = ui::StatusHitIndex(party.status_hits_, party.controller_member_id_, party.controller_status_id_, party.controller_status_occurrence_);
            Check(selected && party.status_hits_[*selected].status.expires == 20000 && party.MoveControllerStatus(1) &&
                party.MoveControllerStatus(-1) && party.ControllerStatusBounds().x == 30,
                "party duplicate focus keeps its own native timer and returns to the correct icon");
            party.status_hits_.erase(party.status_hits_.begin());
            Check(party.ControllerStatusBounds().width == 0 && !party.MoveControllerStatus(1),
                "party inspection ends when its duplicate ordinal is no longer available");
        }
    };
}

namespace xiui
{
    auto RunXIUIStatusTimerChecks(void) -> void { status_display_test::Timers(); }
    auto RunStatusDisplayChecks(void) -> void { status_display_test::Run(); }
    auto RunPartyStatusPreviewChecks(void) -> void { status_display_test::Preview(); }
    auto RunPartyTextChecks(void) -> void { status_display_test::PartyText(); }
}
