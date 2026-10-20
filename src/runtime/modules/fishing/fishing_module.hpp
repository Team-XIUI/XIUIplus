#pragma once

#include "modules/module.hpp"

namespace xiui
{
    class fishing_module final : public hud_module
    {
    public:
        auto Id(void) const -> const char* override { return "fishing"; }
        auto DisplayName(void) const -> const char* override { return "Fishing"; }
        auto State(void) -> module_state& override { return this->state_; }
        auto State(void) const -> const module_state& override { return this->state_; }
        auto ApplyMeterDefaults(const ui::meter_style& style) -> void override;
        auto ApplyTextDefaults(const ui::theme& theme) -> void override;
        auto VisitTextSettings(const text_settings_visitor& visit) -> void override { VisitTextProfiles(visit, this->text_); }
        auto ResetDefaults(const ui::theme& theme) -> void override;
        auto Load(const settings_store& store) -> void override;
        auto Save(const settings_store& store) const -> void override;
        auto OptionCount(void) const -> size_t override { return base_options + meter_style_option_count + text_profile_option_count; }
        auto Option(size_t index) -> module_option override;
        auto ConfigSections(void) const -> std::span<const module_config_section> override;
        auto ConfigSectionContains(size_t section, size_t index) -> bool override;
        auto ShouldRender(const hud_snapshot& snapshot) const -> bool override;
        auto Render(const hud_snapshot& snapshot, const module_render_context& context) -> void override;

    private:
        enum option : size_t { enabled, show_name, show_percent, width, fill_top, fill_bottom, selected_text, base_options };
        module_state state_{true, {-1, -1, 360, 52}, 160, 32};
        bool show_name_{true}, show_percent_{true};
        D3DCOLOR top_{0xFF9D6733}, bottom_{0xFFE6BA68};
        ui::meter_style meter_{ui::theme::CreateDefault().meter};
        std::array<module_text_profile, 2> text_{};
        int selected_text_{};
        meter_animation_state animation_{};
    };
}
