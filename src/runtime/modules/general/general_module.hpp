#pragma once

#include "../module.hpp"
#include "config/attributions.hpp"
#include "config/config_text.hpp"
#include "config/global_text.hpp"
#include "config/controller_focus_settings.hpp"
#include "config/module_visibility.hpp"
#include "config/health_feedback.hpp"
#include "config/hud_presets.hpp"
#include "ui/layout_grid.hpp"
#include "controller_input.hpp"

#include <optional>
#include <algorithm>
#include <vector>

namespace xiui
{
    class general_module final : public hud_module
    {
    public:
        auto Id(void) const -> const char* override { return "general"; }
        auto DisplayName(void) const -> const char* override { return "Global"; }
        auto State(void) -> module_state& override { return this->state_; }
        auto State(void) const -> const module_state& override { return this->state_; }
        auto HasEditableBounds(void) const -> bool override { return false; }
        auto SupportsScale(void) const -> bool override { return false; }
        auto ResetDefaults(const ui::theme&) -> void override;
        auto Load(const settings_store& store) -> void override;
        auto Save(const settings_store& store) const -> void override;
        static constexpr size_t window_theme_option = 0;
        static constexpr size_t dark_backing_opacity_option = 1;
        static constexpr size_t enabled_modules_option = 2;
        static constexpr size_t appearance_section = 0;
        static constexpr size_t enabled_modules_section = 1;
        static constexpr size_t attribution_section = 2;
        static constexpr size_t grid_section = 3;
        static constexpr size_t input_section = 4;
        static constexpr size_t visibility_section = 5;
        static constexpr size_t game_hud_section = 6;
        static constexpr size_t extra_settings_section = 7;
        static constexpr size_t grid_option_count = 6;
        static constexpr size_t input_option_count = 7;
        inline static constexpr const char* input_modes[]{"Auto", "Keyboard & Mouse", "Controller"};
        inline static constexpr const char* appearance_tabs[]{"Window", "Global Text", "Config Settings", "Controller Focus", "HP Feedback"};
        auto SetModules(std::span<hud_module* const> modules) -> void { this->modules_.assign(modules.begin(), modules.end()); }
        auto SetTextModules(std::span<hud_module* const> modules) -> void { this->text_modules_.assign(modules.begin(), modules.end()); }
        auto SetVisibilityModules(std::span<hud_module* const> modules) -> void { this->visibility_.SetModules(modules); }
        auto Visibility(void) -> module_visibility_settings& { return this->visibility_; }
        auto Visibility(void) const -> const module_visibility_settings& { return this->visibility_; }
        auto SetInventoryModule(hud_module& module) -> void { this->inventory_module_ = &module; }
        auto InventoryEnabled(void) const -> bool { return this->inventory_enabled_; }
        auto InventoryEnabledFlag(void) const -> const bool* { return &this->inventory_enabled_; }
        auto SetPartyModule(hud_module& module) -> void { this->party_module_ = &module; }
        auto SetGameHudModule(hud_module& module) -> void { this->game_hud_module_ = &module; }
        auto PartyEnabled(void) const -> bool { return this->party_enabled_; }
        auto SetPartyEnabled(bool enabled) -> void { this->party_enabled_ = enabled; }
        auto PartyEnabledFlag(void) const -> const bool* { return &this->party_enabled_; }
        static auto Enabled(hud_module& module) -> bool;
        auto AttributionStart(void) const -> size_t { return enabled_modules_option + this->modules_.size(); }
        auto GridStart(void) const -> size_t { return this->AttributionStart() + ui::attributions.size(); }
        auto ContributorsStart(void) const -> size_t { return this->GridStart() + grid_option_count; }
        auto AshitaStart(void) const -> size_t { return this->ContributorsStart() + ui::xiui_contributors.size(); }
        auto GlobalTextStart(void) const -> size_t { return this->AshitaStart() + ui::ashita_credits.size(); }
        auto ConfigTextStart(void) const -> size_t { return this->GlobalTextStart() + ui::global_text_settings::option_count; }
        auto InputStart(void) const -> size_t { return this->ConfigTextStart() + ui::config_text_settings::option_count; }
        auto FocusStart(void) const -> size_t { return this->InputStart() + input_option_count; }
        auto VisibilityStart(void) const -> size_t { return this->FocusStart() + ui::controller_focus_settings::option_count; }
        auto IconThemesStart(void) const -> size_t { return this->VisibilityStart() + module_visibility_settings::option_count; }
        auto HideAllUiOption(void) const -> size_t { return this->IconThemesStart() + 2; }
        auto FeedbackStart(void) const -> size_t { return this->HideAllUiOption() + 1; }
        auto NameplatesOption(void) const -> size_t { return this->FeedbackStart() + health_feedback_settings::option_count; }
        auto GameHudStart(void) const -> size_t { return this->NameplatesOption() + 1; }
        auto TakeNameplatesRequest(void) -> std::optional<bool> { const auto request = this->nameplates_request_; this->nameplates_request_.reset(); return request; }
        auto HealthFeedback(void) const -> const health_feedback_settings& { return this->health_feedback_; }
        auto StatusIconTheme(void) const -> int { return this->status_icon_theme_; }
        auto JobIconTheme(void) const -> int { return this->job_icon_theme_; }
        auto FocusStyle(void) const -> const ui::focus_frame_style& { return this->controller_focus_.style; }
        auto InputMode(void) const -> int { return this->input_mode_; }
        auto SetInputMode(int mode) -> void { this->input_mode_ = std::clamp(mode, 0, 2); }
        auto SetControllerStatus(const controller_status& status) -> void { this->controller_status_ = status; }
        auto ControllerStatus(void) const -> const controller_status& { return this->controller_status_; }
        auto AppearanceTab(void) const -> size_t { return this->appearance_tab_; }
        auto SelectAppearanceTab(size_t index) -> void { this->appearance_tab_ = std::min(index, std::size(appearance_tabs) - 1); }
        auto GlobalTextSize(void) const -> float { return this->global_text_.style.scale; }
        auto GlobalFont(void) const -> int { return this->global_text_.style.font; }
        auto GlobalTextStyle(void) const -> const ui::text_style& { return this->global_text_.style; }
        auto ConfigText(void) const -> const ui::config_text_settings& { return this->config_text_; }
        auto AppearanceOption(size_t index) const -> bool
        {
            if (index == this->MenuScaleOption() || index == this->ExtraSettingsStart()) return this->appearance_tab_ == 2;
            if (this->appearance_tab_ == 4) return index >= this->FeedbackStart() && index < this->NameplatesOption();
            if (this->appearance_tab_ == 3) return index >= this->FocusStart() && index < this->VisibilityStart();
            if (this->appearance_tab_ == 2) return index >= this->ConfigTextStart() && index < this->InputStart();
            if (index >= this->HudScaleStart()) return this->appearance_tab_ == 0;
            return this->appearance_tab_ == 1 ? index >= this->GlobalTextStart() && index < this->ConfigTextStart() : index < enabled_modules_option || (index >= this->IconThemesStart() && index < this->HideAllUiOption());
        }
        auto AttributionView(void) const -> size_t { return this->attribution_view_; }
        auto SelectAttributionView(size_t index) -> void { this->attribution_view_ = index < std::size(ui::attribution_views) ? index : 0; }
        auto AttributionOption(size_t index) const -> bool
        {
            if (this->attribution_view_ == 2) return index >= this->AshitaStart() && index < this->GlobalTextStart();
            return this->attribution_view_ == 1 ? index >= this->ContributorsStart() && index < this->AshitaStart() :
                index >= this->AttributionStart() && index < this->GridStart();
        }
        auto Grid(void) const -> const ui::layout_grid_settings& { return this->grid_; }
        auto SetClipboardOwner(HWND owner) -> void { this->clipboard_owner_ = owner; }
        auto HudScaleStart(void) const -> size_t { return this->GameHudStart() + (this->game_hud_module_ != nullptr ? this->game_hud_module_->OptionCount() : 0); }
        auto ExtraSettingsStart(void) const -> size_t { return this->HudScaleStart() + 6; }
        auto Xiv1BackgroundOpacityOption(void) const -> size_t { return this->ExtraSettingsStart() + 1; }
        auto FollowGameWindowTypeOption(void) const -> size_t { return this->ExtraSettingsStart() + 2; }
        auto MenuScaleOption(void) const -> size_t { return this->ExtraSettingsStart() + 3; }
        auto MenuScale(void) const -> float { return 1.0f + .25f * std::clamp(this->menu_scale_, 0, 4); }
        auto ContentOptionCount(void) const -> size_t override { return this->ExtraSettingsStart() + 4; }
        auto FollowMovedModuleSettings(void) const -> bool { return this->follow_moved_module_settings_; }
        auto HudPresetOption(size_t preset) const -> size_t
        {
            constexpr std::array<size_t, 3> offsets{2, 3, 5};
            return preset < offsets.size() ? this->HudScaleStart() + offsets[preset] : this->ContentOptionCount();
        }
        auto HudPresetFromOption(size_t index) const -> std::optional<size_t>
        {
            for (size_t preset = 0; preset < ui::hud_presets.size(); ++preset)
                if (index == this->HudPresetOption(preset)) return preset;
            return std::nullopt;
        }
        auto HudScale(void) const -> float { return this->hud_scale_; }
        auto TakePresetRequest(void) -> std::optional<size_t> { auto value = this->preset_request_; this->preset_request_.reset(); return value; }
        auto ContentOption(size_t index) -> module_option override;
        auto ConfigSections(void) const -> std::span<const module_config_section> override;
        auto ContentConfigSectionContains(size_t section, size_t index) -> bool override;
        auto OnContentOptionChanged(size_t index) -> void override;
        auto ShouldRender(const hud_snapshot&) const -> bool override { return false; }
        auto Render(const hud_snapshot&, const module_render_context&) -> void override {}
        auto UpdateWindowType(std::optional<uint8_t> type) -> void;
        auto RequestWindowType(int type) -> bool;
        auto TakeWindowTypeRequest(void) -> std::optional<uint8_t>;
        auto WindowType(void) const -> uint8_t { return static_cast<uint8_t>(std::clamp(this->window_type_, 0, 7)); }
        auto FollowGameWindowType(void) const -> bool { return this->follow_game_window_type_; }
        auto DarkBackingOpacity(void) const -> float { return this->dark_backing_opacity_; }
        auto Xiv1BackgroundOpacity(void) const -> float { return this->xiv1_background_opacity_; }
        auto AppearanceThemeOption(void) const -> size_t { return this->HudScaleStart() + 4; }
        auto AppearanceTheme(void) const -> int { return this->appearance_theme_; }
        auto SetAppearanceTheme(int theme) -> void { this->appearance_theme_ = ui::AppearanceTheme(theme); }

    private:
#if defined(XIUI_ENABLE_INPUT_TESTS)
        friend struct config_input_test;
#endif
        static auto OpenLink(HWND owner, const char* url) -> bool;
        bool (*open_link_)(HWND, const char*){OpenLink};
        size_t attribution_view_{};
        size_t appearance_tab_{};
        ui::global_text_settings global_text_{};
        ui::config_text_settings config_text_{};
        ui::controller_focus_settings controller_focus_{};
        module_visibility_settings visibility_{};
        health_feedback_settings health_feedback_{};
        bool hide_nameplates_{};
        std::optional<bool> nameplates_request_{};
        module_state state_{};
        std::vector<hud_module*> modules_{};
        std::vector<hud_module*> text_modules_{};
        hud_module* inventory_module_{};
        bool inventory_enabled_{true};
        hud_module* party_module_{};
        hud_module* game_hud_module_{};
        bool party_enabled_{true};
        HWND clipboard_owner_{};
        size_t copied_credit_{static_cast<size_t>(-1)};
        int window_type_{}, custom_window_type_{-1}, requested_window_type_{-1};
        bool window_type_available_{};
        bool follow_game_window_type_{true};
        float dark_backing_opacity_{1.0f};
        float xiv1_background_opacity_{.4f};
        int appearance_theme_{};
        float hud_scale_{1};
        int menu_scale_{};
        bool follow_moved_module_settings_{true};
        std::optional<size_t> preset_request_{};
        ui::layout_grid_settings grid_{};
        int input_mode_{};
        int status_icon_theme_{}, job_icon_theme_{};
        controller_status controller_status_{};
    };
} // namespace xiui
