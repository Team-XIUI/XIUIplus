#pragma once

#include "modules/module.hpp"

namespace xiui
{
    class command_menu_module final : public hud_module
    {
    public:
        auto Id(void) const -> const char* override { return "commands"; }
        auto DisplayName(void) const -> const char* override { return "Commands"; }
        auto State(void) -> module_state& override { return this->state_; }
        auto State(void) const -> const module_state& override { return this->state_; }
        auto SupportsScale(void) const -> bool override { return false; }
        auto EditGrip(void) const -> edit_grip_kind override { return edit_grip_kind::none; }
        auto VisitTextSettings(const text_settings_visitor& visit) -> void override
        { VisitTextStyles(visit, {&this->text_.style}); }
        auto ApplyTextDefaults(const ui::theme& theme) -> void override { this->text_.style = theme.meter_text; this->text_.style.scale = 0.72f; }
        auto ResetDefaults(const ui::theme& theme) -> void override;
        auto Load(const settings_store& store) -> void override;
        auto Save(const settings_store& store) const -> void override;
        auto ContentOptionCount(void) const -> size_t override { return 2; }
        auto ContentOption(size_t index) -> module_option override;
        auto ConfigSections(void) const -> std::span<const module_config_section> override;
        auto ContentConfigSectionContains(size_t section, size_t index) -> bool override { return section == 0 && index < this->OptionCount(); }
        auto OnPositionChanged(void) -> void override { this->position_initialized_ = true; }
        auto Update(const native_ui_state& value, float viewportWidth, float viewportHeight) -> void;
        auto Layout(bool visible) const -> native_menu_layout;
        auto AnchorsTop(void) const -> bool { return this->anchor_ == 0; }
        auto ShouldRender(const hud_snapshot&) const -> bool override { return this->state_.enabled && this->native_.menu_visible; }
        auto Render(const hud_snapshot&, const module_render_context& context) -> void override;

    private:
        module_state state_{true, {-1, -1, 112, 168}, 32, 24};
        native_ui_state native_{};
        module_text_profile text_{};
        bool position_initialized_{};
        int anchor_{1};
        float viewport_width_{}, viewport_height_{};
    };
}
