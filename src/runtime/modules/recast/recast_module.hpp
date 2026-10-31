#pragma once

#include "modules/module.hpp"
#include "recast_source.hpp"

namespace xiui
{
    class recast_module final : public hud_module
    {
    public:
        auto Id(void) const -> const char* override { return "recast"; }
        auto DisplayName(void) const -> const char* override { return "Recast"; }
        auto State(void) -> module_state& override { return this->state_; }
        auto State(void) const -> const module_state& override { return this->state_; }
        auto SetSource(recast_source* source) -> void { this->source_ = source; }
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
        auto Colors(const recast_entry& entry, uint64_t now) const -> std::array<D3DCOLOR, 2>;

    private:
        enum option : size_t { enabled, display, abilities, spells, icons, heading, timer, charges, tenths, tint_text, background,
            width, maximum_rows, row_height, spacing, icon_size, anchor, sorting, fill_direction, ready_hold,
            color_mode, soon_seconds, near_seconds, cooldown_top, cooldown_bottom, soon_top, soon_bottom, ready_top, ready_bottom,
            selected_text, base_options };
        module_state state_{true, {-1, -1, 300, 128}, 160, 24};
        recast_source* source_{};
        bool abilities_{true}, spells_{true}, icons_{true}, heading_{}, timer_{true}, charges_{true}, tenths_{true}, tint_text_{}, background_{};
        int display_{}, anchor_{}, sorting_{}, fill_direction_{}, color_mode_{}, selected_text_{};
        float maximum_rows_{6}, row_height_{28}, spacing_{4}, icon_size_{28}, ready_hold_{2}, soon_seconds_{20}, near_seconds_{5};
        std::array<D3DCOLOR, 2> cooldown_{0xFF844E4E, 0xFFD29393}, soon_{0xFF8D7139, 0xFFE9CA7A}, ready_{0xFF467B5C, 0xFF95E6B4};
        ui::meter_style meter_{ui::theme::CreateDefault().meter};
        std::array<module_text_profile, 3> text_{};

        auto Visible(const recast_entry& entry, uint64_t now) const -> bool;
        auto DrawRow(const recast_entry& entry, const ui::rect& row, uint64_t now, const module_render_context& context) const -> void;
    };
}
