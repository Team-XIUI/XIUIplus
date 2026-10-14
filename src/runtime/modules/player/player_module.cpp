#include "player_module.hpp"
#include "modules/bar_config.hpp"

#include <algorithm>
#include <cstdio>
#include "ui/sound_choices.hpp"

namespace xiui
{
    namespace
    {
        auto PlayerMeterBounds(const ui::rect& row, const ui::meter_style& style) -> ui::rect
        {
            const float height = (style.height > 0 ? style.height : std::clamp(row.height, 14.0f, 30.0f) - 2) + 2;
            return {row.x, row.y + std::max(8.0f, (row.height - height) * 0.5f), row.width, height};
        }
    }

    auto player_module::Id(void) const -> const char* { return "player"; }
    auto player_module::DisplayName(void) const -> const char* { return "Player Bar"; }
    auto player_module::State(void) -> module_state& { return this->state_; }
    auto player_module::State(void) const -> const module_state& { return this->state_; }
    auto player_module::Settings(void) -> settings& { return this->settings_; }

    auto player_module::ApplyMeterDefaults(const ui::meter_style& style) -> void
    {
        this->settings_.hp_style = style;
        this->settings_.mp_style = style;
        this->settings_.tp_style = style;
    }

    auto player_module::ApplyTextDefaults(const ui::theme& theme) -> void
    {
        this->timers_.attack_text = {theme.caption_text};
        this->timers_.rest_text = {theme.caption_text};
        this->settings_.name_text = {theme.title_text};
        this->settings_.jobs_text = {theme.caption_text};
        this->settings_.jobs_text.style.alignment = ui::text_alignment::right;
        this->settings_.label_text = {theme.meter_text};
        this->settings_.label_text.style.alignment = ui::text_alignment::left;
        this->settings_.value_text = {theme.meter_text};
        this->settings_.value_text.style.alignment = ui::text_alignment::right;
    }

    auto player_module::ResetDefaults(const ui::theme& themeDefaults) -> void
    {
        const ui::rect bounds = this->state_.bounds;
        const auto* timing = this->timing_; auto* sounds = this->sounds_;
        *this = player_module{};
        this->timing_ = timing; this->sounds_ = sounds;
        this->state_.bounds = bounds;
        this->ApplyMeterDefaults(themeDefaults.meter);
        this->ApplyTextDefaults(themeDefaults);
    }

    auto player_module::SelectedFillTop(void) -> D3DCOLOR*
    {
        if (this->settings_.selected_bar == 1)
            return &this->settings_.mp_color_top;
        if (this->settings_.selected_bar == 2)
            return &this->settings_.tp_color_top;
        return &this->settings_.hp_color_top;
    }

    auto player_module::SelectedFillBottom(void) -> D3DCOLOR*
    {
        if (this->settings_.selected_bar == 1)
            return &this->settings_.mp_color;
        if (this->settings_.selected_bar == 2)
            return &this->settings_.tp_color;
        return &this->settings_.hp_color;
    }

    auto player_module::SelectedStyle(void) -> ui::meter_style&
    {
        if (this->settings_.selected_bar == 1)
            return this->settings_.mp_style;
        if (this->settings_.selected_bar == 2)
            return this->settings_.tp_style;
        return this->settings_.hp_style;
    }

    auto player_module::SelectedTextProfile(void) -> module_text_profile&
    {
        if (this->settings_.selected_text == 4) return this->timers_.attack_text;
        if (this->settings_.selected_text == 5) return this->timers_.rest_text;
        if (this->settings_.selected_text == 1)
            return this->settings_.jobs_text;
        if (this->settings_.selected_text == 2)
            return this->settings_.label_text;
        if (this->settings_.selected_text == 3)
            return this->settings_.value_text;
        return this->settings_.name_text;
    }

    auto player_module::SyncSelectedStyleToAll(void) -> void
    {
        const ui::meter_style shared = this->SelectedStyle();
        this->settings_.hp_style = shared;
        this->settings_.mp_style = shared;
        this->settings_.tp_style = shared;
    }

    auto player_module::Load(const settings_store& store) -> void
    {
        this->timers_.Load(store);
        this->state_.enabled = store.ReadBool("player", "enabled", true);
        this->settings_.show_identity = store.ReadBool("player", "show_identity", true);
        this->settings_.show_labels = store.ReadBool("player", "show_labels", true);
        this->settings_.show_values = store.ReadBool("player", "show_values", true);
        this->settings_.hide_zero_mp = store.ReadBool("player", "hide_zero_mp", true);
        this->settings_.tp_ready_blink = store.ReadBool("player", "tp_ready_blink", true);
        const D3DCOLOR hpFallback = store.ReadArgb("player", "hp_color", D3DCOLOR_XRGB(255, 121, 125));
        const D3DCOLOR mpFallback = store.ReadArgb("player", "mp_color", D3DCOLOR_XRGB(191, 224, 125));
        const D3DCOLOR tpFallback = store.ReadArgb("player", "tp_color", D3DCOLOR_XRGB(120, 196, 238));
        this->settings_.hp_color_top = store.ReadArgb("meter.player.hp", "fill_top", MeterGradientTop(hpFallback));
        this->settings_.hp_color = store.ReadArgb("meter.player.hp", "fill_bottom", hpFallback);
        this->settings_.mp_color_top = store.ReadArgb("meter.player.mp", "fill_top", MeterGradientTop(mpFallback));
        this->settings_.mp_color = store.ReadArgb("meter.player.mp", "fill_bottom", mpFallback);
        this->settings_.tp_color_top = store.ReadArgb("meter.player.tp", "fill_top", MeterGradientTop(tpFallback));
        this->settings_.tp_color = store.ReadArgb("meter.player.tp", "fill_bottom", tpFallback);
        this->settings_.hp_style = LoadMeterStyle(store, "meter.player.hp", this->settings_.hp_style);
        this->settings_.mp_style = LoadMeterStyle(store, "meter.player.mp", this->settings_.mp_style);
        this->settings_.tp_style = LoadMeterStyle(store, "meter.player.tp", this->settings_.tp_style);
        this->settings_.name_text = LoadTextProfile(store, "text.player.name", this->settings_.name_text);
        this->settings_.jobs_text = LoadTextProfile(store, "text.player.jobs", this->settings_.jobs_text);
        this->settings_.label_text = LoadTextProfile(store, "text.player.labels", this->settings_.label_text);
        this->settings_.value_text = LoadTextProfile(store, "text.player.values", this->settings_.value_text);
        const int savedEditMode = store.ReadInt("player", "style_edit_mode", -1);
        const bool hasExistingProfiles = store.ReadInt("meter.player.hp", "style_version", 0) > 0 ||
            store.ReadInt("meter.player.mp", "style_version", 0) > 0 ||
            store.ReadInt("meter.player.tp", "style_version", 0) > 0;
        this->settings_.style_edit_mode = savedEditMode >= 0 ? std::clamp(savedEditMode, 0, 1) : (hasExistingProfiles ? 1 : 0);
        this->settings_.selected_bar = std::clamp(store.ReadInt("player", "selected_bar", 0), 0, 2);
        this->settings_.selected_text = std::clamp(store.ReadInt("player", "selected_text", 0), 0, 5);
        this->hp_animation_ = {};
        this->mp_animation_ = {};
        this->tp_animation_ = {};
        this->state_.bounds = store.ReadRect("layout.player", {24.0f, 24.0f, 640.0f, 88.0f});
    }

    auto player_module::Save(const settings_store& store) const -> void
    {
        this->timers_.Save(store);
        store.WriteBool("player", "enabled", this->state_.enabled);
        store.WriteBool("player", "show_identity", this->settings_.show_identity);
        store.WriteBool("player", "show_labels", this->settings_.show_labels);
        store.WriteBool("player", "show_values", this->settings_.show_values);
        store.WriteBool("player", "hide_zero_mp", this->settings_.hide_zero_mp);
        store.WriteBool("player", "tp_ready_blink", this->settings_.tp_ready_blink);
        store.WriteInt("player", "style_edit_mode", this->settings_.style_edit_mode);
        store.WriteInt("player", "selected_bar", this->settings_.selected_bar);
        store.WriteInt("player", "selected_text", this->settings_.selected_text);
        store.WriteArgb("player", "hp_color", this->settings_.hp_color);
        store.WriteArgb("player", "mp_color", this->settings_.mp_color);
        store.WriteArgb("player", "tp_color", this->settings_.tp_color);
        store.WriteArgb("meter.player.hp", "fill_top", this->settings_.hp_color_top);
        store.WriteArgb("meter.player.hp", "fill_bottom", this->settings_.hp_color);
        store.WriteArgb("meter.player.mp", "fill_top", this->settings_.mp_color_top);
        store.WriteArgb("meter.player.mp", "fill_bottom", this->settings_.mp_color);
        store.WriteArgb("meter.player.tp", "fill_top", this->settings_.tp_color_top);
        store.WriteArgb("meter.player.tp", "fill_bottom", this->settings_.tp_color);
        SaveMeterStyle(store, "meter.player.hp", this->settings_.hp_style);
        SaveMeterStyle(store, "meter.player.mp", this->settings_.mp_style);
        SaveMeterStyle(store, "meter.player.tp", this->settings_.tp_style);
        SaveTextProfile(store, "text.player.name", this->settings_.name_text);
        SaveTextProfile(store, "text.player.jobs", this->settings_.jobs_text);
        SaveTextProfile(store, "text.player.labels", this->settings_.label_text);
        SaveTextProfile(store, "text.player.values", this->settings_.value_text);
        store.WriteRect("layout.player", this->state_.bounds);
    }

    auto player_module::OptionCount(void) const -> size_t { return tp_blink_option + 1; }

    auto player_module::ConfigSections(void) const -> std::span<const module_config_section> { return ui::TimingSections(true); }

    auto player_module::ConfigSectionContains(const size_t section, const size_t index) -> bool
    {
        if (section >= this->ConfigSections().size() || index >= this->OptionCount()) return false;
        if (index == tp_blink_option) return section == static_cast<size_t>(bar_config_section::effects);
        if (index >= timing_options_start) return section == (ui::player_timing_settings::RecoveryOption(index - timing_options_start) ? 8u : 7u);
        const auto topic = static_cast<bar_config_section>(section);
        if (index >= 9 && index < 9 + meter_style_option_count) return topic == MeterConfigSection(index - 9);
        if (index == width_option) return topic == bar_config_section::layout;
        if (index == auto_mp_option) return topic == bar_config_section::general;
        if (index >= 9 + meter_style_option_count || index == 6) return topic == bar_config_section::text;
        if (index == 4 || index == 5)
            return topic == bar_config_section::layout || topic == bar_config_section::bar_style || topic == bar_config_section::animation ||
                topic == bar_config_section::effects || topic == bar_config_section::colors;
        if (index == 7 || index == 8) return topic == bar_config_section::colors;
        return topic == bar_config_section::general;
    }

    auto player_module::Option(const size_t index) -> module_option
    {
        if (index == tp_blink_option)
            return module_option::Toggle("TP ready blink", "Subtle pulse at 1,000 TP or higher. The bar fills in three 1,000-TP layers.", &this->settings_.tp_ready_blink, module_option_group::effects);
        if (index >= timing_options_start) return this->timers_.Option(index - timing_options_start);
        if (index == auto_mp_option)
            return module_option::Toggle("Auto-hide MP", "Hide MP when maximum MP is zero. Empty MP on a caster remains visible.", &this->settings_.hide_zero_mp);
        if (index == width_option)
            return ModuleWidthOption(this->state_);
        static constexpr const char* editModes[] = {"All bars", "Per bar"};
        static constexpr const char* barNames[] = {"HP", "MP", "TP"};
        static constexpr const char* textNames[] = {"Name", "Jobs", "Bar labels", "Bar values", "Attack timing", "Recovery timing"};
        switch (index)
        {
        case 0: return module_option::Toggle("Enabled", "Show the player HUD.", &this->state_.enabled);
        case 1: return module_option::Toggle("Identity", "Show name and jobs.", &this->settings_.show_identity);
        case 2: return module_option::Toggle("Labels", "Show HP, MP, and TP.", &this->settings_.show_labels);
        case 3: return module_option::Toggle("Values", "Show resource values.", &this->settings_.show_values);
        case 4: return module_option::EditScope("Style editing", "Apply to all or one bar.", &this->settings_.style_edit_mode, editModes, 2);
        case 5: return module_option::Element("Selected bar", "Resource colors and style source.", &this->settings_.selected_bar, barNames, 3);
        case 6: return module_option::Element("Selected text", "Text element configured in Text.", &this->settings_.selected_text, textNames, 6);
        case 7: return module_option::Color("Fill top", "Selected resource gradient.", this->SelectedFillTop(), module_option_group::colors);
        case 8: return module_option::Color("Fill bottom", "Selected resource gradient.", this->SelectedFillBottom(), module_option_group::colors);
        default:
            if (index < 9 + meter_style_option_count)
                return MeterStyleOption(index - 9, this->SelectedStyle());
            return TextProfileOption(index - 9 - meter_style_option_count, this->SelectedTextProfile());
        }
    }

    auto player_module::OnOptionChanged(const size_t index) -> void
    {
        if (this->settings_.style_edit_mode == 0 && (index == 4 || (index >= 9 && index < 9 + meter_style_option_count)))
            this->SyncSelectedStyleToAll();
    }

    auto player_module::ShouldRender(const hud_snapshot&) const -> bool
    {
        return this->state_.enabled;
    }

    auto player_module::PreviewChoice(const size_t index, const size_t choice) -> bool
    {
        return index == timing_options_start + 10 && choice > 0 && choice <= std::size(ui::sound_values) && this->sounds_ != nullptr &&
            this->sounds_->Play(ui::sound_values[choice - 1]);
    }

    auto player_module::ShowsMP(const hud_snapshot& snapshot) const -> bool
    {
        return !this->settings_.hide_zero_mp || snapshot.mp_max != 0 || snapshot.mp != 0;
    }

    auto player_module::ResourceRows(const hud_snapshot& snapshot) const -> std::array<ui::rect, 3>
    {
        const auto& panel = this->state_.bounds;
        const float y = panel.y + 4 + (this->settings_.show_identity ? 20 : 0);
        const float height = std::max(14.0f, panel.y + panel.height - y - 5);
        const bool mp = this->ShowsMP(snapshot);
        const float count = mp ? 3.0f : 2.0f, gap = 12;
        const float width = std::max(1.0f, (panel.width - 12 - gap * (count - 1)) / count);
        const float x = panel.x + 6;
        return {{{x, y, width, height}, mp ? ui::rect{x + width + gap, y, width, height} : ui::rect{},
            {x + (width + gap) * (mp ? 2 : 1), y, width, height}}};
    }

    auto player_module::HPMeterBounds(const hud_snapshot& snapshot) const -> ui::rect
    {
        return ui::LayoutMeterTrack(PlayerMeterBounds(this->ResourceRows(snapshot)[0], this->settings_.hp_style), this->settings_.hp_style);
    }

    auto player_module::TPMeterBounds(const hud_snapshot& snapshot) const -> ui::rect
    {
        return ui::LayoutMeterTrack(PlayerMeterBounds(this->ResourceRows(snapshot)[2], this->settings_.tp_style), this->settings_.tp_style);
    }

    auto player_module::AttackTimerBounds(const hud_snapshot& snapshot) const -> ui::rect
    {
        const auto& panel = this->state_.bounds;
        if (this->timers_.attack_layout == 2)
        {
            const auto tp = this->TPMeterBounds(snapshot);
            return {tp.x + this->timers_.offset_x, tp.y + tp.height + this->timers_.mini_gap + this->timers_.offset_y,
                tp.width, this->timers_.mini_height};
        }
        const float height = ui::TimingHeight(this->timers_.attack_text, this->timers_.height, this->timers_.display == 1, true);
        const float y = this->timers_.attack_layout == 1 ? panel.y - height - this->timers_.spacing : panel.y + panel.height + this->timers_.spacing;
        return {panel.x + 6 + this->timers_.offset_x, y + this->timers_.offset_y, panel.width - 12, height};
    }

    auto player_module::RecoveryTimerBounds(const hud_snapshot& snapshot) const -> ui::rect
    {
        const auto& panel = this->state_.bounds;
        if (this->timers_.recovery_layout == 2)
        {
            const auto hp = this->HPMeterBounds(snapshot);
            return {hp.x + this->timers_.offset_x, hp.y + hp.height + this->timers_.recovery_mini_gap + this->timers_.offset_y,
                hp.width, this->timers_.recovery_mini_height};
        }
        const float height = ui::TimingHeight(this->timers_.rest_text, this->timers_.height, this->timers_.display == 1,
            this->timers_.gains || this->timers_.recovery_time);
        const float y = this->timers_.recovery_layout == 1 ? panel.y - height - this->timers_.spacing : panel.y + panel.height + this->timers_.spacing;
        return {panel.x + 6 + this->timers_.offset_x, y + this->timers_.offset_y, panel.width - 12, height};
    }

    auto player_module::Render(const hud_snapshot& snapshot, const module_render_context& context) -> void
    {
        const float tallestBar = std::max({this->settings_.hp_style.height, this->ShowsMP(snapshot) ? this->settings_.mp_style.height : 0, this->settings_.tp_style.height});
        this->state_.minimum_height = std::max(76.0f, tallestBar + (this->settings_.show_identity ? 39.0f : 19.0f));
        this->state_.bounds.height = std::max(this->state_.bounds.height, this->state_.minimum_height);
        const auto& panel = this->state_.bounds;

        float y = panel.y + 4.0f;
        if (this->settings_.show_identity)
        {
            context.DrawText(snapshot.player_name, {panel.x + 7.0f, y, panel.width * 0.55f, 18.0f}, panel, this->settings_.name_text);
            char jobs[64]{};
            std::snprintf(jobs, sizeof(jobs), "%s / %s", snapshot.main_job, snapshot.sub_job);
            context.DrawText(jobs, {panel.x + panel.width * 0.5f, y, panel.width * 0.48f - 7.0f, 18.0f}, panel, this->settings_.jobs_text);
            y += 20.0f;
        }

        const auto rows = this->ResourceRows(snapshot);
        const auto meterBounds = [&](const ui::rect& row, ui::meter_style& style) -> ui::rect {
            if (style.height <= 0.0f)
                style.height = std::clamp(row.height, 14.0f, 30.0f) - 2.0f;
            return PlayerMeterBounds(row, style);
        };
        char hpValue[48]{};
        char mpValue[48]{};
        char tpValue[48]{};
        if (this->settings_.show_values)
        {
            std::snprintf(hpValue, sizeof(hpValue), "%u / %u", snapshot.hp, snapshot.hp_max);
            std::snprintf(mpValue, sizeof(mpValue), "%u / %u", snapshot.mp, snapshot.mp_max);
            std::snprintf(tpValue, sizeof(tpValue), "%u", snapshot.tp);
        }
        context.DrawMeter(meterBounds(rows[0], this->settings_.hp_style), this->settings_.show_labels ? "HP" : "", hpValue, module_render_context::Ratio(snapshot.hp, snapshot.hp_max), this->settings_.hp_color_top, this->settings_.hp_color, this->settings_.hp_style, this->hp_animation_, true, &this->settings_.label_text, &this->settings_.value_text);
        if (this->ShowsMP(snapshot)) context.DrawMeter(meterBounds(rows[1], this->settings_.mp_style), this->settings_.show_labels ? "MP" : "", mpValue, module_render_context::Ratio(snapshot.mp, snapshot.mp_max), this->settings_.mp_color_top, this->settings_.mp_color, this->settings_.mp_style, this->mp_animation_, true, &this->settings_.label_text, &this->settings_.value_text);
        context.DrawMeter(meterBounds(rows[2], this->settings_.tp_style), this->settings_.show_labels ? "TP" : "", tpValue,
            module_render_context::Ratio(snapshot.tp, 3000), this->settings_.tp_color_top, this->settings_.tp_color, this->settings_.tp_style,
            this->tp_animation_, true, &this->settings_.label_text, &this->settings_.value_text,
            ui::meter_text_anchor::automatic, ui::meter_text_anchor::automatic, this->settings_.tp_ready_blink ? meter_fill_mode::tp_blink : meter_fill_mode::tp);

        const double now = ::GetTickCount64() / 1000.0;
        auto attack = this->timing_ != nullptr ? this->timing_->Player(now) : attack_prediction{};
        auto recovery = this->timing_ != nullptr ? this->timing_->Recovery().Predict(now) : attack_prediction{};
        if (context.IsEditing())
        {
            attack = {true, true, false, 0.6f, 1.6f, 4};
            recovery = {true, true, false, 0.4f, 5.4f, 9};
        }
        ui::rect timer{panel.x + 6 + this->timers_.offset_x, panel.y + panel.height + this->timers_.spacing + this->timers_.offset_y, panel.width - 12, 0};
        if (this->timers_.attack && attack.active)
        {
            const auto tp = context.IsEditing() ? tp_prediction{4, 7, 13.6f, 65, false} : this->timing_->TP(this->timers_.Goal(), now);
            const auto bounds = this->AttackTimerBounds(snapshot);
            const auto details = ui::TPDetails(tp, this->timers_.Goal(), this->timers_, this->timers_.attack_layout == 2);
            const bool soon = attack.known && (attack.state.empty() || attack.state == "Ready") && attack.seconds <= this->timers_.ready_seconds;
            const auto text = soon ? TintTextProfile(this->timers_.attack_text, this->timers_.ready_color) : this->timers_.attack_text;
            const float height = this->timers_.attack_layout == 2 ? ui::DrawMiniTiming(attack, details.c_str(), bounds, this->settings_.tp_style,
                text, this->timers_.mini_text, this->timers_.direction == 1, soon ? this->timers_.ready_color : this->timers_.attack_color, context) :
                ui::DrawTiming(attack, "Attack", details.c_str(), bounds, this->settings_.tp_style, this->timers_.attack_text,
                    this->timers_.height, this->timers_.display == 1, this->timers_.direction == 1, this->timers_.attack_color,
                    this->timers_.ready_color, this->timers_.ready_seconds, context);
            timer.y = std::max(timer.y, bounds.y + height + this->timers_.spacing);
        }
        if (this->timers_.resting && recovery.active && (context.IsEditing() || !this->timers_.hide_full || !this->timing_->Recovery().Full()))
        {
            const auto details = context.IsEditing() ? std::string{"Last tick: HP +24  MP +18  Goal ~28s"} : ui::RecoveryDetails(this->timing_->Recovery(), now, this->timers_);
            const auto observed = this->timing_ != nullptr ? this->timing_->Recovery().LastObservedTick() : 0;
            const bool flash = this->timers_.flash && observed > 0 && now - observed < 0.35;
            const auto color = flash ? this->timers_.ready_color : this->timers_.rest_color;
            auto bounds = this->RecoveryTimerBounds(snapshot);
            if (this->timers_.recovery_layout == 0) bounds.y = timer.y;
            else if (this->timers_.recovery_layout == 1 && this->timers_.attack && attack.active && this->timers_.attack_layout == 1)
                bounds.y = this->AttackTimerBounds(snapshot).y - bounds.height - this->timers_.spacing;
            if (this->timers_.recovery_layout == 2)
            {
                const auto text = flash ? TintTextProfile(this->timers_.rest_text, this->timers_.ready_color) : this->timers_.rest_text;
                ui::DrawMiniTiming(recovery, details.c_str(), bounds, this->settings_.hp_style, text, this->timers_.recovery_mini_text,
                    this->timers_.direction == 1, color, context);
            }
            else ui::DrawTiming(recovery, "Resting tick", details.c_str(), bounds, this->settings_.hp_style, this->timers_.rest_text,
                this->timers_.height, this->timers_.display == 1, this->timers_.direction == 1, color, color, 0, context);
        }

    }
} // namespace xiui
