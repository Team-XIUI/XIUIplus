#pragma once

#include "../module.hpp"
#include "ui/combat_timing_display.hpp"
#include "ui/native_ui_sounds.hpp"

namespace xiui
{
    class player_module final : public hud_module
    {
    public:
        struct settings
        {
            bool show_identity{true};
            bool show_labels{true};
            bool show_values{true};
            bool hide_zero_mp{true};
            bool tp_ready_blink{true};
            D3DCOLOR hp_color{D3DCOLOR_XRGB(255, 121, 125)};
            D3DCOLOR hp_color_top{D3DCOLOR_XRGB(114, 54, 56)};
            D3DCOLOR mp_color{D3DCOLOR_XRGB(191, 224, 125)};
            D3DCOLOR mp_color_top{D3DCOLOR_XRGB(85, 100, 56)};
            D3DCOLOR tp_color{D3DCOLOR_XRGB(120, 196, 238)};
            D3DCOLOR tp_color_top{D3DCOLOR_XRGB(54, 88, 107)};
            ui::meter_style hp_style{ui::theme::CreateDefault().meter};
            ui::meter_style mp_style{ui::theme::CreateDefault().meter};
            ui::meter_style tp_style{ui::theme::CreateDefault().meter};
            module_text_profile name_text{ui::theme::CreateDefault().title_text};
            module_text_profile jobs_text{ui::theme::CreateDefault().caption_text};
            module_text_profile label_text{ui::theme::CreateDefault().meter_text};
            module_text_profile value_text{ui::theme::CreateDefault().meter_text};
            int style_edit_mode{};
            int selected_bar{};
            int selected_text{};
        };

        auto Id(void) const -> const char* override;
        auto DisplayName(void) const -> const char* override;
        auto State(void) -> module_state& override;
        auto State(void) const -> const module_state& override;
        auto Settings(void) -> settings&;
        auto ApplyMeterDefaults(const ui::meter_style& style) -> void override;
        auto VisitTextSettings(const text_settings_visitor& visit) -> void override
        { VisitTextStyles(visit, {&this->settings_.name_text.style, &this->settings_.jobs_text.style, &this->settings_.label_text.style, &this->settings_.value_text.style, &this->timers_.attack_text.style, &this->timers_.rest_text.style}); }
        auto ApplyTextDefaults(const ui::theme& theme) -> void override;
        auto ResetDefaults(const ui::theme& themeDefaults) -> void override;
        auto Load(const settings_store& store) -> void override;
        auto Save(const settings_store& store) const -> void override;
        auto OptionCount(void) const -> size_t override;
        auto Option(size_t index) -> module_option override;
        auto ConfigSections(void) const -> std::span<const module_config_section> override;
        auto ConfigSectionContains(size_t section, size_t index) -> bool override;
        auto OnOptionChanged(size_t index) -> void override;
        auto PreviewChoice(size_t index, size_t choice) -> bool override;
        auto SetTiming(const combat_timing_source* source) -> void { this->timing_ = source; }
        auto SetSounds(ui::native_ui_sounds* sounds) -> void { this->sounds_ = sounds; }
        auto TimingSettings(void) -> ui::player_timing_settings& { return this->timers_; }
        auto ShouldRender(const hud_snapshot& snapshot) const -> bool override;
        auto ShowsMP(const hud_snapshot& snapshot) const -> bool;
        auto ResourceRows(const hud_snapshot& snapshot) const -> std::array<ui::rect, 3>;
        auto HPMeterBounds(const hud_snapshot& snapshot) const -> ui::rect;
        auto TPMeterBounds(const hud_snapshot& snapshot) const -> ui::rect;
        auto AttackTimerBounds(const hud_snapshot& snapshot) const -> ui::rect;
        auto RecoveryTimerBounds(const hud_snapshot& snapshot) const -> ui::rect;
        auto Render(const hud_snapshot& snapshot, const module_render_context& context) -> void override;

    private:
        static constexpr size_t width_option = 9 + meter_style_option_count + text_profile_option_count;
        static constexpr size_t auto_mp_option = width_option + 1;
        static constexpr size_t timing_options_start = auto_mp_option + 1;
        static constexpr size_t tp_blink_option = timing_options_start + ui::player_timing_settings::option_count;
        ui::player_timing_settings timers_{};
        const combat_timing_source* timing_{};
        ui::native_ui_sounds* sounds_{};
        module_state state_{true, {24.0f, 24.0f, 640.0f, 88.0f}, 430.0f, 76.0f};
        settings settings_{};
        meter_animation_state hp_animation_{};
        meter_animation_state mp_animation_{};
        meter_animation_state tp_animation_{};

        auto SelectedFillTop(void) -> D3DCOLOR*;
        auto SelectedFillBottom(void) -> D3DCOLOR*;
        auto SelectedStyle(void) -> ui::meter_style&;
        auto SelectedTextProfile(void) -> module_text_profile&;
        auto SyncSelectedStyleToAll(void) -> void;
    };
} // namespace xiui
