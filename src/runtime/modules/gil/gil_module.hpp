#pragma once

#include "gil_source.hpp"
#include "modules/module.hpp"

namespace xiui
{
    auto FormatGil(int64_t amount, bool sign = false) -> std::string;

    class gil_module final : public hud_module
    {
    public:
        auto Id(void) const -> const char* override { return "gil"; }
        auto DisplayName(void) const -> const char* override { return "Gil"; }
        auto State(void) -> module_state& override { return this->state_; }
        auto State(void) const -> const module_state& override { return this->state_; }
        auto SetSource(gil_source* source) -> void { this->source_ = source; }
        auto VisitTextSettings(const text_settings_visitor& visit) -> void override
        { VisitTextProfiles(visit, this->text_); }
        auto ApplyTextDefaults(const ui::theme& theme) -> void override;
        auto ResetDefaults(const ui::theme& theme) -> void override;
        auto Load(const settings_store& store) -> void override;
        auto Save(const settings_store& store) const -> void override;
        auto OptionCount(void) const -> size_t override { return base_options + text_profile_option_count; }
        auto Option(size_t index) -> module_option override;
        auto OnOptionChanged(size_t index) -> void override;
        auto ConfigSections(void) const -> std::span<const module_config_section> override;
        auto ConfigSectionContains(size_t section, size_t index) -> bool override;
        auto ShouldRender(const hud_snapshot&) const -> bool override;
        auto Render(const hud_snapshot&, const module_render_context& context) -> void override;

    private:
#if defined(XIUI_ENABLE_INVENTORY_TESTS)
        friend struct inventory_test;
#endif
        enum option : size_t { enabled, hide_menu, tracking, mode, reset, icon, icon_right, right_align, icon_size, spacing,
            background, opacity, padding, positive, negative, selected_text, base_options };
        module_state state_{true, {-1, -1, 140, 42}, 24, 18};
        gil_source* source_{};
        bool hide_menu_{}, tracking_{true}, icon_{true}, icon_right_{true}, right_align_{}, background_{};
        int mode_{}, selected_text_{};
        float icon_size_{24}, spacing_{2};
        ui::container_style container_{1, 5, 6, false};
        D3DCOLOR positive_{0xFF95D89E}, negative_{0xFFFF8C8C};
        std::array<module_text_profile, 2> text_{};
        uint32_t cached_balance_{UINT32_MAX};
        int64_t cached_net_{INT64_MIN};
        int cached_mode_{-1};
        uint64_t next_rate_{};
        std::string balance_text_{}, tracking_text_{};
        auto UpdateText(uint64_t now, bool preview) -> void;
    };
}
