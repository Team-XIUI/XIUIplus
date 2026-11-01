#pragma once

#include "../module.hpp"
#include "modules/healing/heal_prediction.hpp"
#include "ui/combat_timing_display.hpp"
#include "ui/native_ui_sounds.hpp"
#include "ui/tp_meter.hpp"

namespace xiui
{
    class cast_cost_source;
    class party_source;
    struct player_resource
    {
        uint32_t current{}, maximum{}, percent{};
        float ratio{};
    };
    enum class player_hp_state : uint8_t { normal, warning, critical };
    auto FormatPlayerResource(uint32_t current, uint32_t maximum, int mode) -> std::string;
    auto PlayerHPState(uint32_t current, uint32_t maximum, float warning, float critical) -> player_hp_state;

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
            bool mp_cost_preview{true};
            heal_preview_settings heal_preview{};
            int hp_display{}, mp_display{};
            bool hp_state_colors{}, hp_state_text{}, tp_ready_text{};
            float hp_warning{50}, hp_critical{25};
            D3DCOLOR hp_warning_top{0xFF806030}, hp_warning_bottom{0xFFFFCA78};
            D3DCOLOR hp_critical_top{0xFF802028}, hp_critical_bottom{0xFFFF5966};
            D3DCOLOR tp_ready_color{0xFFFFD98A};
            ui::tp_meter_colors tp_palette{};
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
            module_text_profile mp_label_text{ui::theme::CreateDefault().meter_text};
            module_text_profile mp_value_text{ui::theme::CreateDefault().meter_text};
            module_text_profile tp_label_text{ui::theme::CreateDefault().meter_text};
            module_text_profile tp_value_text{ui::theme::CreateDefault().meter_text};
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
        { VisitTextStyles(visit, {&this->settings_.name_text.style, &this->settings_.jobs_text.style, &this->settings_.label_text.style, &this->settings_.value_text.style,
            &this->settings_.mp_label_text.style, &this->settings_.mp_value_text.style, &this->settings_.tp_label_text.style, &this->settings_.tp_value_text.style,
            &this->timers_.attack_text.style, &this->timers_.rest_text.style}); }
        auto ApplyTextDefaults(const ui::theme& theme) -> void override;
        auto ResetDefaults(const ui::theme& themeDefaults) -> void override;
        auto Load(const settings_store& store) -> void override;
        auto Save(const settings_store& store) const -> void override;
        auto ContentOptionCount(void) const -> size_t override;
        auto ContentOption(size_t index) -> module_option override;
        auto ConfigSections(void) const -> std::span<const module_config_section> override;
        auto ContentConfigSectionContains(size_t section, size_t index) -> bool override;
        auto OnContentOptionChanged(size_t index) -> void override;
        auto PreviewChoice(size_t index, size_t choice) -> bool override;
        auto SetTiming(const combat_timing_source* source) -> void { this->timing_ = source; }
        auto SetSounds(ui::native_ui_sounds* sounds) -> void { this->sounds_ = sounds; }
        auto SetHealPrediction(heal_prediction_source* source) -> void { this->heal_prediction_ = source; }
        auto SetMPPreview(const cast_cost_source* source) -> void { this->mp_preview_ = source; }
        auto SetSource(const party_source* source) -> void { this->source_ = source; }
        auto Resources(const hud_snapshot& snapshot) const -> std::array<player_resource, 2>;
        auto TimingSettings(void) -> ui::player_timing_settings& { return this->timers_; }
        auto ShouldRender(const hud_snapshot& snapshot) const -> bool override;
        auto ShowsMP(const hud_snapshot& snapshot) const -> bool;
        auto ResourceRows(const hud_snapshot& snapshot) const -> std::array<ui::rect, 3>;
        auto HPMeterBounds(const hud_snapshot& snapshot) const -> ui::rect;
        auto MPMeterBounds(const hud_snapshot& snapshot) const -> ui::rect;
        auto TPMeterBounds(const hud_snapshot& snapshot) const -> ui::rect;
        auto AttackTimerBounds(const hud_snapshot& snapshot) const -> ui::rect;
        auto RecoveryTimerBounds(const hud_snapshot& snapshot) const -> ui::rect;
        auto Render(const hud_snapshot& snapshot, const module_render_context& context) -> void override;

    private:
#if defined(XIUI_ENABLE_INPUT_TESTS)
        friend struct config_input_test;
#endif
        static constexpr size_t width_option = 9 + meter_style_option_count + text_profile_option_count;
        static constexpr size_t auto_mp_option = width_option + 1;
        static constexpr size_t timing_options_start = auto_mp_option + 1;
        static constexpr size_t tp_blink_option = timing_options_start + ui::player_timing_settings::option_count;
        static constexpr size_t resource_options_start = tp_blink_option + 1;
        static constexpr size_t resource_option_count = 16;
        static constexpr size_t mp_preview_option = resource_options_start + resource_option_count;
        static constexpr size_t heal_options_start = mp_preview_option + 1;
        heal_prediction_source* heal_prediction_{};
        ui::player_timing_settings timers_{};
        const combat_timing_source* timing_{};
        ui::native_ui_sounds* sounds_{};
        const cast_cost_source* mp_preview_{};
        const party_source* source_{};
        module_state state_{true, {24.0f, 24.0f, 640.0f, 88.0f}, 430.0f, 76.0f};
        settings settings_{};
        meter_animation_state hp_animation_{};
        meter_animation_state mp_animation_{};
        meter_animation_state tp_animation_{};
        bool xiv1_theme_{};

        auto SelectedFillTop(void) -> D3DCOLOR*;
        auto SelectedFillBottom(void) -> D3DCOLOR*;
        auto SelectedStyle(void) -> ui::meter_style&;
        auto SelectedTextProfile(void) -> module_text_profile&;
    };
} // namespace xiui
