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
        static auto AllSpells(void) -> void
        {
            using resources::spell_damage_kind;
            constexpr uint32_t gainMessages[]{82, 127, 141, 160, 164, 166, 186, 194, 203, 205, 230, 236, 237, 242, 243, 266, 267,
                268, 269, 270, 271, 272, 277, 278, 279, 280, 319, 320, 374, 375, 412, 645, 754, 755, 804};
            const auto hit = [](combat_source& source, uint32_t spell, uint32_t message, uint32_t parameter, uint64_t now,
                uint32_t additional = 0, uint32_t additionalMessage = 0) {
                combat_action action{}; action.actor = 1; action.category = 4; action.parameter = spell; action.target_count = 1;
                action.targets[0].id = 2; action.targets[0].result_count = 1;
                action.targets[0].results[0] = {parameter, message, additional, additionalMessage}; source.Observe(action, nullptr, now);
            };
            const auto effect = [](const combat_source& source, uint32_t status) -> const combat_status* {
                const auto* actor = source.Find(2); if (actor == nullptr) return nullptr;
                for (const auto& value : actor->statuses) if (value.id == status) return &value;
                return nullptr;
            };
            std::set<uint32_t> spells;
            size_t guaranteed{}, possible{};
            for (const auto& entry : resources::xiui_spell_status_rules)
            {
                if (!spells.insert(entry.spell).second) continue;
                const auto rules = SpellStatusRules(entry.spell);
                Check(std::count_if(rules.begin(), rules.end(), [](const auto& rule) { return rule.primary; }) == 1,
                    "every spell has exactly one primary effect or packet-supplied effect slot");
                const auto& primary = *std::find_if(rules.begin(), rules.end(), [](const auto& rule) { return rule.primary; });
                const uint32_t packet = primary.status != 0 ? primary.status : primary.display != 0 ? 2 : 7;
                const uint32_t display = primary.display != 0 ? primary.display : packet;
                combat_source confirmed;
                hit(confirmed, entry.spell, 236, packet, 1000);
                const auto* tracked = effect(confirmed, display);
                Check(tracked != nullptr && tracked->spell == entry.spell && tracked->expires == 1000 + uint64_t{primary.seconds} * 1000 &&
                    tracked->estimated && !tracked->unconfirmed, "every configured spell accepts explicit application with its mapped display and base duration");
                for (const auto message : gainMessages)
                {
                    combat_source variant; hit(variant, entry.spell, message, packet, 1000);
                    const auto* applied = effect(variant, display);
                    Check(applied != nullptr && applied->expires == 1000 + uint64_t{primary.seconds} * 1000 && !applied->unconfirmed,
                        "every spell keeps its mapped duration across application variants, including additional-effect messages");
                }
                if (rules.size() > 1 && primary.damage != spell_damage_kind::possible)
                    for (const auto& rule : rules) Check(effect(confirmed, rule.display) != nullptr, "confirmed compound enfeebles retain every mapped effect");
                for (const auto message : {75u, 85u, 655u})
                {
                    hit(confirmed, entry.spell, message, 0, 2000);
                    tracked = effect(confirmed, display);
                    Check(tracked != nullptr && tracked->observed == 1000 && tracked->expires == 1000 + uint64_t{primary.seconds} * 1000,
                        "failed reapplications do not refresh or erase a confirmed active effect");
                    combat_source failed; hit(failed, entry.spell, message, 0, 2000);
                    Check(failed.Find(2) == nullptr, "resist, no-effect and immunity messages never invent a status");
                }
                for (const auto& rule : rules)
                {
                    const uint32_t native = rule.status != 0 ? rule.status : rule.display != 0 ? 2 : 7;
                    const uint32_t shown = rule.display != 0 ? rule.display : native;
                    combat_source landed; hit(landed, rule.spell, 269, native, 1000);
                    Check(effect(landed, shown) != nullptr && effect(landed, shown)->expires == 1000 + uint64_t{rule.seconds} * 1000,
                        "each spell effect handles burst/AoE message variants with its own duration");
                    landed.ApplyStatus(2, native, 0, 206, 2000);
                    Check(effect(landed, shown) == nullptr, "every mapped display effect clears on its native wear-off ID");
                }
                if (primary.damage == spell_damage_kind::none)
                {
                    combat_source damage; hit(damage, entry.spell, 2, packet, 1000);
                    Check(damage.Find(2) == nullptr, "ordinary debuff spells require a status message rather than a damage-number coincidence");
                    continue;
                }
                if (primary.damage == spell_damage_kind::guaranteed) ++guaranteed; else ++possible;
                for (const auto message : {2u, 252u, 264u, 265u})
                {
                    combat_source damage; hit(damage, entry.spell | 0x10000, message, 17, 1000);
                    tracked = effect(damage, display);
                    Check(tracked != nullptr && tracked->spell == entry.spell && tracked->expires == 1000 + uint64_t{primary.seconds} * 1000 &&
                        tracked->unconfirmed == (primary.damage == spell_damage_kind::possible),
                        "every damage-applying spell handles ordinary, burst and additional-target results with explicit confidence");
                    Check(damage.Find(2)->statuses.size() == 1, "damage inference never creates damage-number icons or unreported compound effects");
                }
                combat_source zero; hit(zero, entry.spell, 2, 0, 1000);
                Check((effect(zero, display) != nullptr) == (primary.damage == spell_damage_kind::guaranteed),
                    "guaranteed effects allow zero damage; resistible secondaries require a connecting hit");
                if (primary.damage == spell_damage_kind::possible)
                {
                    combat_source candidate; hit(candidate, entry.spell, 2, 17, 1000);
                    ui::status_strip_settings settings;
                    Check(ui::StatusTimerText(*effect(candidate, display), 1000, settings).starts_with("?"), "unconfirmed secondary effects carry a visible uncertainty marker");
                    hit(candidate, entry.spell, 75, 0, 2000);
                    Check(effect(candidate, display) != nullptr && !effect(candidate, display)->unconfirmed && effect(candidate, display)->observed == 1000,
                        "no-effect evidence confirms an existing candidate without restarting its timer");
                    combat_source immune; hit(immune, entry.spell, 2, 17, 1000); hit(immune, entry.spell, 655, 0, 2000);
                    Check(effect(immune, display) == nullptr, "complete resistance removes only unconfirmed candidates");
                    combat_source explicitEffect; hit(explicitEffect, entry.spell, 2, 17, 1000, primary.status, 160);
                    Check(effect(explicitEffect, display) != nullptr && !effect(explicitEffect, display)->unconfirmed &&
                        effect(explicitEffect, display)->expires == 1000 + uint64_t{primary.seconds} * 1000,
                        "explicit additional effects confirm their spell-specific duration instead of using a generic proc timer");
                }
            }
            Check(spells.size() == resources::xiui_spell_count && spells.size() > 100, "packet checks cover every imported spell, not a hand-picked list");
            for (const auto message : {64u, 83u, 123u, 159u, 168u, 204u, 206u, 321u, 322u, 341u, 342u, 343u, 344u, 350u, 378u, 453u, 531u, 647u, 805u, 806u})
            {
                combat_source source; source.ApplyStatus(2, 4, 58, 236, 1000); source.ApplyStatus(2, 4, 0, message, 2000);
                Check(effect(source, 4) == nullptr, "all native removal variants clear the exact observed status");
            }
            for (const auto message : gainMessages)
            {
                combat_source source; source.ApplyStatus(2, 4, 58, message, 1000);
                Check(effect(source, 4) != nullptr, "all verified status-application message variants are accepted");
            }
            for (const auto spell : {98u, 259u, 274u})
            {
                combat_source source; source.ApplyStatus(2, 2, 253, 236, 1000); hit(source, spell, 236, 2, 2000);
                Check(effect(source, 19) != nullptr && effect(source, 2) == nullptr, "higher sleep variants replace the old icon without duplicate sleep entries");
                hit(source, 144, 2, 0, 3000); Check(effect(source, 19) != nullptr, "zero spell damage does not wake Sleep");
                hit(source, 144, 7, 20, 3000); Check(effect(source, 19) != nullptr, "healing is not mistaken for damage that wakes Sleep");
                hit(source, 144, 2, 20, 4000); Check(effect(source, 19) == nullptr, "positive spell damage removes stale sleep variants immediately");
            }
            {
                combat_source source; hit(source, 536, 236, 3, 1000); hit(source, 536, 2, 17, 2000);
                Check(effect(source, 3) != nullptr && !effect(source, 3)->unconfirmed && effect(source, 3)->observed == 1000,
                    "a possible secondary never downgrades or extends a confirmed active effect");
                combat_source phoenix; phoenix.SetRules(server_rules::phoenix); hit(phoenix, 536, 2, 17, 1000); phoenix.Prune(62000);
                Check(effect(phoenix, 3) == nullptr, "expired unconfirmed candidates do not persist as unknown effects on Phoenix");
            }
            for (const auto message : {6u, 20u, 97u, 113u, 406u, 605u, 646u})
            {
                combat_source source; source.ApplyStatus(2, 4, 58, 236, 1000); hit(source, 144, message, 0, 2000);
                Check(source.Find(2) == nullptr, "death variants clear all target spell observations");
            }
            {
                combat_source source; hit(source, 58, 236, 4, 1000); hit(source, 58, 546, 4, 2000); source.Prune(200000);
                Check(effect(source, 4) != nullptr && effect(source, 4)->expires == 0 && effect(source, 4)->uncertain,
                    "an observed extension with no deadline does not retain an obsolete expiry time");
                hit(source, 58, 453, 4, 210000);
                Check(effect(source, 4) == nullptr && source.Find(1) != nullptr && source.Find(1)->statuses.size() == 1 && source.Find(1)->statuses.front().id == 4,
                    "effect theft removes the target's effect and records the recipient's observed gain");
                combat_source full;
                for (uint32_t id = 300; id < 332; ++id) full.ApplyStatus(2, id, 0, 236, 1000);
                hit(full, 536, 2, 17, 2000);
                Check(full.Find(2)->statuses.size() == 32 && effect(full, 300) != nullptr && effect(full, 3) == nullptr,
                    "inferred secondary effects cannot evict confirmed effects from a full status cache");
            }
            std::printf("Spell audit: %zu spells; %zu guaranteed damage applications; %zu possible secondary effects.\n", spells.size(), guaranteed, possible);
        }

        static auto Timers(void) -> void
        {
            {
                ui::status_strip_settings settings;
                struct timer_case { unsigned seconds; const char* text; };
                const std::array<timer_case, 11> cases{{{1, "1"}, {59, "59"}, {60, "60"}, {61, "01:01"},
                    {119, "01:59"}, {120, "02:00"}, {3599, "59:59"}, {3600, "1h"}, {7199, "1h"}, {18540, "5h"}, {21420, "5h"}}};
                for (const auto& test : cases)
                    Check(ui::StatusTimerText({41, 0, 1000, 1000 + uint64_t{test.seconds} * 1000}, 1000, settings) == test.text,
                        "native countdowns show whole hours, then MM:SS under an hour and seconds alone at 60");
                Check(ui::StatusTimerText({41, 0, 1000, 61001}, 1000, settings) == "01:01",
                    "fractional seconds round up at the minute boundary");
                combat_status range{}; range.id = 4; range.estimated = true; range.earliest = 62000; range.expires = 121000;
                Check(ui::StatusTimerText(range, 1000, settings) == "01:01-02:00", "estimated ranges share the countdown format");
                range.earliest = 1000; range.unconfirmed = true;
                Check(ui::StatusTimerText(range, 1000, settings) == "?<02:00", "estimated bounds retain their uncertainty markers");
            }
            {
                combat_source effects;
                combat_action action{}; action.actor = 99; action.category = 4; action.target_count = 1;
                action.targets[0].id = 100; action.targets[0].result_count = 1;
                const auto cast = [&](uint32_t spell, uint32_t message, uint32_t damage, uint64_t now) {
                    action.parameter = spell; action.targets[0].results[0] = {damage, message}; effects.Observe(action, nullptr, now);
                };
                effects.ApplyStatus(100, 128, 235, 236, 1000);
                cast(23, 2, 8, 1000);
                Check(effects.Find(100)->statuses.size() == 2 && effects.Find(100)->statuses.back().id == 134 &&
                    effects.Find(100)->statuses.back().expires == 61000 && effects.Find(100)->statuses.back().estimated,
                    "Dia damage infers its own debuff and duration rather than treating damage as a status ID");
                for (const auto message : {75u, 85u, 655u}) cast(23, message, 134, 2000);
                Check(effects.Find(100)->statuses.back().observed == 1000, "failed and no-effect casts do not refresh Dia");
                cast(230, 252, 24, 3000);
                Check(effects.Find(100)->statuses.size() == 2 && effects.Find(100)->statuses.back().id == 135 &&
                    effects.Find(100)->statuses.back().expires == 63000 && effects.Find(100)->statuses.front().id == 128,
                    "Bio's damage result replaces Dia while retaining unrelated debuffs");
                cast(23, 2, 0, 4000);
                Check(effects.Find(100)->statuses.back().id == 135 && effects.Find(100)->statuses.back().observed == 3000,
                    "a lower-priority Dia hit cannot replace a known active Bio");
                cast(24, 264, 12, 5000); cast(231, 265, 0, 6000);
                Check(effects.Find(100)->statuses.back().id == 135 && effects.Find(100)->statuses.back().spell == 231 &&
                    effects.Find(100)->statuses.back().expires == 126000, "higher Dia/Bio tiers replace each other in priority order, including zero damage");
                effects.ApplyStatus(100, 135, 0, 206, 7000);
                Check(effects.Find(100)->statuses.size() == 1, "Bio wear-off removes only Bio");
                cast(144, 2, 134, 8000);
                Check(effects.Find(100)->statuses.size() == 1, "unrelated spell damage never creates Dia or arbitrary damage-number icons");
                action.category = 3; cast(23, 2, 134, 9000);
                Check(effects.Find(100)->statuses.size() == 1, "non-spell actions cannot infer a damage-over-time spell status");
                action.category = 4; action.target_count = 3; action.parameter = 0x10000 | 33;
                for (size_t i = 0; i < 3; ++i) { action.targets[i].id = 200 + static_cast<uint32_t>(i); action.targets[i].result_count = 1; action.targets[i].results[0] = {0, i == 2 ? 85u : 2u}; }
                effects.Observe(action, nullptr, 10000);
                Check(effects.Find(200)->statuses.front().id == 134 && effects.Find(201)->statuses.front().spell == 33 && effects.Find(202) == nullptr,
                    "Diaga tracks each successful recipient while skipping resisted recipients and masking packet flags");
            }
            {
                ui::design_system design;
                module_text_profile profile{ui::theme::CreateDefault().caption_text}; profile.style.scale = 38 / ui::text_base_pixels;
                profile.style.glow.enabled = profile.style.shadow.enabled = true;
                ui::status_strip_settings settings;
                const std::array<combat_status, 5> effects{{{128, 235, 1000, 12000, false, true}, {3, 0, 1000, 66000, false, true},
                    {41, 0, 1000, 3721000, false, true}, {4, 58, 1000, 92000, false, true, 2000}, {1000, 0, 1000, 0, true}}};
                const auto caption = ui::StatusTimerProfile(profile, settings);
                Check(caption.style.scale == profile.style.scale * settings.timer_scale && caption.style.glow.enabled && caption.style.shadow.enabled &&
                    caption.style.top_color == profile.style.top_color && caption.style.bottom_color == profile.style.bottom_color,
                    "large countdown text retains its requested scale, gradient and normal effects");
                for (const int placement : {0, 1, 2})
                {
                    settings.timer_placement = placement;
                    const auto layout = ui::LayoutStatusStrip(effects, {10, 20, 220, 0}, 1000, profile, design, settings);
                    Check(layout.cell_width > settings.size && layout.columns < effects.size(), "large timer labels reserve wider cells and wrap the icon strip");
                    for (size_t i = 0; i < effects.size(); ++i)
                    {
                        const auto icon = layout.Icon(i), timer = ui::StatusTimerBounds(icon, settings, profile, layout.cell_width, layout.timer_height);
                        const auto text = ui::StatusTimerText(effects[i], 1000, settings);
                        Check(design.MeasureText(text.c_str(), caption.style) <= timer.width && design.TextHeight(caption.style) <= timer.height,
                            "seconds, minutes, hours, duration ranges and unknown markers fit without per-label font shrinking");
                        Check(timer.y >= layout.bounds.y && timer.y + timer.height <= layout.bounds.y + layout.height && icon.width == settings.size,
                            "every timer placement reserves enough row height without resizing its icon");
                        if ((i + 1) % layout.columns != 0 && i + 1 < effects.size())
                        {
                            const auto next = ui::StatusTimerBounds(layout.Icon(i + 1), settings, profile, layout.cell_width, layout.timer_height);
                            Check(next.x >= timer.x + timer.width, "neighboring large countdowns do not overlap");
                        }
                    }
                }
            }
            Check(XIUIStatusSeconds(4, 58, 4, 236) == 120 && XIUIStatusSeconds(4, 254, 5, 236) == 180 &&
                XIUIStatusSeconds(4, 249, 34, 230) == 180, "XIUI+ supplies Paralyze, Blind and Blaze Spikes durations");
            Check(XIUIStatusSeconds(6, 682, 149, 127) == 30 && XIUIStatusSeconds(3, 170, 148, 242) == 120 &&
                XIUIStatusSeconds(13, 523, 16, 236) == 15 && XIUIStatusSeconds(0, 0, 2, 160) == 25,
                "ability and weapon IDs stay separate; per-effect pet overrides and additional effects retain XIUI+ values");
            combat_source source;
            ui::status_strip_settings settings;
            source.ApplyStatus(100, 4, 58, 236, 1000);
            const auto paralyze = source.Find(100)->statuses.front();
            Check(paralyze.estimated && paralyze.expires == 121000 && ui::StatusTimerText(paralyze, 1000, settings) == "02:00",
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
                source.Find(101)->statuses.front().expires == 0, "estimated mob buffs expire while effects absent from XIUI+ remain untimed");
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
                loaded.bar_offsets_x_[3] == 0 && loaded.bar_offsets_y_[3] == 0, "XIUI+ casting inherits the name anchor without duplicate offsets");
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
                "confirmed effects use XIUI+ estimates without inferring secondary statuses from damage");
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
    auto RunAllSpellStatusChecks(void) -> void { status_display_test::AllSpells(); }
    auto RunStatusDisplayChecks(void) -> void { status_display_test::Run(); }
    auto RunPartyStatusPreviewChecks(void) -> void { status_display_test::Preview(); }
    auto RunPartyTextChecks(void) -> void { status_display_test::PartyText(); }
}
