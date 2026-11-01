#pragma once

#include "modules/module.hpp"
#include "chains_source.hpp"

namespace xiui
{
    class chains_module final : public hud_module
    {
    public:
        auto Id(void) const -> const char* override { return "chains"; }
        auto DisplayName(void) const -> const char* override { return "Chains"; }
        auto State(void) -> module_state& override { return this->state_; }
        auto State(void) const -> const module_state& override { return this->state_; }
        auto SetSources(const skillchain_source* tracker, const chains_source* actions) -> void { this->tracker_ = tracker; this->actions_ = actions; }
        auto ApplyTextDefaults(const ui::theme& theme) -> void override;
        auto VisitTextSettings(const text_settings_visitor& visit) -> void override { VisitTextProfiles(visit, this->text_); }
        auto ResetDefaults(const ui::theme& theme) -> void override;
        auto Load(const settings_store& store) -> void override;
        auto Save(const settings_store& store) const -> void override;
        auto ContentOptionCount(void) const -> size_t override { return base_options + text_profile_option_count; }
        auto ContentOption(size_t index) -> module_option override;
        auto ConfigSections(void) const -> std::span<const module_config_section> override;
        auto ContentConfigSectionContains(size_t section, size_t index) -> bool override;
        auto ShouldRender(const hud_snapshot&) const -> bool override;
        auto Render(const hud_snapshot&, const module_render_context& context) -> void override;

    private:
        enum option : size_t { enabled, weapon, spell, pet, require_buff, avatar, jug, frame, width, direction, max_rows,
            container, opacity, padding, color, burst_elements, selected_text, base_options };
        module_state state_{true, {-1, -1, 420, 240}, 240, 80, 800};
        const skillchain_source* tracker_{};
        const chains_source* actions_{};
        chains_filters filters_{};
        bool container_{true}, color_{true}, burst_elements_{true};
        int direction_{}, selected_text_{};
        float max_rows_{8};
        ui::container_style container_style_{};
        std::array<module_text_profile, 6> text_{};
    };
}
