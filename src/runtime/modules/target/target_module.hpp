#pragma once

#include "../module.hpp"
#include "entities/entity_name_colors.hpp"
#include "combat/combat_source.hpp"
#include "modules/party/party_source.hpp"
#include "ui/combat_timing_display.hpp"
#include "target_of_target_settings.hpp"

namespace xiui
{
    class target_module final : public hud_module
    {
    public:
        struct settings
        {
            bool hide_when_empty{false};
            bool show_distance{true};
            bool show_percent{true};
            bool show_label{true};
            D3DCOLOR hp_color{D3DCOLOR_XRGB(251, 148, 148)};
            D3DCOLOR hp_color_top{D3DCOLOR_XRGB(112, 66, 66)};
            ui::meter_style hp_style{ui::theme::CreateDefault().meter};
            module_text_profile name_text{ui::theme::CreateDefault().title_text};
            module_text_profile distance_text{ui::theme::CreateDefault().caption_text};
            module_text_profile label_text{ui::theme::CreateDefault().meter_text};
            module_text_profile value_text{ui::theme::CreateDefault().meter_text};
            int selected_text{};
            target_of_target_settings recipient{};
        };

        auto Id(void) const -> const char* override;
        auto DisplayName(void) const -> const char* override;
        auto State(void) -> module_state& override;
        auto State(void) const -> const module_state& override;
        auto Settings(void) -> settings&;
        auto SetTiming(const combat_timing_source* source) -> void { this->timing_ = source; }
        auto ApplyMeterDefaults(const ui::meter_style& style) -> void override;
        auto VisitTextSettings(const text_settings_visitor& visit) -> void override
        { VisitTextStyles(visit, {&this->settings_.name_text.style, &this->settings_.distance_text.style, &this->settings_.label_text.style, &this->settings_.value_text.style, &this->timers_.text.style}); VisitTextProfiles(visit, this->settings_.recipient.text); }
        auto ApplyTextDefaults(const ui::theme& theme) -> void override;
        auto ResetDefaults(const ui::theme& themeDefaults) -> void override;
        auto Load(const settings_store& store) -> void override;
        auto Save(const settings_store& store) const -> void override;
        auto OptionCount(void) const -> size_t override;
        auto Option(size_t index) -> module_option override;
        auto ConfigViews(void) const -> std::span<const char* const> override
        {
            static constexpr const char* views[]{"Target", "Target of Target"};
            return views;
        }
        auto ConfigView(void) const -> size_t override { return this->config_view_; }
        auto SelectConfigView(size_t index) -> void override { this->config_view_ = index == 1 ? 1 : 0; }
        auto ConfigSections(void) const -> std::span<const module_config_section> override;
        auto ConfigSectionContains(size_t section, size_t index) -> bool override;
        auto ShouldRender(const hud_snapshot& snapshot) const -> bool override;
        auto Update(IAshitaCore* core) -> void;
        auto UpdateDetails(IAshitaCore* core, const combat_source& combat, const party_source& party, uint64_t now) -> void;
        auto Render(const hud_snapshot& snapshot, const module_render_context& context) -> void override;

    private:
#if defined(XIUI_ENABLE_PARITY_TESTS)
        friend struct module_parity_test;
#endif
        struct target_view
        {
            uint32_t id{}, index{}, hp{};
            float distance{};
            D3DCOLOR color{unclaimed_name_color};
            bool health{}, locked{};
            std::string name{};
            std::array<combat_status, 32> statuses{};
            size_t status_count{};
            combat_cast cast{};
        };
        module_state state_{true, {24.0f, 128.0f, 440.0f, 82.0f}, 300.0f, 70.0f};
        settings settings_{};
        size_t config_view_{};
        ui::enemy_timing_settings timers_{};
        const combat_timing_source* timing_{};
        static constexpr size_t timing_options_start = 9 + meter_style_option_count + text_profile_option_count;
        static constexpr size_t recipient_options_start = timing_options_start + ui::enemy_timing_settings::option_count;
        meter_animation_state hp_animation_{};
        D3DCOLOR name_color_{unclaimed_name_color};
        bool has_health_{true};
        std::array<target_view, 3> views_{};
        std::array<meter_animation_state, 3> detail_animations_{};
        bool details_ready_{};

        auto SelectedTextProfile(void) -> module_text_profile&;
        auto ReadView(IAshitaCore& core, uint32_t index, const combat_source& combat, const party_source& party, target_view& view) -> void;
        auto DrawView(const target_view& view, const ui::rect& area, const char* prefix, meter_animation_state& animation,
            uint64_t now, const module_render_context& context, bool recipient = false) -> float;
    };
} // namespace xiui
