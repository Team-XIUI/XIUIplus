#pragma once

#include "combat/combat_settings.hpp"

#include "../module.hpp"
#include "entities/entity_name_colors.hpp"
#include "combat/combat_source.hpp"
#include "modules/party/party_source.hpp"
#include "ui/combat_timing_display.hpp"
#include "target_of_target_settings.hpp"
#include "target_display_settings.hpp"
#include "target_mob_info.hpp"
#include "world_target_bar.hpp"
#include "target_rings.hpp"
#include "ui/status_strip.hpp"

namespace xiui
{
    class target_module final : public hud_module
    {
    public:
        target_module(void);
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
            ui::status_strip_settings statuses{};
            module_text_profile status_text{ui::theme::CreateDefault().caption_text};
            target_of_target_settings recipient{};
            target_of_target_settings subtarget{};
            target_display_settings display{};
            target_mob_info mob_info{};
            world_target_bar world_bar{};
            target_ring_settings rings{};
        };

        auto SetCombatSettings(combat_settings* value) -> void { this->combat_settings_ = value; }
        auto Id(void) const -> const char* override;
        auto DisplayName(void) const -> const char* override;
        auto State(void) -> module_state& override;
        auto State(void) const -> const module_state& override;
        auto Settings(void) -> settings&;
        auto Settings(void) const -> const settings& { return this->settings_; }
        auto SetHealPrediction(heal_prediction_source* source) -> void { this->settings_.world_bar.healing = source; }
        auto SetTiming(const combat_timing_source* source) -> void { this->timing_ = source; }
        auto ApplyMeterDefaults(const ui::meter_style& style) -> void override;
        auto VisitTextSettings(const text_settings_visitor& visit) -> void override
        { VisitTextStyles(visit, {&this->settings_.name_text.style, &this->settings_.distance_text.style, &this->settings_.label_text.style, &this->settings_.value_text.style, &this->timers_.text.style, &this->settings_.status_text.style, &this->subtarget_status_text_.style, &this->settings_.display.cast_text.style}); this->settings_.mob_info.VisitTextSettings(visit); VisitTextProfiles(visit, this->settings_.world_bar.text); VisitTextProfiles(visit, this->settings_.recipient.text); VisitTextProfiles(visit, this->settings_.subtarget.text); }
        auto ApplyTextDefaults(const ui::theme& theme) -> void override;
        auto ResetDefaults(const ui::theme& themeDefaults) -> void override;
        auto Load(const settings_store& store) -> void override;
        auto Save(const settings_store& store) const -> void override;
        auto ContentOptionCount(void) const -> size_t override;
        auto ContentOption(size_t index) -> module_option override;
        auto ConfigViews(void) const -> std::span<const char* const> override
        {
            static constexpr const char* views[]{"Target", "Target of Target", "Subtarget", "World Target Bar", "Target Rings"};
            return views;
        }
        auto ConfigView(void) const -> size_t override { return this->config_view_; }
        auto ConfigScaleState(void) -> module_state& override { return this->config_view_ == 3 ? this->settings_.world_bar.scale_state : this->config_view_ == 1 ? this->recipient_state_ : this->config_view_ == 2 ? this->subtarget_state_ : this->State(); }
        auto ConfigScaleEditable(void) const -> bool override { return this->config_view_ != 4; }
        auto ConfigScaleVisible(void) const -> bool override { return this->config_view_ != 4; }
        auto SelectConfigView(size_t index) -> void override { this->config_view_ = index <= 4 ? index : 0; }
        auto ConfigSections(void) const -> std::span<const module_config_section> override;
        auto ContentConfigSectionContains(size_t section, size_t index) -> bool override;
        auto OnContentOptionChanged(size_t index) -> void override;
        auto ShouldRender(const hud_snapshot& snapshot) const -> bool override;
        auto Update(IAshitaCore* core) -> void;
        auto UpdateDetails(IAshitaCore* core, const combat_source& combat, const party_source& party, uint64_t now) -> void;
        auto Render(const hud_snapshot& snapshot, const module_render_context& context) -> void override;
        auto SetMobInfoDisplayAllowed(bool allowed) -> void { this->mob_info_display_allowed_ = allowed; }
        auto RenderWorldBar(const hud_snapshot& snapshot, const module_render_context& context) -> void;
        auto BeginRenderFrame(void) -> void { this->status_hits_.clear(); }
        auto RenderStatusTooltip(const module_render_context& context, IResourceManager* resources, float x, float y, bool controller = false) const -> void;
        auto BeginControllerStatus(void) -> bool;
        auto HasStatusDetails(void) const -> bool { return !this->status_hits_.empty(); }
        auto MoveControllerStatus(int direction) -> bool;
        auto EndControllerStatus(void) -> void { this->controller_status_entity_ = 0; this->controller_status_id_ = 0; this->controller_status_occurrence_ = 0; }
        auto ControllerStatusBounds(void) const -> ui::rect;

    private:
#if defined(XIUI_ENABLE_INPUT_TESTS)
        friend struct config_input_test;
#endif
#if defined(XIUI_ENABLE_PARITY_TESTS)
        friend struct module_parity_test;
        friend struct status_display_test;
        friend struct target_children_test;
        friend struct target_display_test;
        friend struct mob_info_test;
#endif
        friend class target_child_module;
        friend class target_mob_info_module;
        friend class target_cast_module;
        struct target_view
        {
            uint32_t id{}, index{}, hp{};
            float distance{};
            D3DCOLOR color{unclaimed_name_color};
            entity_class kind{entity_class::other};
            bool health{}, locked{};
            std::string name{};
            std::array<combat_status, 32> statuses{};
            size_t status_count{};
            combat_cast cast{};
            const mob_record* mob{};
        };
        module_state state_{true, {24.0f, 128.0f, 440.0f, 82.0f}, 300.0f, 70.0f};
        settings settings_{};
        module_state subtarget_state_{true, {24, 218, 440, 82}, 100, 30};
        module_state recipient_state_{true, {528, 128, 160, 72}, 100, 30};
        module_state cast_state_{true, {31, 178, 426, 10}, 60, 4};
        bool cast_positioned_{};
        int subtarget_placement_{};
        bool subtarget_display_allowed_{true}, recipient_display_allowed_{true};
        ui::status_strip_settings subtarget_statuses_{};
        module_text_profile subtarget_status_text_{ui::theme::CreateDefault().caption_text};
        size_t config_view_{};
        ui::enemy_timing_settings timers_{};
        const combat_timing_source* timing_{};
        combat_settings* combat_settings_{};
        static constexpr size_t timing_options_start = 9 + meter_style_option_count + text_profile_option_count;
        static constexpr size_t recipient_options_start = timing_options_start + ui::enemy_timing_settings::option_count;
        static constexpr size_t status_options_start = recipient_options_start + target_of_target_settings::option_count;
        static constexpr size_t subtarget_options_start = status_options_start + ui::status_strip_settings::option_count;
        static constexpr size_t subtarget_option_count = target_of_target_settings::option_count + ui::status_strip_settings::option_count;
        static constexpr size_t display_options_start = subtarget_options_start + subtarget_option_count;
        static constexpr size_t mob_info_options_start = display_options_start + target_display_settings::option_count;
        static constexpr size_t world_options_start = mob_info_options_start + target_mob_info::option_count;
        static constexpr size_t ring_options_start = world_options_start + world_target_bar::option_count;
        meter_animation_state hp_animation_{};
        D3DCOLOR name_color_{unclaimed_name_color};
        bool has_health_{true};
        std::array<target_view, 3> views_{};
        std::array<meter_animation_state, 3> detail_animations_{};
        bool details_ready_{};
        bool mob_info_display_allowed_{true};
        std::vector<ui::status_tooltip> status_hits_{};
        uint32_t controller_status_entity_{};
        uint16_t controller_status_id_{};
        size_t controller_status_occurrence_{};

        auto SelectedTextProfile(void) -> module_text_profile&;
        auto AttachedCastBounds(const ui::rect& panel) const -> ui::rect;
        auto DrawCast(const combat_cast& cast, const ui::rect& bounds, uint64_t now, const module_render_context& context) -> void;
        auto ReadView(IAshitaCore& core, uint32_t index, const combat_source& combat, const party_source& party, target_view& view) -> void;
        auto DrawView(const target_view& view, const ui::rect& area, const char* prefix, meter_animation_state& animation,
            uint64_t now, const module_render_context& context, const target_of_target_settings* presentation = nullptr) -> float;
        auto ChildOptions(bool subtarget) -> target_of_target_settings& { return subtarget ? this->settings_.subtarget : this->settings_.recipient; }
        auto ChildOptions(bool subtarget) const -> const target_of_target_settings& { return subtarget ? this->settings_.subtarget : this->settings_.recipient; }
        auto ChildIndependent(bool subtarget) const -> bool;
        auto ChildVisible(bool subtarget, bool editing) const -> bool;
        auto ChildOption(bool subtarget, size_t index) -> module_option;
        auto ChildContains(bool subtarget, size_t section, size_t index) const -> bool;
        auto RenderChild(bool subtarget, const module_render_context& context) -> void;
        auto ResetChild(bool subtarget) -> void;
    };

    class target_cast_module final : public hud_module
    {
    public:
        explicit target_cast_module(target_module& parent) : parent_{parent} {}
        auto Id(void) const -> const char* override { return "target.cast"; }
        auto DisplayName(void) const -> const char* override { return "Target Cast Bar"; }
        auto State(void) -> module_state& override { return this->parent_.cast_state_; }
        auto State(void) const -> const module_state& override { return this->parent_.cast_state_; }
        auto HasEditableBounds(void) const -> bool override;
        auto PreviewVisible(void) const -> bool override { return this->HasEditableBounds(); }
        auto OnPositionChanged(void) -> void override { this->parent_.cast_positioned_ = true; }
        auto ResetDefaults(const ui::theme&) -> void override {}
        auto ResetSettingsFrom(const hud_module& defaults, const ui::theme&) -> void override;
        auto Load(const settings_store&) -> void override {}
        auto Save(const settings_store&) const -> void override {}
        auto ContentOptionCount(void) const -> size_t override { return 0; }
        auto ContentOption(size_t) -> module_option override { return {}; }
        auto ShouldRender(const hud_snapshot&) const -> bool override;
        auto Render(const hud_snapshot&, const module_render_context& context) -> void override;
    private:
        target_module& parent_;
    };
} // namespace xiui
