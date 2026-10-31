#pragma once

#include "equipment_source.hpp"
#include "modules/module.hpp"

namespace xiui
{
    class equipment_module final : public hud_module
    {
    public:
        auto Id(void) const -> const char* override { return "equipment"; }
        auto DisplayName(void) const -> const char* override { return "Equipment"; }
        auto State(void) -> module_state& override { return this->state_; }
        auto State(void) const -> const module_state& override { return this->state_; }
        auto SetSource(const equipment_source* source) -> void { this->source_ = source; }
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
        auto ShouldRender(const hud_snapshot&) const -> bool override { return this->state_.enabled && this->source_ != nullptr && this->source_->Available(); }
        auto Render(const hud_snapshot&, const module_render_context& context) -> void override;
        auto RenderTooltip(const module_render_context& context, float x, float y) const -> void;
        auto SlotBounds(size_t gridIndex) const -> ui::rect;
        auto SlotColumns(void) const -> size_t { return this->Columns(); }

    private:
#if defined(XIUI_ENABLE_INVENTORY_TESTS)
        friend struct inventory_test;
#endif
        enum option : size_t { enabled, heading, ammo, empty_labels, slot_labels, tooltips, icon_size, layout, spacing, inset,
            background, slot_frames, opacity, padding, icon_opacity, selected_text, base_options };
        module_state state_{true, {-1, -1, 206, 226}, 48, 48};
        const equipment_source* source_{};
        bool heading_{true}, ammo_{true}, empty_labels_{true}, slot_labels_{}, tooltips_{true}, background_{true}, slot_frames_{true};
        float icon_size_{32}, spacing_{4}, inset_{2}, icon_opacity_{1};
        int selected_text_{}, layout_{};
        ui::container_style container_{1, 5, 6, false};
        std::array<module_text_profile, 4> text_{};
        auto HeadingHeight(void) const -> float;
        auto LabelHeight(void) const -> float;
        auto CellSize(void) const -> float { return this->icon_size_ + this->inset_ * 2 + 10; }
        auto OuterInset(void) const -> float { return this->background_ ? this->container_.edge_size + this->container_.padding : 0; }
        auto Columns(void) const -> size_t { constexpr size_t columns[]{4, 8, 2}; return columns[this->layout_]; }
    };
}
