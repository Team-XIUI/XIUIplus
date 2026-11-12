#include "localization/localization.hpp"
#include "player_module.hpp"
#include "ui/health_colors.hpp"
#include "modules/bar_config.hpp"
#include "modules/party/party_source.hpp"
#include "modules/castcost/mp_cost_preview.hpp"
#include "ui/xiv1_resource_layout.hpp"

#include <algorithm>
#include <cstdio>
#include <cmath>
#include "ui/sound_choices.hpp"

namespace xiui
{
    auto FormatPlayerResource(const uint32_t current, const uint32_t maximum, const int mode) -> std::string
    {
        const auto percent = maximum == 0 ? 0 : static_cast<uint32_t>(std::min<uint64_t>(100, current * uint64_t{100} / maximum));
        return FormatPartyResource(current, percent, maximum, true, mode);
    }

    auto PlayerHPState(const uint32_t current, const uint32_t maximum, const float warning, const float critical) -> player_hp_state
    {
        if (maximum == 0) return player_hp_state::normal;
        const auto percent = static_cast<double>(current) * 100 / maximum;
        if (percent <= std::min(warning, critical)) return player_hp_state::critical;
        return percent <= std::max(warning, critical) ? player_hp_state::warning : player_hp_state::normal;
    }

    namespace
    {
        auto PlayerMeterBounds(const ui::rect& row, const ui::meter_style& style) -> ui::rect
        {
            const float height = (style.height > 0 ? style.height : 12.0f) + 2;
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
        for (auto* meter : {&this->settings_.hp_style, &this->settings_.mp_style, &this->settings_.tp_style})
            if (meter->height <= 0) meter->height = 12;
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
        this->settings_.mp_label_text = this->settings_.tp_label_text = this->settings_.label_text;
        this->settings_.mp_value_text = this->settings_.tp_value_text = this->settings_.value_text;
    }

    auto player_module::ResetDefaults(const ui::theme& themeDefaults) -> void
    {
        const ui::rect bounds = this->state_.bounds;
        const auto* timing = this->timing_; auto* sounds = this->sounds_;
        const auto* mpPreview = this->mp_preview_;
        auto* healPrediction = this->heal_prediction_;
        const auto* source = this->source_;
        *this = player_module{};
        this->timing_ = timing; this->sounds_ = sounds;
        this->mp_preview_ = mpPreview; this->heal_prediction_ = healPrediction;
        this->source_ = source;
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
        if (this->settings_.selected_text == 6) return this->settings_.mp_label_text;
        if (this->settings_.selected_text == 7) return this->settings_.mp_value_text;
        if (this->settings_.selected_text == 8) return this->settings_.tp_label_text;
        if (this->settings_.selected_text == 9) return this->settings_.tp_value_text;
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

    auto player_module::Load(const settings_store& store) -> void
    {
        this->timers_.Load(store);
        this->state_.enabled = store.ReadBool("player", "enabled", true);
        this->settings_.show_identity = store.ReadBool("player", "show_identity", true);
        this->settings_.show_labels = store.ReadBool("player", "show_labels", true);
        this->settings_.show_values = store.ReadBool("player", "show_values", true);
        this->settings_.hide_zero_mp = store.ReadBool("player", "hide_zero_mp", true);
        this->settings_.tp_ready_blink = store.ReadBool("player", "tp_ready_blink", true);
        this->settings_.mp_cost_preview = store.ReadBool("player", "mp_cost_preview", true);
        this->settings_.heal_preview.Load(store, "player");
        this->settings_.hp_display = std::clamp(store.ReadInt("player", "hp_display", 0), 0, 5);
        this->settings_.mp_display = std::clamp(store.ReadInt("player", "mp_display", 0), 0, 5);
        if (store.ReadInt("player", "number_format_version", 0) < 1)
        {
            if (this->settings_.hp_display == 4) this->settings_.hp_display = 0;
            if (this->settings_.mp_display == 4) this->settings_.mp_display = 0;
        }
        this->settings_.hp_state_colors = store.ReadBool("player", "hp_state_colors", false);
        this->settings_.hp_state_text = store.ReadBool("player", "hp_state_text", false);
        this->settings_.tp_ready_text = store.ReadBool("player", "tp_ready_text", false);
        const auto threshold = [&](const char* key, const float fallback, const float maximum) {
            const auto value = store.ReadFloat("player", key, fallback);
            return std::isfinite(value) ? std::clamp(value, 0.0f, maximum) : fallback;
        };
        this->settings_.hp_warning = threshold("hp_warning", 50, 100);
        this->settings_.hp_critical = threshold("hp_critical", std::min(25.0f, this->settings_.hp_warning), this->settings_.hp_warning);
        this->settings_.hp_warning_top = store.ReadArgb("player", "hp_warning_top", this->settings_.hp_warning_top);
        this->settings_.hp_warning_bottom = store.ReadArgb("player", "hp_warning_bottom", this->settings_.hp_warning_bottom);
        this->settings_.hp_critical_top = store.ReadArgb("player", "hp_critical_top", this->settings_.hp_critical_top);
        this->settings_.hp_critical_bottom = store.ReadArgb("player", "hp_critical_bottom", this->settings_.hp_critical_bottom);
        this->settings_.tp_ready_color = store.ReadArgb("player", "tp_ready_color", this->settings_.tp_ready_color);
        this->settings_.tp_palette.custom_overlay = store.ReadBool("player", "tp_custom_overlay", false);
        this->settings_.tp_palette.overlay_top = store.ReadArgb("player", "tp_overlay_top", this->settings_.tp_palette.overlay_top);
        this->settings_.tp_palette.overlay_bottom = store.ReadArgb("player", "tp_overlay_bottom", this->settings_.tp_palette.overlay_bottom);
        this->settings_.tp_palette.flash = store.ReadArgb("player", "tp_flash_color", 0xFFFFFFFF);
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
        const bool oversizedLegacy = store.ReadInt("player", "bar_height_version", 0) < 2 &&
            this->settings_.hp_style.height == 28 && this->settings_.mp_style.height == 28 && this->settings_.tp_style.height == 28;
        for (auto* meter : {&this->settings_.hp_style, &this->settings_.mp_style, &this->settings_.tp_style})
            if (meter->height <= 0 || oversizedLegacy) meter->height = 12;
        this->settings_.name_text = LoadTextProfile(store, "text.player.name", this->settings_.name_text);
        this->settings_.jobs_text = LoadTextProfile(store, "text.player.jobs", this->settings_.jobs_text);
        this->settings_.label_text = LoadTextProfile(store, "text.player.labels", this->settings_.label_text);
        this->settings_.value_text = LoadTextProfile(store, "text.player.values", this->settings_.value_text);
        if (store.ReadInt("player", "resource_text_version", 0) < 1)
        {
            this->settings_.mp_label_text = this->settings_.tp_label_text = this->settings_.label_text;
            this->settings_.mp_value_text = this->settings_.tp_value_text = this->settings_.value_text;
        }
        else
        {
            this->settings_.mp_label_text = LoadTextProfile(store, "text.player.mp.labels", this->settings_.mp_label_text);
            this->settings_.mp_value_text = LoadTextProfile(store, "text.player.mp.values", this->settings_.mp_value_text);
            this->settings_.tp_label_text = LoadTextProfile(store, "text.player.tp.labels", this->settings_.tp_label_text);
            this->settings_.tp_value_text = LoadTextProfile(store, "text.player.tp.values", this->settings_.tp_value_text);
        }
        const int savedEditMode = store.ReadInt("player", "style_edit_mode", -1);
        const bool hasExistingProfiles = store.ReadInt("meter.player.hp", "style_version", 0) > 0 ||
            store.ReadInt("meter.player.mp", "style_version", 0) > 0 ||
            store.ReadInt("meter.player.tp", "style_version", 0) > 0;
        this->settings_.style_edit_mode = savedEditMode >= 0 ? std::clamp(savedEditMode, 0, 1) : (hasExistingProfiles ? 1 : 0);
        this->settings_.selected_bar = std::clamp(store.ReadInt("player", "selected_bar", 0), 0, 2);
        this->settings_.selected_text = std::clamp(store.ReadInt("player", "selected_text", 0), 0, 9);
        this->hp_animation_ = {};
        this->mp_animation_ = {};
        this->tp_animation_ = {};
        this->state_.bounds = store.ReadRect("layout.player", {24.0f, 24.0f, 640.0f, 88.0f});
    }

    auto player_module::Save(const settings_store& store) const -> void
    {
        this->timers_.Save(store);
        store.WriteBool("player", "enabled", this->state_.enabled);
        store.WriteInt("player", "bar_height_version", 2);
        store.WriteInt("player", "number_format_version", 1);
        store.WriteBool("player", "show_identity", this->settings_.show_identity);
        store.WriteBool("player", "show_labels", this->settings_.show_labels);
        store.WriteBool("player", "show_values", this->settings_.show_values);
        store.WriteBool("player", "hide_zero_mp", this->settings_.hide_zero_mp);
        store.WriteBool("player", "tp_ready_blink", this->settings_.tp_ready_blink);
        store.WriteBool("player", "mp_cost_preview", this->settings_.mp_cost_preview);
        this->settings_.heal_preview.Save(store, "player");
        store.WriteInt("player", "hp_display", this->settings_.hp_display); store.WriteInt("player", "mp_display", this->settings_.mp_display);
        store.WriteBool("player", "hp_state_colors", this->settings_.hp_state_colors); store.WriteBool("player", "hp_state_text", this->settings_.hp_state_text);
        store.WriteFloat("player", "hp_warning", this->settings_.hp_warning); store.WriteFloat("player", "hp_critical", this->settings_.hp_critical);
        store.WriteArgb("player", "hp_warning_top", this->settings_.hp_warning_top); store.WriteArgb("player", "hp_warning_bottom", this->settings_.hp_warning_bottom);
        store.WriteArgb("player", "hp_critical_top", this->settings_.hp_critical_top); store.WriteArgb("player", "hp_critical_bottom", this->settings_.hp_critical_bottom);
        store.WriteBool("player", "tp_ready_text", this->settings_.tp_ready_text); store.WriteArgb("player", "tp_ready_color", this->settings_.tp_ready_color);
        store.WriteBool("player", "tp_custom_overlay", this->settings_.tp_palette.custom_overlay);
        store.WriteArgb("player", "tp_overlay_top", this->settings_.tp_palette.overlay_top); store.WriteArgb("player", "tp_overlay_bottom", this->settings_.tp_palette.overlay_bottom);
        store.WriteArgb("player", "tp_flash_color", this->settings_.tp_palette.flash);
        store.WriteInt("player", "resource_text_version", 1);
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
        SaveTextProfile(store, "text.player.mp.labels", this->settings_.mp_label_text); SaveTextProfile(store, "text.player.mp.values", this->settings_.mp_value_text);
        SaveTextProfile(store, "text.player.tp.labels", this->settings_.tp_label_text); SaveTextProfile(store, "text.player.tp.values", this->settings_.tp_value_text);
        store.WriteRect("layout.player", this->state_.bounds);
    }

    auto player_module::ContentOptionCount(void) const -> size_t { return heal_options_start + heal_preview_settings::option_count; }

    auto player_module::ConfigSections(void) const -> std::span<const module_config_section> { return ui::TimingSections(true); }

    auto player_module::ContentConfigSectionContains(const size_t section, const size_t index) -> bool
    {
        if (section >= this->ConfigSections().size() || index >= this->OptionCount()) return false;
        if (index >= heal_options_start) return section == static_cast<size_t>(heal_preview_settings::ColorOption(index - heal_options_start) ? bar_config_section::colors : bar_config_section::general);
        if (index == mp_preview_option) return section == static_cast<size_t>(bar_config_section::general);
        if (index >= resource_options_start) return section == static_cast<size_t>(index - resource_options_start < 2 ? bar_config_section::general : bar_config_section::colors);
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

    auto player_module::ContentOption(const size_t index) -> module_option
    {
        if (index >= heal_options_start) return this->settings_.heal_preview.Option(index - heal_options_start, this->heal_prediction_);
        if (index == mp_preview_option)
            return module_option::Toggle("Spell MP cost preview", "Highlight the selected spell's base MP cost on your MP bar. Works with Cast Cost hidden.", &this->settings_.mp_cost_preview).Ordered(80);
        if (index >= resource_options_start)
        {
            static constexpr const char* formats[]{"Number", "Percent", "Number (percent)", "Percent (number)", "Current / max", "Hidden"};
            const auto hp = [&](module_option option) { return option.VisibleWhen(this->settings_.hp_state_colors); };
            switch (index - resource_options_start)
            {
            case 0: return module_option::Choice("HP values", "Format actual current and maximum local HP.", &this->settings_.hp_display, formats, 6).VisibleWhen(this->settings_.show_values);
            case 1: return module_option::Choice("MP values", "Format actual current and maximum local MP.", &this->settings_.mp_display, formats, 6).VisibleWhen(this->settings_.show_values);
            case 2: return module_option::Toggle("HP state colors", "Use warning and critical fill gradients below the configured HP thresholds.", &this->settings_.hp_state_colors).Ordered(510);
            case 3: return hp(module_option::Slider("HP warning percent", "Use the warning gradient at or below this percentage.", &this->settings_.hp_warning, this->settings_.hp_critical, 100, 1)).Ordered(511);
            case 4: return hp(module_option::Slider("HP critical percent", "Use the critical gradient at or below this percentage.", &this->settings_.hp_critical, 0, this->settings_.hp_warning, 1)).Ordered(512);
            case 5: return hp(module_option::Toggle("Custom HP text tint", "Override native HP number colors with the configured warning or critical fill color, preserving text effects.", &this->settings_.hp_state_text)).Ordered(513).VisibleWhen(this->settings_.show_values && this->settings_.hp_display != 5);
            case 6: return hp(module_option::Color("HP warning top", "Upper warning gradient.", &this->settings_.hp_warning_top, module_option_group::colors)).Ordered(600, true);
            case 7: return hp(module_option::Color("HP warning bottom", "Lower warning gradient and optional text tint.", &this->settings_.hp_warning_bottom, module_option_group::colors)).Ordered(601);
            case 8: return hp(module_option::Color("HP critical top", "Upper critical gradient.", &this->settings_.hp_critical_top, module_option_group::colors)).Ordered(610, true);
            case 9: return hp(module_option::Color("HP critical bottom", "Lower critical gradient and optional text tint.", &this->settings_.hp_critical_bottom, module_option_group::colors)).Ordered(611);
            case 10: return module_option::Toggle("Tint ready TP text", "Tint the TP value at 1,000 TP or higher, preserving its gradient and effects.", &this->settings_.tp_ready_text).Ordered(680).VisibleWhen(this->settings_.show_values);
            case 11: return module_option::Color("Ready TP text", "Value tint at 1,000 TP or higher.", &this->settings_.tp_ready_color, module_option_group::colors).Ordered(681).VisibleWhen(this->settings_.show_values && this->settings_.tp_ready_text);
            case 12: return module_option::Toggle("Custom TP overlay", "Use a separate gradient for stored TP above 1,000. The third layer retains its distinct tint.", &this->settings_.tp_palette.custom_overlay).Ordered(690);
            case 13: return module_option::Color("TP overlay top", "Upper gradient for stored TP layers.", &this->settings_.tp_palette.overlay_top, module_option_group::colors).Ordered(700, true).VisibleWhen(this->settings_.tp_palette.custom_overlay);
            case 14: return module_option::Color("TP overlay bottom", "Lower gradient for stored TP layers.", &this->settings_.tp_palette.overlay_bottom, module_option_group::colors).Ordered(701).VisibleWhen(this->settings_.tp_palette.custom_overlay);
            default: return module_option::Color("TP flash color", "Ready blink tint; alpha controls its strength.", &this->settings_.tp_palette.flash, module_option_group::colors).Ordered(710).VisibleWhen(this->settings_.tp_ready_blink);
            }
        }
        if (index == tp_blink_option)
            return module_option::Toggle("TP ready blink", "Subtle pulse at 1,000 TP or higher. The bar fills in three 1,000-TP layers.", &this->settings_.tp_ready_blink, module_option_group::effects);
        if (index >= timing_options_start) return this->timers_.Option(index - timing_options_start);
        if (index == auto_mp_option)
            return module_option::Toggle("Auto-hide MP", "Hide MP when maximum MP is zero. Empty MP on a caster remains visible.", &this->settings_.hide_zero_mp);
        if (index == width_option)
            return ModuleWidthOption(this->state_);
        static constexpr const char* editModes[] = {"All bars", "Per bar"};
        static constexpr const char* barNames[] = {"HP", "MP", "TP"};
        static constexpr const char* textNames[] = {"Name", "Jobs", "HP label", "HP values", "Attack timing", "Recovery timing", "MP label", "MP values", "TP label", "TP values"};
        switch (index)
        {
        case 0: return module_option::Toggle("Enabled", "Show the player HUD.", &this->state_.enabled).Ordered(-50);
        case 1: return module_option::Toggle("Identity", "Show name and jobs.", &this->settings_.show_identity);
        case 2: return module_option::Toggle("Labels", "Show HP, MP, and TP.", &this->settings_.show_labels);
        case 3: return module_option::Toggle("Values", "Show resource values.", &this->settings_.show_values);
        case 4: return module_option::EditScope("Style editing", "Choose which bars to edit. Selecting All bars preserves values; edits apply only the changed setting to every bar.", &this->settings_.style_edit_mode, editModes, 2);
        case 5: return module_option::Element("Selected bar", "Resource colors and style source.", &this->settings_.selected_bar, barNames, 3);
        case 6: return module_option::Element("Selected text", "Labels are HP/MP/TP captions; values are their numbers. Each resource has independent text controls.", &this->settings_.selected_text, textNames, 10);
        case 7: return module_option::Color("Fill top", "Selected resource gradient.", this->SelectedFillTop(), module_option_group::colors).Ordered(500, true);
        case 8: return module_option::Color("Fill bottom", "Selected resource gradient.", this->SelectedFillBottom(), module_option_group::colors).Ordered(501);
        default:
            if (index < 9 + meter_style_option_count)
                return MeterStyleOption(index - 9, this->SelectedStyle());
            bool visible = true;
            switch (this->settings_.selected_text)
            {
            case 0: case 1: visible = this->settings_.show_identity; break;
            case 2: case 6: case 8: visible = this->settings_.show_labels; break;
            case 3: visible = this->settings_.show_values && this->settings_.hp_display != 5; break;
            case 7: visible = this->settings_.show_values && this->settings_.mp_display != 5; break;
            case 9: visible = this->settings_.show_values; break;
            case 4: visible = this->timers_.attack && (this->timers_.attack_layout != 2 || this->timers_.mini_text != 0); break;
            case 5: visible = this->timers_.resting && (this->timers_.recovery_layout != 2 || this->timers_.recovery_mini_text != 0); break;
            }
            return TextProfileOption(index - 9 - meter_style_option_count, this->SelectedTextProfile()).VisibleWhen(visible);
        }
    }

    auto player_module::OnContentOptionChanged(const size_t index) -> void
    {
        if (this->settings_.style_edit_mode == 0 && index >= 9 && index < 9 + meter_style_option_count)
            for (auto* style : {&this->settings_.hp_style, &this->settings_.mp_style, &this->settings_.tp_style})
                CopyMeterStyleOption(index - 9, this->SelectedStyle(), *style);
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

    auto player_module::Resources(const hud_snapshot& snapshot) const -> std::array<player_resource, 2>
    {
        const auto resource = [](const uint32_t current, const uint32_t maximum, const uint32_t percent, const bool native) {
            const auto ratio = maximum != 0 ? module_render_context::Ratio(current, maximum) : native ? percent / 100.0f : 0.0f;
            const auto displayedPercent = native ? percent : maximum == 0 ? 0 :
                static_cast<uint32_t>(std::min<uint64_t>(100, current * uint64_t{100} / maximum));
            return player_resource{current, maximum, displayedPercent, ratio};
        };
        if (this->source_ != nullptr && this->source_->Available())
        {
            const auto& local = this->source_->Members()[0];
            if (local.active && local.in_zone && !local.preview)
                return {{resource(local.hp, local.hp_max_exact ? local.hp_max : 0, local.hp_percent, true),
                    resource(local.mp, local.mp_max_exact ? local.mp_max : 0, local.mp_percent, true)}};
        }
        return {{resource(snapshot.hp, snapshot.hp_max, 0, false), resource(snapshot.mp, snapshot.mp_max, 0, false)}};
    }

    auto player_module::ShowsMP(const hud_snapshot& snapshot) const -> bool
    {
        const auto mp = this->Resources(snapshot)[1];
        return !this->settings_.hide_zero_mp || mp.maximum != 0 || mp.current != 0;
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
        if (this->xiv1_theme_) return ui::Xiv1ResourceTrack(this->ResourceRows(snapshot)[0], 0);
        return ui::LayoutMeterTrack(PlayerMeterBounds(this->ResourceRows(snapshot)[0], this->settings_.hp_style), this->settings_.hp_style);
    }

    auto player_module::MPMeterBounds(const hud_snapshot& snapshot) const -> ui::rect
    {
        if (this->xiv1_theme_ && this->ShowsMP(snapshot)) return ui::Xiv1ResourceTrack(this->ResourceRows(snapshot)[1], 1);
        return this->ShowsMP(snapshot) ? ui::LayoutMeterTrack(PlayerMeterBounds(this->ResourceRows(snapshot)[1], this->settings_.mp_style), this->settings_.mp_style) : ui::rect{};
    }

    auto player_module::TPMeterBounds(const hud_snapshot& snapshot) const -> ui::rect
    {
        if (this->xiv1_theme_) return ui::Xiv1ResourceTrack(this->ResourceRows(snapshot)[2], 2);
        return ui::LayoutMeterTrack(PlayerMeterBounds(this->ResourceRows(snapshot)[2], this->settings_.tp_style), this->settings_.tp_style);
    }

    auto player_module::AttackTimerBounds(const hud_snapshot& snapshot) const -> ui::rect
    {
        const auto& panel = this->state_.bounds;
        if (this->timers_.attack_layout == 2)
        {
            const auto tp = this->TPMeterBounds(snapshot);
            return {tp.x + this->timers_.offset_x, tp.y - this->timers_.mini_height - this->timers_.mini_gap + this->timers_.offset_y,
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
            return {hp.x + this->timers_.offset_x, hp.y - this->timers_.recovery_mini_height - this->timers_.recovery_mini_gap + this->timers_.offset_y,
                hp.width, this->timers_.recovery_mini_height};
        }
        const float height = ui::TimingHeight(this->timers_.rest_text, this->timers_.height, this->timers_.display == 1,
            this->timers_.gains || this->timers_.recovery_time);
        const float y = this->timers_.recovery_layout == 1 ? panel.y - height - this->timers_.spacing : panel.y + panel.height + this->timers_.spacing;
        return {panel.x + 6 + this->timers_.offset_x, y + this->timers_.offset_y, panel.width - 12, height};
    }

    auto player_module::Render(const hud_snapshot& snapshot, const module_render_context& context) -> void
    {
        const bool xiv1 = context.Design().IsXiv1Theme();
        this->xiv1_theme_ = xiv1;
        const float tallestBar = std::max({this->settings_.hp_style.height, this->ShowsMP(snapshot) ? this->settings_.mp_style.height : 0, this->settings_.tp_style.height});
        const std::array labels{this->settings_.label_text, this->settings_.mp_label_text, this->settings_.tp_label_text};
        const std::array values{this->settings_.value_text, this->settings_.mp_value_text, this->settings_.tp_value_text};
        const std::array styles{this->settings_.hp_style, this->settings_.mp_style, this->settings_.tp_style};
        const std::array formats{this->settings_.hp_display, this->settings_.mp_display, 0};
        float textSpace = tallestBar;
        for (size_t i = 0; i < styles.size(); ++i)
        {
            if (i == 1 && !this->ShowsMP(snapshot)) continue;
            const auto barHeight = std::max(1.0f, styles[i].height) + 2;
            if (this->settings_.show_labels)
                textSpace = std::max(textSpace, barHeight + context.Design().TextHeight(labels[i].style) + 2 * std::abs(labels[i].offset_y));
            if (this->settings_.show_values && formats[i] != 5)
                textSpace = std::max(textSpace, 2 * (context.Design().TextHeight(values[i].style) + std::abs(values[i].offset_y)) - barHeight);
        }
        this->state_.minimum_height = std::max(76.0f, textSpace + (this->settings_.show_identity ? 39.0f : 19.0f));
        if (!xiv1) this->state_.bounds.height = std::max(this->state_.bounds.height, this->state_.minimum_height);
        const auto& panel = this->state_.bounds;

        float y = panel.y + 4.0f;
        if (this->settings_.show_identity)
        {
            auto nameText = this->settings_.name_text;
            if (xiv1) nameText.style.font = nameText.style.ThemeFont(ui::xiv1_italic_font);
            context.DrawText(snapshot.player_name, {panel.x + 7.0f, y, panel.width * 0.55f, 18.0f}, panel, nameText);
            char jobs[64]{};
            std::snprintf(jobs, sizeof(jobs), "%s / %s", snapshot.main_job, snapshot.sub_job);
            context.DrawText(jobs, {panel.x + panel.width * 0.5f, y, panel.width * 0.48f - 7.0f, 18.0f}, panel, this->settings_.jobs_text);
            y += 20.0f;
        }

        const auto rows = this->ResourceRows(snapshot);
        auto hpStyle = this->settings_.hp_style, mpStyle = this->settings_.mp_style, tpStyle = this->settings_.tp_style;
        if (xiv1)
        {
            hpStyle.height = mpStyle.height = ui::xiv1_resource_height; tpStyle.height = ui::xiv1_tp_height;
            for (auto* style : {&hpStyle, &mpStyle, &tpStyle}) style->bookends = true;
        }
        const auto meterBounds = [&](const ui::rect& row, const ui::meter_style& style) -> ui::rect {
            if (xiv1) return {row.x, row.y, row.width, style.height + 2};
            return PlayerMeterBounds(row, style);
        };
        const auto [hp, mp] = this->Resources(snapshot);
        const auto hpValue = this->settings_.show_values ? FormatPartyResource(hp.current, hp.percent, hp.maximum, true, this->settings_.hp_display) : std::string{};
        const auto mpValue = this->settings_.show_values ? FormatPartyResource(mp.current, mp.percent, mp.maximum, true, this->settings_.mp_display) : std::string{};
        const auto tpValue = this->settings_.show_values ? std::to_string(snapshot.tp) : std::string{};
        auto hpTop = this->settings_.hp_color_top, hpBottom = this->settings_.hp_color;
        auto mpTop = this->settings_.mp_color_top, mpBottom = this->settings_.mp_color;
        if (xiv1) { ui::Xiv1ResourceFill(0, hpTop, hpBottom); ui::Xiv1ResourceFill(1, mpTop, mpBottom); }
        const bool hpKnown = hp.maximum != 0 || (this->source_ != nullptr && this->source_->Available());
        auto hpText = hpKnown ? TintTextProfile(this->settings_.value_text, ui::HPValueColor(hp.percent)) : this->settings_.value_text;
        const auto hpState = hp.maximum != 0 ? PlayerHPState(hp.current, hp.maximum, this->settings_.hp_warning, this->settings_.hp_critical) :
            PlayerHPState(hp.percent, this->source_ != nullptr && this->source_->Available() ? 100 : 0, this->settings_.hp_warning, this->settings_.hp_critical);
        if (this->settings_.hp_state_colors && hpState != player_hp_state::normal)
        {
            hpTop = hpState == player_hp_state::critical ? this->settings_.hp_critical_top : this->settings_.hp_warning_top;
            hpBottom = hpState == player_hp_state::critical ? this->settings_.hp_critical_bottom : this->settings_.hp_warning_bottom;
            if (this->settings_.hp_state_text) hpText = TintTextProfile(this->settings_.value_text, hpBottom);
        }
        const auto tpText = this->settings_.tp_ready_text && snapshot.tp >= 1000 ?
            TintTextProfile(this->settings_.tp_value_text, this->settings_.tp_ready_color) : this->settings_.tp_value_text;

        const double now = ::GetTickCount64() / 1000.0;
        auto attack = this->timing_ != nullptr ? this->timing_->Player(now) : attack_prediction{};
        auto recovery = this->timing_ != nullptr ? this->timing_->Recovery().Predict(now) : attack_prediction{};
        if (context.IsEditing())
        {
            attack = {true, true, false, 0.6f, 1.6f, 4};
            recovery = {true, true, false, 0.4f, 5.4f, 9};
        }
        const bool attackShown = this->timers_.attack && attack.active;
        const bool recoveryShown = this->timers_.resting && recovery.active && (context.IsEditing() || !this->timers_.hide_full || !this->timing_->Recovery().Full());
        ui::rect timer{panel.x + 6 + this->timers_.offset_x, panel.y + panel.height + this->timers_.spacing + this->timers_.offset_y, panel.width - 12, 0};
        const auto drawAttack = [&] {
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
        };
        const auto drawRecovery = [&] {
            const auto details = context.IsEditing() ? std::string{"Last tick: HP +24  MP +18  Goal ~28s"} : ui::RecoveryDetails(this->timing_->Recovery(), now, this->timers_);
            const auto observed = this->timing_ != nullptr ? this->timing_->Recovery().LastObservedTick() : 0;
            const bool flash = this->timers_.flash && observed > 0 && now - observed < 0.35;
            const auto color = flash ? this->timers_.ready_color : this->timers_.rest_color;
            auto bounds = this->RecoveryTimerBounds(snapshot);
            if (this->timers_.recovery_layout == 0) bounds.y = timer.y;
            else if (this->timers_.recovery_layout == 1 && attackShown && this->timers_.attack_layout == 1)
                bounds.y = this->AttackTimerBounds(snapshot).y - bounds.height - this->timers_.spacing;
            if (this->timers_.recovery_layout == 2)
            {
                const auto text = flash ? TintTextProfile(this->timers_.rest_text, this->timers_.ready_color) : this->timers_.rest_text;
                ui::DrawMiniTiming(recovery, details.c_str(), bounds, this->settings_.hp_style, text, this->timers_.recovery_mini_text,
                    this->timers_.direction == 1, color, context);
            }
            else ui::DrawTiming(recovery, "Resting tick", details.c_str(), bounds, this->settings_.hp_style, this->timers_.rest_text,
                this->timers_.height, this->timers_.display == 1, this->timers_.direction == 1, color, color, 0, context);
        };
        // Mini bars sit in the caption space above HP/TP; draw them before the meters so labels stay readable.
        if (attackShown && this->timers_.attack_layout == 2) drawAttack();
        if (recoveryShown && this->timers_.recovery_layout == 2) drawRecovery();

        this->hp_animation_.Bind((uint64_t{snapshot.hp_max} << 32) | (this->source_ != nullptr ? this->source_->Members()[0].id : 0));
        const auto healRanges = this->settings_.heal_preview.Ranges(this->heal_prediction_, this->source_ != nullptr ? this->source_->Members()[0].id : 0, ::GetTickCount64(), hp.ratio);
        context.DrawMeter(meterBounds(rows[0], hpStyle), !xiv1 && this->settings_.show_labels ? i18n::Text("HP") : "", xiv1 ? "" : hpValue.c_str(), hp.ratio, hpTop, hpBottom, hpStyle, this->hp_animation_, true, &this->settings_.label_text, &hpText,
            ui::meter_text_anchor::automatic, ui::meter_text_anchor::automatic, meter_fill_mode::health, {}, healRanges, nullptr, !this->settings_.hp_state_colors);
        if (this->ShowsMP(snapshot))
        {
            std::array<ui::meter_range, 1> ranges{};
            const auto sampledAt = ::GetTickCount64();
            if (this->mp_preview_ != nullptr && this->settings_.mp_cost_preview)
            {
                const auto preview = this->mp_preview_->MPPreview(sampledAt);
                ranges = MPPreviewRanges(preview, mp.current, mp.maximum, preview.player_id, sampledAt, context.Design().GetTheme());
            }
            context.DrawMeter(meterBounds(rows[1], mpStyle), !xiv1 && this->settings_.show_labels ? i18n::Text("MP") : "", xiv1 ? "" : mpValue.c_str(),
                mp.ratio, mpTop, mpBottom, mpStyle,
                this->mp_animation_, true, &this->settings_.mp_label_text, &this->settings_.mp_value_text,
                ui::meter_text_anchor::automatic, ui::meter_text_anchor::automatic, meter_fill_mode::single, {}, ranges);
        }
        context.DrawMeter(meterBounds(rows[2], tpStyle), !xiv1 && this->settings_.show_labels ? i18n::Text("TP") : "", xiv1 ? "" : tpValue.c_str(),
            module_render_context::Ratio(snapshot.tp, 3000), this->settings_.tp_color_top, this->settings_.tp_color, tpStyle,
            this->tp_animation_, true, &this->settings_.tp_label_text, &tpText,
            ui::meter_text_anchor::automatic, ui::meter_text_anchor::automatic, this->settings_.tp_ready_blink ? meter_fill_mode::tp_blink : meter_fill_mode::tp,
            {}, {}, &this->settings_.tp_palette);

        auto footprint = panel;
        const auto include = [&](const ui::rect& at) {
            if (at.width <= 0 || at.height <= 0) return;
            const auto right = std::max(footprint.x + footprint.width, at.x + at.width), bottom = std::max(footprint.y + footprint.height, at.y + at.height);
            footprint.x = std::min(footprint.x, at.x); footprint.y = std::min(footprint.y, at.y);
            footprint.width = right - footprint.x; footprint.height = bottom - footprint.y;
        };
        const std::array captions{i18n::Text("HP"), i18n::Text("MP"), i18n::Text("TP")};
        const std::array numbers{hpValue, mpValue, tpValue};
        for (size_t i = 0; i < styles.size(); ++i)
        {
            if (i == 1 && !this->ShowsMP(snapshot)) continue;
            if (xiv1)
            {
                const auto& style = i == 0 ? hpStyle : i == 1 ? mpStyle : tpStyle;
                const auto track = ui::LayoutMeterTrack(meterBounds(rows[i], style), style);
                auto label = ui::Xiv1ResourceLabel(labels[i]);
                auto value = ui::Xiv1ResourceValue(i == 0 ? hpText : i == 2 ? tpText : values[i],
                    values[0].offset_y);
                const float labelHeight = context.Design().TextHeight(label.style), valueHeight = context.Design().TextHeight(value.style);
                const auto text = ui::LayoutXiv1ResourceText(track, labelHeight, valueHeight);
                if (this->settings_.show_labels) { context.DrawText(captions[i], text.label, text.label, label); include(context.TextBounds(captions[i], text.label, text.label, label)); }
                if (!numbers[i].empty()) { context.DrawText(numbers[i].c_str(), text.value, text.value, value); include(context.TextBounds(numbers[i].c_str(), text.value, text.value, value)); }
                continue;
            }
            const auto track = ui::LayoutMeterTrack(PlayerMeterBounds(rows[i], styles[i]), styles[i]);
            const auto text = ui::LayoutMeterText(track, std::max(16.0f, context.Design().TextHeight(labels[i].style)),
                std::max(16.0f, context.Design().TextHeight(values[i].style)), 32);
            if (this->settings_.show_labels) include(context.TextBounds(captions[i], text.label, text.placement, labels[i]));
            if (!numbers[i].empty()) include(context.TextBounds(numbers[i].c_str(), text.value, text.placement, values[i]));
        }
        this->state_.content_bounds = {footprint.x - panel.x, footprint.y - panel.y, footprint.width, footprint.height};

        if (attackShown && this->timers_.attack_layout != 2) drawAttack();
        if (recoveryShown && this->timers_.recovery_layout != 2) drawRecovery();
    }
} // namespace xiui
