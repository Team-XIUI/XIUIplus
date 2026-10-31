#pragma once

#include "modules/module.hpp"
#include "cast_cost_source.hpp"

namespace xiui
{
    class cast_cost_module final : public hud_module
    {
    public:
        auto Id(void) const -> const char* override { return "castcost"; }
        auto DisplayName(void) const -> const char* override { return "Cast Cost"; }
        auto State(void) -> module_state& override { return this->state_; }
        auto State(void) const -> const module_state& override { return this->state_; }
        auto SetSource(cast_cost_source* source) -> void { this->source_ = source; }
        auto ApplyMeterDefaults(const ui::meter_style& style) -> void override;
        auto VisitTextSettings(const text_settings_visitor& visit) -> void override
        { VisitTextProfiles(visit, this->text_); }
        auto ApplyTextDefaults(const ui::theme& theme) -> void override;
        auto ResetDefaults(const ui::theme& theme) -> void override;
        auto Load(const settings_store& store) -> void override;
        auto Save(const settings_store& store) const -> void override;
        auto ContentOptionCount(void) const -> size_t override { return base_options + meter_style_option_count + text_profile_option_count; }
        auto ContentOption(size_t index) -> module_option override;
        auto ConfigSections(void) const -> std::span<const module_config_section> override;
        auto ContentConfigSectionContains(size_t section, size_t index) -> bool override;
        auto ShouldRender(const hud_snapshot&) const -> bool override;
        auto Render(const hud_snapshot&, const module_render_context& context) -> void override;

    private:
        enum option : size_t { enabled, show_name, show_cost, show_timing, show_cooldown, width, line_spacing, container,
            opacity, padding, fill_top, fill_bottom, ready_color, unavailable_color, selected_text, base_options };
        module_state state_{true, {-1, -1, 340, 136}, 180, 40};
        cast_cost_source* source_{};
        bool show_name_{true}, show_cost_{true}, show_timing_{true}, show_cooldown_{true}, container_{true};
        float line_spacing_{3};
        ui::container_style container_style_{0.95f, 5, 10, false};
        ui::meter_style meter_{ui::theme::CreateDefault().meter};
        D3DCOLOR top_{0xFF3798CE}, bottom_{0xFF78C5EE}, ready_{0xFF95D89E}, unavailable_{0xFFFF8C8C};
        std::array<module_text_profile, 5> text_{};
        int selected_text_{};
        meter_animation_state animation_{};
        uint32_t last_id_{UINT32_MAX};
        action_cost_kind last_kind_{};
    };
}
