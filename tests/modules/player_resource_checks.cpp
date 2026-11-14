#include "modules/player/player_module.hpp"
#include "modules/party/party_source.hpp"
#include "ui/health_colors.hpp"
#include "ui/xiv1_resource_layout.hpp"

#include <cstdio>
#include <stdexcept>

namespace
{
    auto Check(const bool value, const char* message) -> void
    { if (!value) throw std::runtime_error{message}; }
}

namespace xiui
{
    struct player_resource_test
    {
        static auto SetLocal(party_source& source, const party_member& member) -> void
        { source.player_id_ = member.id; source.members_[0] = member; }
    };
}

    auto RunPlayerResourceChecks(void) -> bool
    {
        using namespace xiui;
        try
        {
            D3DCOLOR top = 0xFF723638, bottom = 0xFFFF797D; ui::Xiv1ResourceFill(0, top, bottom);
            Check(top == 0xFFD8C1B6 && bottom == 0xFFBC7160, "XIV 1.0 replaces default HP fills with its palette");
            top = 0xFF102030; bottom = 0xFFFF797D; ui::Xiv1ResourceFill(0, top, bottom);
            Check(top == 0xFF102030 && bottom == 0xFFBC7160, "customized HP Fill top survives the XIV 1.0 palette");
            top = 0xFF556438; bottom = 0xFF405060; ui::Xiv1ResourceFill(1, top, bottom);
            Check(top == 0xFFD5DDB0 && bottom == 0xFF405060, "customized MP Fill bottom survives the XIV 1.0 palette");
            top = 0xFF723638; bottom = 0xFFFF797D; ui::Xiv1ResourceFill(2, top, bottom);
            Check(top == 0xFF723638 && bottom == 0xFFFF797D, "TP fills are not replaced by the HP/MP palette");
            player_module defaults;
            defaults.Load(settings_store::FromIniText("[player]\nhp_display=4\nmp_display=4\n"));
            Check(defaults.Settings().hp_display == 0 && defaults.Settings().mp_display == 0,
                "old default resource fractions migrate to current numbers");
            auto current = settings_store::FromIniText(""); defaults.Save(current);
            current.WriteInt("player", "hp_display", 4); defaults.Load(current);
            Check(defaults.Settings().hp_display == 4 && defaults.Settings().mp_display == 0,
                "later resource format choices remain authoritative");
            for (const auto [percent, color] : std::array<std::pair<uint32_t, D3DCOLOR>, 9>{
                {{100, 0xFFFFFFFF}, {75, 0xFFFFFFFF}, {74, 0xFFFFFF00}, {50, 0xFFFFFF00},
                 {49, 0xFFFFA500}, {25, 0xFFFFA500}, {24, 0xFFFF0000}, {1, 0xFFFF0000}, {0, 0xFFFF0000}}})
                Check(ui::HPValueColor(percent) == color, "native HP value zones use strict 75/50/25 boundaries, including KO");
            module_text_profile hpText;
            hpText.style.top_color = 0xCCFFFFFF; hpText.style.bottom_color = 0x88999999;
            hpText.style.glow.enabled = hpText.style.stroke.enabled = hpText.style.shadow.enabled = true;
            hpText.offset_x = 7; hpText.offset_y = -3; hpText.placement = 2;
            const auto tintedHP = TintTextProfile(hpText, ui::HPValueColor(49));
            Check(tintedHP.style.top_color == 0xCCFFA500 && tintedHP.style.bottom_color == 0x88996300 &&
                tintedHP.style.glow.enabled && tintedHP.style.stroke.enabled && tintedHP.style.shadow.enabled &&
                tintedHP.offset_x == 7 && tintedHP.offset_y == -3 && tintedHP.placement == 2 && hpText.style.top_color == 0xCCFFFFFF,
                "native HP value tint preserves gradient brightness, alpha, effects and placement without changing the saved profile");
            const char* expected[]{"1250", "62%", "1250 (62%)", "62% (1250)", "1250/2000", ""};
            for (int mode = 0; mode < 6; ++mode)
                Check(FormatPlayerResource(1250, 2000, mode) == expected[mode], "all six Player formats use exact native maxima");
            Check(FormatPlayerResource(0, 0, 1) == "0%" && FormatPlayerResource(0, 0, 4) == "0/?" &&
                FormatPlayerResource(0xFFFFFFFF, 0xFFFFFFFF, 1) == "100%", "zero and large resource maxima are safe");
            Check(PlayerHPState(501, 1000, 50, 25) == player_hp_state::normal &&
                PlayerHPState(500, 1000, 50, 25) == player_hp_state::warning &&
                PlayerHPState(251, 1000, 50, 25) == player_hp_state::warning &&
                PlayerHPState(250, 1000, 50, 25) == player_hp_state::critical &&
                PlayerHPState(0, 1000, 50, 25) == player_hp_state::critical &&
                PlayerHPState(0, 0, 50, 25) == player_hp_state::normal, "HP warning/critical boundaries are inclusive and unknown maxima are neutral");

            hud_snapshot oldJob{};
            oldJob.hp = 339; oldJob.hp_max = 3729; oldJob.mp = 352; oldJob.mp_max = 3872;
            party_source source;
            party_member local{}; local.id = 1; local.active = local.in_zone = true;
            local.hp = 339; local.mp = 352; local.hp_percent = local.mp_percent = 100;
            local.hp_max = LocalResourceMaximum(local.hp, local.hp_percent, oldJob.hp_max);
            local.mp_max = LocalResourceMaximum(local.mp, local.mp_percent, oldJob.mp_max);
            local.hp_max_exact = local.mp_max_exact = true;
            player_resource_test::SetLocal(source, local);
            player_module changedJob; changedJob.SetSource(&source);
            auto resources = changedJob.Resources(oldJob);
            Check(resources[0].maximum == 339 && resources[1].maximum == 352 && resources[0].ratio == 1 && resources[1].ratio == 1,
                "job changes use native full resources instead of stale previous-job maxima");
            Check(LocalResourceMaximum(250, 73, 3729, 339) == 339 && LocalResourceMaximum(176, 50, 3872, 352) == 352 &&
                LocalResourceMaximum(250, 25, 1000, 339) == 1000 && LocalResourceMaximum(250, 73, 3729) == 0,
                "confirmed full maxima survive damage or spending, refreshed native maxima win, and unknown maxima are not guessed");
            local.hp = 250; local.hp_percent = 73; local.hp_max_exact = false;
            local.mp = 176; local.mp_percent = 50; local.mp_max_exact = false;
            player_resource_test::SetLocal(source, local);
            resources = changedJob.Resources(oldJob);
            Check(resources[0].maximum == 0 && std::abs(resources[0].ratio - 0.73f) < 0.0001f && resources[0].percent == 73 &&
                resources[1].maximum == 0 && resources[1].ratio == 0.5f &&
                FormatPartyResource(resources[0].current, resources[0].percent, resources[0].maximum, true, 4) == "250/?",
                "unknown maxima retain native fill and percentages without presenting estimated totals as exact");
            changedJob.ResetDefaults(ui::theme::CreateDefault());
            Check(changedJob.Resources(oldJob)[0].percent == 73, "resetting Player preferences preserves its native resource source");
            source.Clear();
            Check(changedJob.Resources(oldJob)[0].maximum == 3729, "unavailable native party data retains snapshot fallback");

            const auto store = settings_store::FromIniText("");
            player_module player;
            auto label = player.Settings().label_text, value = player.Settings().value_text;
            label.offset_x = 9; label.offset_y = -4; label.style.font = 1; label.style.glow.enabled = true;
            value.offset_x = -7; value.offset_y = 12; value.style.scale = 1.25f; value.style.stroke.enabled = true;
            value.style.top_color = 0xAABBCCDD; value.style.bottom_color = 0x88443322;
            SaveTextProfile(store, "text.player.labels", label); SaveTextProfile(store, "text.player.values", value);
            player.Load(store);
            auto& settings = player.Settings();
            Check(settings.mp_label_text.offset_x == 9 && settings.tp_label_text.offset_y == -4 && settings.mp_label_text.style.font == 1 &&
                settings.tp_label_text.style.glow.enabled && settings.mp_value_text.offset_x == -7 && settings.tp_value_text.offset_y == 12 &&
                settings.mp_value_text.style.stroke.enabled && settings.tp_value_text.style.top_color == value.style.top_color &&
                settings.mp_value_text.style.bottom_color == value.style.bottom_color, "legacy shared profiles seed every resource with fonts, gradients, effects and offsets intact");
            settings.mp_value_text.style.scale = 1.5f; settings.mp_value_text.offset_y = 19;
            settings.tp_label_text.offset_x = 23; settings.tp_value_text.style.bottom_color = 0x80775533;
            settings.hp_display = 2; settings.mp_display = 3;
            settings.hp_state_colors = true; settings.hp_state_text = true; settings.hp_warning = 55; settings.hp_critical = 17;
            settings.hp_warning_top = 0x80445566; settings.hp_critical_bottom = 0x88776655;
            settings.tp_ready_text = true; settings.tp_ready_color = 0x99EEDDCC;
            settings.tp_palette.custom_overlay = true; settings.tp_palette.overlay_top = 0xAA112233;
            settings.tp_palette.overlay_bottom = 0xBB445566; settings.tp_palette.flash = 0x8000FF00;
            player.Save(store);
            value.style.scale = 0.6f; SaveTextProfile(store, "text.player.values", value);
            player_module restored; restored.Load(store);
            auto& loaded = restored.Settings();
            Check(store.ReadInt("player", "resource_text_version", 0) == 1 && loaded.value_text.style.scale == 0.6f &&
                loaded.mp_value_text.style.scale == 1.5f && loaded.mp_value_text.offset_y == 19 && loaded.tp_label_text.offset_x == 23 &&
                loaded.tp_value_text.style.bottom_color == 0x80775533, "later per-resource profile edits survive reload independently");
            Check(loaded.hp_display == 2 && loaded.mp_display == 3 && loaded.hp_state_colors && loaded.hp_state_text &&
                loaded.hp_warning == 55 && loaded.hp_critical == 17 && loaded.hp_warning_top == 0x80445566 && loaded.hp_critical_bottom == 0x88776655 &&
                loaded.tp_ready_text && loaded.tp_ready_color == 0x99EEDDCC && loaded.tp_palette.custom_overlay &&
                loaded.tp_palette.overlay_top == 0xAA112233 && loaded.tp_palette.overlay_bottom == 0xBB445566 && loaded.tp_palette.flash == 0x8000FF00,
                "resource formats, state thresholds and alpha-aware TP/HP colors persist");
            bool visitedMP{}, visitedTP{};
            restored.VisitTextSettings([&](ui::text_style& style) {
                if (&style == &loaded.mp_value_text.style) visitedMP = true;
                if (&style == &loaded.tp_label_text.style) visitedTP = true;
            });
            Check(visitedMP && visitedTP, "new profiles participate in shared global text changes");
            const auto selector = restored.Option(6);
            Check(selector.choice_count == 10 && std::string_view{selector.choices[2]} == "HP label" &&
                std::string_view{selector.choices[7]} == "MP values" && std::string_view{selector.choices[9]} == "TP values", "resource text controls retain existing selector identities");

            loaded.hp_style.appearance = loaded.mp_style.appearance = loaded.tp_style.appearance = ui::meter_style::modern;
            loaded.hp_style.height = 7; loaded.mp_style.height = 17; loaded.tp_style.height = 27;
            loaded.style_edit_mode = 0; loaded.selected_bar = 0;
            restored.OnOptionChanged(4);
            Check(loaded.hp_style.height == 7 && loaded.mp_style.height == 17 && loaded.tp_style.height == 27,
                "selecting All bars does not copy or reset resource styles");
            const auto bookends = restored.Option(11);
            Check(bookends.kind == module_option_kind::toggle && bookends.toggle != nullptr, "Bookends exposes its shared toggle control");
            *bookends.toggle = false; restored.OnOptionChanged(11);
            Check(!loaded.hp_style.bookends && !loaded.mp_style.bookends && !loaded.tp_style.bookends && loaded.mp_style.height == 17 && loaded.tp_style.height == 27,
                "All bars changes only the edited property");
            size_t paired{};
            for (size_t i = 0; i < restored.OptionCount(); ++i)
            {
                const auto option = restored.Option(i);
                if (option.kind == module_option_kind::color && option.pair_next) ++paired;
                bool reachable{};
                for (size_t section = 0; section < restored.ConfigSections().size(); ++section) reachable |= restored.ConfigSectionContains(section, i);
                Check(reachable, "every Player option belongs to a controller-accessible settings topic");
            }
            Check(paired >= 4, "base, HP state and TP overlay gradients retain paired color rows");

            const auto base = ui::TPMeterLayers(2500.0f / 3000, 0xAA102030, 0xBB405060, 0, false);
            ui::tp_meter_colors palette{};
            const auto unchanged = ui::TPMeterLayers(2500.0f / 3000, 0xAA102030, 0xBB405060, 0, false, &palette);
            Check(base[1].top == unchanged[1].top && base[2].bottom == unchanged[2].bottom, "default TP palette retains the existing staged colors");
            palette.custom_overlay = true; palette.overlay_top = 0x8A204060; palette.overlay_bottom = 0x9B80A0C0; palette.flash = 0xFF00FF00;
            const auto custom = ui::TPMeterLayers(2500.0f / 3000, 0xAA102030, 0xBB405060, 0, false, &palette);
            Check(custom[0].ratio == 1 && custom[1].ratio == 1 && std::abs(custom[2].ratio - 0.5f) < 0.0001f &&
                custom[0].top == base[0].top && custom[1].top == palette.overlay_top && custom[2].top != custom[1].top,
                "custom overlays preserve all three TP stages and a distinct final layer");
            const auto flash = ui::TPMeterLayers(2500.0f / 3000, 0xAA102030, 0xBB405060, 0.5, true, &palette);
            const auto belowReady = ui::TPMeterLayers(999.0f / 3000, 0xAA102030, 0xBB405060, 0.5, true, &palette);
            Check(flash[0].top != custom[0].top && (flash[0].top >> 24) == 0xAA && belowReady[0].top == base[0].top,
                "custom flash affects ready colors while preserving alpha and stops below readiness");

            hud_snapshot snapshot{}; snapshot.hp = 250; snapshot.hp_max = 1000; snapshot.mp = 60; snapshot.mp_max = 100; snapshot.tp = 1500;
            ui::design_system design; const module_render_context context{design};
            const auto savedHP = loaded.hp_color, savedText = loaded.value_text.style.top_color;
            restored.Render(snapshot, context);
            const auto originalHeight = restored.State().minimum_height;
            loaded.mp_value_text.style.scale = 2; loaded.mp_value_text.offset_y = 30;
            restored.Render(snapshot, context);
            Check(restored.State().minimum_height > originalHeight && restored.State().content_bounds.height > 0 &&
                loaded.hp_color == savedHP && loaded.value_text.style.top_color == savedText,
                "independent large text reserves a measured footprint and state rendering does not overwrite saved colors");
            snapshot.mp = snapshot.mp_max = 0;
            Check(!restored.ShowsMP(snapshot) && restored.MPMeterBounds(snapshot).width == 0, "automatic MP hiding keeps empty caster MP distinct from no MP maximum");
            design.SetAppearanceTheme(ui::xiv1_appearance);
            loaded.show_labels = false; loaded.show_values = true;
            loaded.tp_value_text.visible = true;
            restored.Render(snapshot, context);
            const auto track = restored.TPMeterBounds(snapshot);
            auto& timing = restored.TimingSettings();
            timing.attack_layout = timing.recovery_layout = 2;
            timing.mini_gap = timing.recovery_mini_gap = timing.offset_y = 0;
            const auto attack = restored.AttackTimerBounds(snapshot), recovery = restored.RecoveryTimerBounds(snapshot);
            Check(std::abs(attack.y + attack.height - track.y) < .001f &&
                std::abs(recovery.y + recovery.height - restored.HPMeterBounds(snapshot).y) < .001f,
                "mini attack and recovery tracks touch the top of the XIV 1.0 TP and HP tracks");
            loaded.tp_value_text.visible = false; restored.Render(snapshot, context);
            Check(std::abs(restored.AttackTimerBounds(snapshot).y - attack.y) < .001f,
                "resource text visibility does not move the mini timer anchor");
            return true;
        }
        catch (const std::exception& error)
        {
            std::fprintf(stderr, "Player resource check failed: %s\n", error.what());
            return false;
        }
    }
