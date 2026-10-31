#pragma once

#include "modules/module.hpp"
#include "combat/skillchain_source.hpp"

namespace xiui
{
    class magic_burst_module final : public hud_module
    {
    public:
        auto Id(void) const -> const char* override { return "magicburst"; }
        auto DisplayName(void) const -> const char* override { return "Magic Burst"; }
        auto State(void) -> module_state& override { return this->state_; }
        auto State(void) const -> const module_state& override { return this->state_; }
        auto SetSource(const skillchain_source* source) -> void { this->source_ = source; }
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
        enum option : size_t { enabled, size, opacity, artwork, pulse, fade_in, timer, chain_name, elements, element_tint, selected_text, base_options };
        module_state state_{true, {-1, -1, 128, 150}, 64, 64, 384};
        const skillchain_source* source_{};
        bool artwork_{true}, pulse_{true}, fade_in_{true}, timer_{true}, chain_name_{true}, elements_{}, element_tint_{true};
        int selected_text_{};
        float opacity_{1};
        std::array<module_text_profile, 3> text_{};
    };
}
