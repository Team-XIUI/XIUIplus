#pragma once

#include "inventory_source.hpp"
#include "modules/module.hpp"

namespace xiui
{
    class inventory_module final : public hud_module
    {
    public:
        explicit inventory_module(size_t group = 0);
        auto Id(void) const -> const char* override { return inventory_groups[this->group_].id; }
        auto DisplayName(void) const -> const char* override { return inventory_groups[this->group_].name; }
        auto State(void) -> module_state& override { return this->state_; }
        auto State(void) const -> const module_state& override { return this->state_; }
        auto SetSource(const inventory_source* source) -> void { this->source_ = source; }
        auto SetModuleEnabled(const bool* enabled) -> void { this->module_enabled_ = enabled; }
        auto PreviewVisible(void) const -> bool override { return this->state_.enabled && (this->module_enabled_ == nullptr || *this->module_enabled_); }
        auto VisitTextSettings(const text_settings_visitor& visit) -> void override
        { VisitTextProfiles(visit, this->text_); }
        auto ApplyTextDefaults(const ui::theme& theme) -> void override;
        auto ResetDefaults(const ui::theme& theme) -> void override;
        auto Load(const settings_store& store) -> void override;
        auto Save(const settings_store& store) const -> void override;
        auto ContentOptionCount(void) const -> size_t override { return base_options + text_profile_option_count; }
        auto ContentOption(size_t index) -> module_option override;
        auto ConfigSections(void) const -> std::span<const module_config_section> override;
        auto ContentConfigSectionContains(size_t section, size_t index) -> bool override;
        auto OnContentOptionChanged(size_t index) -> void override;
        auto ShouldRender(const hud_snapshot&) const -> bool override;
        auto Render(const hud_snapshot&, const module_render_context& context) -> void override;
        auto Describe(void) const -> std::string;

        static auto Fullness(const inventory_usage& usage, int mode, float warning, float critical) -> int;
        static auto DotPosition(uint32_t slot, uint32_t columns, uint32_t rows, uint32_t groupsAcross, float size, float gap, float groupGap) -> ui::point;

    private:
#if defined(XIUI_ENABLE_INVENTORY_TESTS)
        friend struct inventory_test;
#endif
        enum option : size_t { enabled, dots, count, label, split, shown, hide_menu, text_tint, count_mode, threshold_mode, warning, critical,
            columns, rows, dot_size, dot_gap, group_gap, section_gap, text_position, background, opacity, padding,
            normal_color, empty_color, warning_color, critical_color, selected_text, base_options };
        size_t group_{};
        const inventory_source* source_{};
        const bool* module_enabled_{};
        module_state state_{true, {-1, -1, 170, 84}, 24, 16};
        bool dots_{true}, count_{true}, label_{true}, split_{}, text_tint_{}, background_{};
        std::array<bool, 8> shown_{true, true, true, true, true, true, true, true};
        int count_mode_{}, threshold_mode_{}, text_position_{}, selected_text_{};
        float warning_percent_{75}, critical_percent_{95}, warning_slots_{}, critical_slots_{};
        float columns_{5}, rows_{6}, dot_size_{7}, dot_gap_{2}, group_gap_{8}, section_gap_{10};
        ui::container_style container_{1, 5, 8, false};
        D3DCOLOR normal_{}, empty_{}, warning_{0xFFFFB66B}, critical_{0xFFFF647C};
        std::array<module_text_profile, 2> text_{};
        auto Usage(void) const -> inventory_usage;
        auto ShownMask(void) const -> uint32_t;
    };
}
