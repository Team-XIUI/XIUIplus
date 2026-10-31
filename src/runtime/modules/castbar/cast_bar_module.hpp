#pragma once

#include "modules/module.hpp"
#include "player_cast_source.hpp"
#include "cast_adjustments.hpp"

namespace xiui
{
    class cast_bar_module final : public hud_module
    {
    public:
        auto Id(void) const -> const char* override { return "castbar"; }
        auto DisplayName(void) const -> const char* override { return "Cast Bar"; }
        auto State(void) -> module_state& override { return this->state_; }
        auto State(void) const -> const module_state& override { return this->state_; }
        auto SetSource(player_cast_source* source) -> void { this->source_ = source; }
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
        enum option : size_t { enabled, show_name, show_percent, show_items, show_interrupted, width, fill_top, fill_bottom,
            interrupted_color, selected_text, fastcast_enabled, fastcast_job, fastcast_amount, fastcast_rdm_sub, fastcast_cure, fastcast_singing, fastcast_summoning, base_options };
        module_state state_{true, {-1, -1, 440, 52}, 160, 32};
        player_cast_source* source_{};
        bool show_name_{true}, show_percent_{true}, show_items_{true}, show_interrupted_{true};
        D3DCOLOR top_{0xFF3798CE}, bottom_{0xFF78C5EE}, interrupted_{0xFFFF8C8C};
        ui::meter_style meter_{ui::theme::CreateDefault().meter};
        std::array<module_text_profile, 2> text_{};
        int selected_text_{}, selected_job_{};
        cast_adjustments fastcast_{};
        meter_animation_state animation_{};
        uint64_t cast_started_{};
    };
}
